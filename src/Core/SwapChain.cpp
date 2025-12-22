#include <VulkanApp/Core/SwapChain.h>


#include <vulkan/vulkan.h>

#include <QWindow>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <array>

/// @brief creates the swapchain and the image views
/// @param context
/// @param window
void SwapChain::init(VulkanContext* context, QWindow* window, VkImageUsageFlags usage) {
	m_context = context;
	m_imageUsage = usage;
	create(window);
	createImageViews();
}

/// @brief Recreates the swapchain and its image views and frame buffers, recreation of frame buffers not called here
/// @param window
void SwapChain::recreate(QWindow* window) {
	create(window);
	createImageViews();
}

/// @brief Destroyes the frame buffers, the image views then the swapchain
void SwapChain::cleanup() {
	if(m_context){
		VkDevice device = m_context->getDevice();

		cleanupFramebuffers();

		for (auto imageView : m_imageViews) {
			if(imageView != VK_NULL_HANDLE) vkDestroyImageView(device, imageView, nullptr);
		}
		std::vector<VkImageView>().swap(m_imageViews);

		if(m_swapChain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, m_swapChain, nullptr);

		m_swapChain = VK_NULL_HANDLE;
	}

}

void SwapChain::cleanupFramebuffers() {
    for (auto framebuffer : m_frameBuffers) {
		if(framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(m_context->getDevice(), framebuffer, nullptr);
    }
	std::vector<VkFramebuffer>().swap(m_frameBuffers);
}

/// @brief Gives a color format for the swapchain images
/// @param availableFormats
/// @return the format where RGBA8 and Non linear khr are supported or the 1st available format
VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}
	return availableFormats[0];
}

/// @brief Gives a Present mode for the swapchain
/// @param availablePresentModes
/// @return  Mailbox present mode or fifo if not supported
VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	// le type de queue qu'on va utiliser pour stocker nos images a afficher a l'écran
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			// triple buffering, si le buffer est plein, on remplace les images dans le buffer, compromi, réduit quand meme la latence sans tearing
			return availablePresentMode;
	}
	return VK_PRESENT_MODE_FIFO_KHR; // le seul mode supporté par défaut
					 // ca attend si le buffer d'images est plein (l'écran trop lent par exemple)
}

/// @brief Uses the size of the window to give a resolution for the swapchain images,
/// @param window
/// @param capabilities
/// @return A clamped width and height based on the max and min resolution supported
VkExtent2D SwapChain::chooseSwapExtent(QWindow* window, const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {

		int width = window->width() * window->devicePixelRatio();
        int height = window->height() * window->devicePixelRatio();

		VkExtent2D actualExtent{
		    static_cast<uint32_t>(width),
		    static_cast<uint32_t>(height),
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}


void SwapChain::create(QWindow* window) {

	SwapChainSupportDetails swapChainSupport{m_context->getSwapChainSupport()};

	if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
		throw std::runtime_error("No suitable swap chain formats or present modes found.");
	}

	VkSurfaceFormatKHR surfaceFormat{chooseSwapSurfaceFormat(swapChainSupport.formats)};
	VkPresentModeKHR presentMode{chooseSwapPresentMode(swapChainSupport.presentModes)};
	VkExtent2D extent{chooseSwapExtent(window, swapChainSupport.capabilities)};

	uint32_t imageCount{swapChainSupport.capabilities.minImageCount + 1};

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		imageCount = swapChainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = m_context->getSurface();
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = m_imageUsage;

	QueueFamilyIndices indices = m_context->getQueueFamilies();
	std::set<uint32_t> uniqueIndices{indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value()};

	std::vector<uint32_t> queueFamilyIndices(uniqueIndices.begin(), uniqueIndices.end());

	if (indices.graphicsFamily != indices.presentFamily) { // queues separated
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = queueFamilyIndices.size();
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_context->getDevice(), &swapChainCreateInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapChain, &imageCount, nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapChain, &imageCount, m_images.data());

	m_imageFormat = surfaceFormat.format;
	m_extent = extent;
}

/// @brief Create framebuffers with 3 attachments : color (msaa), depth, swapchain image
/// @param renderPass 
/// @param depthImageView 
/// @param colorImageView 
void SwapChain::createFrameBuffers(VkRenderPass renderPass, VkImageView depthImageView, VkImageView colorImageView) {
	m_frameBuffers.resize(m_imageViews.size());

	for (size_t i{0}; i < m_frameBuffers.size(); ++i) {

		std::array<VkImageView, 3> attachments = {
			colorImageView, // MSAA color 
			depthImageView, // depth
			m_imageViews[i], // swapchain image (resolve target)
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_extent.width;
		framebufferInfo.height = m_extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_context->getDevice(), &framebufferInfo, nullptr, &m_frameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

/// @brief Creates an VkImageView for each swapchain image and stores them in m_imageViews. Each view is 2D, uses the swapchain image format, a single mip level/layer and the color aspect.
void SwapChain::createImageViews() {
	m_imageViews.resize(m_images.size());

	for (size_t i = 0; i < m_images.size(); i++) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_imageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image view!");
		}
	}
}