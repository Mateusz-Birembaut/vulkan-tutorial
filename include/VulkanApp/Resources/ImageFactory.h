#pragma once

#include <VulkanApp/Resources/Image.h>

#include "stb_image.h"

Image createColorAttachment(VulkanContext* context, const SwapChain& swapchain) {
	Image img;
	auto extent = swapchain.getExtent();
	img.init(context, extent.width, extent.height, swapchain.getImageFormat(), 1, context->getMsaaSamples(),
			 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 false, false);
	return img;
}

Image createDepthAttachment(VulkanContext* context, const SwapChain& swapchain, const RenderPass& renderPass) {
	Image img;
	auto format = renderPass.findDepthFormat();
	auto extent = swapchain.getExtent();
	img.init(context, extent.width, extent.height, renderPass.findDepthFormat(), 1, context->getMsaaSamples(),
			 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			 VK_IMAGE_ASPECT_DEPTH_BIT,
			 false, false);
	return img;
}

Image createTextureFromFile(VulkanContext* context, CommandManager* cmdManager, const std::string& path, bool createSampler, bool genMipMap) {
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