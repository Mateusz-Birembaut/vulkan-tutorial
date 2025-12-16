#pragma once 

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Core/SwapChain.h>

#include <vulkan/vulkan.h>

class RenderPass
{

public:
	RenderPass() = default;
	~RenderPass() = default;

	void init(VulkanContext* context, SwapChain* swapchain);
	void cleanup() noexcept;

	VkRenderPass get() { return m_renderPass;};

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();

private:

	VulkanContext* m_context = nullptr;
	SwapChain* m_swapchain = nullptr;

	VkRenderPass m_renderPass = VK_NULL_HANDLE;

	void createRenderPass();
};




