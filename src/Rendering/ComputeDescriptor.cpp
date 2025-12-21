#include <VulkanApp/Rendering/ComputeDescriptor.h>

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Utils/Uniforms.h>

#include <array>
#include <stdexcept>


void ComputeDescriptor::init(VulkanContext* context, const uint32_t max_frames_in_flight,
							  const std::vector<Buffer>& camUniformBuffers,
							  const std::vector<Buffer>& sphereBuffers,
							  const std::vector<Buffer>& lightBuffers,
							  const std::vector<Image>& storageImagesBuffers){
	m_context = context;
	createSetLayout();
	createPool(max_frames_in_flight);
	createSets(max_frames_in_flight, camUniformBuffers, sphereBuffers, lightBuffers, storageImagesBuffers);
};


void ComputeDescriptor::cleanup() noexcept{
	if (m_context && m_context->getDevice() != VK_NULL_HANDLE) {
		VkDevice device = m_context->getDevice();
		if (m_pool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, m_pool, nullptr);
			m_pool = VK_NULL_HANDLE;
		}
		if (m_setLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device, m_setLayout, nullptr);
			m_setLayout = VK_NULL_HANDLE;
		}
		m_sets.clear();
	}
};

/// @brief Creates set layout for mvp matrix in vertax stage and 2d sampler for textures in fragment stage
void ComputeDescriptor::createSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding{}; // camera data
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding spheresSSBOLayoutBinding{}; // spheres
	spheresSSBOLayoutBinding.binding = 1;
	spheresSSBOLayoutBinding.descriptorCount = 1;
	spheresSSBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	spheresSSBOLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	spheresSSBOLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding imageLayoutBinding{}; // image written storage
	imageLayoutBinding.binding = 2;
	imageLayoutBinding.descriptorCount = 1;
	imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	imageLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsLayoutBinding{}; // image written storage
	lightsLayoutBinding.binding = 3;
	lightsLayoutBinding.descriptorCount = 1;
	lightsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lightsLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {uboLayoutBinding, spheresSSBOLayoutBinding, imageLayoutBinding, lightsLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_context->getDevice(), &layoutInfo, nullptr, &m_setLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}


void ComputeDescriptor::createPool(const uint32_t max_frames_in_flight) {
	std::array<VkDescriptorPoolSize, 4> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[3].descriptorCount = static_cast<uint32_t>(max_frames_in_flight);


	VkDescriptorPoolCreateInfo infoPool{};
	infoPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	infoPool.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	infoPool.pPoolSizes = poolSizes.data();
	infoPool.maxSets = static_cast<uint32_t>(max_frames_in_flight);

	if (vkCreateDescriptorPool(m_context->getDevice(), &infoPool, nullptr, &m_pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}



void ComputeDescriptor::createSets( const uint32_t max_frames_in_flight, 
							  const std::vector<Buffer>& camUniformBuffers,
							  const std::vector<Buffer>& sphereBuffers,
							  const std::vector<Buffer>& lightBuffers,
							  const std::vector<Image>& storageImagesBuffers) { 

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
		VkDescriptorBufferInfo camUniformBufferInfo{};
		camUniformBufferInfo.buffer = camUniformBuffers[i].get();
		camUniformBufferInfo.offset = 0;
		camUniformBufferInfo.range = camUniformBuffers[i].getSize();

		VkDescriptorBufferInfo spheresBufferInfo{};
		spheresBufferInfo.buffer = sphereBuffers[i].get();
		spheresBufferInfo.offset = 0;
		spheresBufferInfo.range = sphereBuffers[i].getSize();

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = storageImagesBuffers[i].getView();
		imageInfo.sampler = VK_NULL_HANDLE; // sampler is ingored by vulkan

		VkDescriptorBufferInfo lightsBufferInfo{};
		lightsBufferInfo.buffer = lightBuffers[i].get();
		lightsBufferInfo.offset = 0;
		lightsBufferInfo.range = lightBuffers[i].getSize();

		std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &camUniformBufferInfo; 
		
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &spheresBufferInfo;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = m_sets[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &imageInfo;

		descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[3].dstSet = m_sets[i];
		descriptorWrites[3].dstBinding = 3;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pBufferInfo = &lightsBufferInfo;

		vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

