#pragma once

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <vector>

class SwapChain {

      public:
	SwapChain() = default;
	~SwapChain() = default;

	void init(VulkanContext* context, QWindow* window, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	void cleanup();
	void recreate(QWindow* window);

	void createFrameBuffers(VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView);

	VkSwapchainKHR getSwapChain() const { return m_swapChain; }
    VkFormat getImageFormat() const { return m_imageFormat; }
    VkExtent2D getExtent() const { return m_extent; }
	const std::vector<VkImage>& getImages() const { return m_images; }
    const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }
	const std::vector<VkFramebuffer>& getFramebuffers() const { return m_frameBuffers; }
    size_t getImageCount() const { return m_images.size(); }

      private:
	void create(QWindow* window);
	void createImageViews();

	void cleanupFramebuffers();

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(QWindow* window, const VkSurfaceCapabilitiesKHR& capabilities);

	VulkanContext* m_context = nullptr;

	VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_imageViews;
	std::vector<VkFramebuffer> m_frameBuffers;
	VkFormat m_imageFormat;
	VkExtent2D m_extent;
	VkImageUsageFlags m_imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
};
