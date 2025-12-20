#pragma once

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <string>

const std::string compute_shader = "Shaders/comp.spv";

class ComputePipeline {

public:
	ComputePipeline() = default;
	~ComputePipeline() = default;

	void init(VulkanContext* context, VkDescriptorSetLayout descriptorSetLayout);
	void cleanup();

	VkPipeline get() const { return m_pipeline; }
	VkPipelineLayout getLayout() const { return m_layout; }
	VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
	VulkanContext* getContext() const { return m_context; }

private:
	VulkanContext* m_context = nullptr;

	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE; 	

	void createComputePipeline();
};

