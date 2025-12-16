#pragma once

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <string>

const std::string g_vertex_shader = "Shaders/vert.spv";
const std::string g_fragment_shader = "Shaders/frag.spv";

class Pipeline
{

public:
	Pipeline() = default;
	~Pipeline() = default;

	void init(VulkanContext* context, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout);
	void cleanup();

	VkPipeline get() const { return m_pipeline; }
	VkPipelineLayout getLayout() const { return m_layout; }
	VkRenderPass getRenderPass() const { return m_renderPass; }
	VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
	VulkanContext* getContext() const { return m_context; }

private:

	VulkanContext* m_context = nullptr;

	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE; 	

	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createGraphicsPipeline();
};
