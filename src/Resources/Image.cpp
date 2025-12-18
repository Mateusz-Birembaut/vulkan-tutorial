#include <VulkanApp/Resources/Image.h>

#include <VulkanApp/Resources/Buffer.h>

#include "stb_image.h"

#include <set>
#include <iostream>
#include <cmath>


Image Image::createColorAttachment(VulkanContext* context, const SwapChain& swapchain) {
	Image img;
	auto extent = swapchain.getExtent();
	img.init(context, extent.width, extent.height, swapchain.getImageFormat(), 1, context->getMsaaSamples(),
			 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 false, false);
	return img;
}

Image Image::createDepthAttachment(VulkanContext* context, const SwapChain& swapchain, const RenderPass& renderPass) {
	Image img;
	auto format = renderPass.findDepthFormat();
	auto extent = swapchain.getExtent();
	img.init(context, extent.width, extent.height, renderPass.findDepthFormat(), 1, context->getMsaaSamples(),
			 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			 VK_IMAGE_ASPECT_DEPTH_BIT,
			 false, false);
	return img;
}

Image Image::createTextureFromFile(VulkanContext* context, CommandManager* cmdManager, const std::string& path, bool createSampler, bool genMipMap) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		throw std::runtime_error(std::string("failed to load texture file: ") + path);
	}

	VkDeviceSize imgSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;

	uint32_t mipLevels = 1;
	if (genMipMap) {
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight))));
	}

	Buffer staging;
	staging.init(context, cmdManager, imgSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	staging.map();
	staging.write(pixels, imgSize, 0);
	staging.unmap();


	Image img;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (genMipMap) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	img.init(context, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, mipLevels, VK_SAMPLE_COUNT_1_BIT, usage, VK_IMAGE_ASPECT_COLOR_BIT, createSampler, genMipMap);

	// transition, copy, generate mipmaps (or transition to shader read)
	img.transitionImageLayout(cmdManager, img.m_image, VK_FORMAT_R8G8B8A8_SRGB, mipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	img.copyBufferToImage(cmdManager, staging.get(), img.m_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	if (genMipMap) {
		img.generateMipmaps(cmdManager, img.m_image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);
	} else {
		img.transitionImageLayout(cmdManager, img.m_image, VK_FORMAT_R8G8B8A8_SRGB, mipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	staging.cleanup();
	stbi_image_free(pixels);
	return img;
}


void Image::cleanup() noexcept{

	if(m_context){
		auto device = m_context->getDevice();

		if(m_sampler != VK_NULL_HANDLE) vkDestroySampler(device, m_sampler, nullptr);
		if(m_view != VK_NULL_HANDLE) vkDestroyImageView(device, m_view, nullptr);
		if(m_image != VK_NULL_HANDLE) vkDestroyImage(device, m_image, nullptr);
		if(m_memory != VK_NULL_HANDLE) vkFreeMemory(device, m_memory, nullptr);
	}
}

void Image::init(VulkanContext* context, uint32_t w, uint32_t h, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool hasSampler, bool generateMipmap) {
    m_context = context;
    m_mipLevels = mipLevels;

	VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	auto indices = m_context->getQueueFamilies();
	if (indices.transferFamily.has_value() && indices.transferFamily.value() != indices.graphicsFamily.value()) {
		sharingMode = VK_SHARING_MODE_CONCURRENT;
	}

	createImage(w, h, format, mipLevels, samples, sharingMode, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	createImageView(m_image, format, mipLevels, aspect);
	if(hasSampler) createSampler();
}


void Image::transitionImageLayout(CommandManager* cmdManager, VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandPool pool = cmdManager->getTransferCommandPool();
	VkQueue queue = m_context->getTransferQueue();
	VkCommandBuffer commandBuffer = cmdManager->beginSingleTimeCommands(pool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		throw std::invalid_argument("unsupported layout transition in Image::transitionImageLayout");
	}

	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	cmdManager->endSingleTimeCommands(commandBuffer, pool, queue);
}

void Image::copyBufferToImage(CommandManager* cmdManager, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandPool pool = cmdManager->getTransferCommandPool();
	VkQueue queue = m_context->getTransferQueue();
	VkCommandBuffer commandBuffer = cmdManager->beginSingleTimeCommands(pool);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0,0,0};
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	cmdManager->endSingleTimeCommands(commandBuffer, pool, queue);
}

void Image::createImage(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels,
			    VkSampleCountFlagBits numSamples, VkSharingMode sharingMode,
			    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.depth = 1;
	imageInfo.extent.height = height;
	imageInfo.extent.width = width;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = sharingMode;
	imageInfo.samples = numSamples;
	//imageInfo.flags = 0; // Optional, see sparse 3D (ex: clouds)

	auto indices = m_context->getQueueFamilies();
	std::set<uint32_t> uniqueIndices{indices.transferFamily.value(), indices.graphicsFamily.value()};
	std::vector<uint32_t> queueIndices(uniqueIndices.begin(), uniqueIndices.end());
	if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
		imageInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueIndices.size());
		imageInfo.pQueueFamilyIndices = queueIndices.data(); //
	}

	if (vkCreateImage(m_context->getDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_context->getDevice(), m_image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(m_context->getDevice(), m_image, m_memory, 0);
}



void Image::createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

}


void Image::createSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR; 
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.anisotropyEnable = VK_TRUE;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_context->getPhysicalDevice(), &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; 

	samplerInfo.unnormalizedCoordinates = VK_FALSE; 

	samplerInfo.compareEnable = VK_FALSE; 
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	if (vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &m_sampler)) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}


void Image::generateMipmaps(CommandManager* cmdManager, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
	// on regarde si le format support le linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_context->getPhysicalDevice(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandPool pool = cmdManager->getCommandPool();
	VkQueue queue = m_context->getGraphicsQueue();
	VkCommandBuffer commandBuffer = cmdManager->beginSingleTimeCommands(pool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				     0, nullptr,
				     0, nullptr,
				     1, &barrier); // permet d'attendre que le transfert avec copy ou la mipmap soit fini avant de continuer et de pouvoir écrire

		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		// on va prendre la taille de la texture du miplevel precendent i-1

		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {
		    mipWidth > 1 ? mipWidth / 2 : 1,
		    mipHeight > 1 ? mipHeight / 2 : 1,
		    1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		// on va "écrire" le miplevel i en fonction des données du miplevel precedent

		vkCmdBlitImage(commandBuffer, // queue a besoin de la queue graphics
			       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &blit,
			       VK_FILTER_LINEAR); // interpolation

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, // attend que blit termine avant de sample
				     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				     0, nullptr,
				     0, nullptr,
				     1, &barrier);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	// on transitionne en mode shader read le dernier mipLevel
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
			     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			     0, nullptr,
			     0, nullptr,
			     1, &barrier);

	cmdManager->endSingleTimeCommands(commandBuffer, pool, queue);
}
