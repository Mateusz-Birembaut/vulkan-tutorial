#include <VulkanApp/Rendering/Descriptors.h>

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Utils/Uniforms.h>

#include <array>
#include <stdexcept>


void Descriptors::init(VulkanContext* context, const uint32_t max_frames_in_flight, const std::vector<Buffer>& uniformBuffers, VkImageView textureImageView, VkSampler textureSampler){
	m_context = context;
	createSetLayout();
	createPool(max_frames_in_flight);
	createSets(max_frames_in_flight, uniformBuffers, textureImageView, textureSampler);
};

void Descriptors::cleanup() noexcept{
	if (m_context && m_context->getDevice() != VK_NULL_HANDLE) {
		VkDevice device = m_context->getDevice();
		if (m_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_pool, nullptr);
		if (m_setLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_setLayout, nullptr);
	}
};

/// @brief Creates set layout for mvp matrix in vertax stage and 2d sampler for textures in fragment stage
void Descriptors::createSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_context->getDevice(), &layoutInfo, nullptr, &m_setLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Descriptors::createPool(const uint32_t max_frames_in_flight) {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);

	VkDescriptorPoolCreateInfo infoPool{};
	infoPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	infoPool.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	infoPool.pPoolSizes = poolSizes.data();
	infoPool.maxSets = static_cast<uint32_t>(max_frames_in_flight);

	if (vkCreateDescriptorPool(m_context->getDevice(), &infoPool, nullptr, &m_pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}


void Descriptors::createSets( const uint32_t max_frames_in_flight, 
							  const std::vector<Buffer>& uniformBuffers,
							  VkImageView textureImageView,
							  VkSampler textureSampler) { 

	std::vector<VkDescriptorSetLayout> layouts(max_frames_in_flight, m_setLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_pool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(max_frames_in_flight);
	allocInfo.pSetLayouts = layouts.data();

	m_sets.resize(max_frames_in_flight);

	if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, m_sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor sets!");
	}

	for (int i{0}; i < max_frames_in_flight; ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i].get();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo; 
		
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}