#pragma once

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Core/SwapChain.h>
#include <VulkanApp/Rendering/RenderPass.h>
#include <VulkanApp/Commands/CommandManager.h>
#include <string>

#include <vulkan/vulkan.h>

class Image
{

public:
	Image() = default;
	~Image() = default;

	static Image createColorAttachment(VulkanContext* context, const SwapChain& swapchain);
	static Image createDepthAttachment(VulkanContext* context, const SwapChain& swapchain, const RenderPass& renderPass);
	static Image createTextureFromFile(VulkanContext* context, CommandManager* cmdManager, const std::string& path, bool createSampler = true, bool genMipMap = true);

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;
	Image(Image&& other) noexcept {
		*this = std::move(other);
	}

	Image& operator=(Image&& other) noexcept {
		if (this != &other) {
			cleanup();
			m_context = other.m_context;
			m_image = other.m_image;
			m_memory = other.m_memory;
			m_view = other.m_view;
			m_sampler = other.m_sampler;
			m_mipLevels = other.m_mipLevels;

			other.m_context = nullptr;
			other.m_image = VK_NULL_HANDLE;
			other.m_memory = VK_NULL_HANDLE;
			other.m_view = VK_NULL_HANDLE;
			other.m_sampler = VK_NULL_HANDLE;
			other.m_mipLevels = 1;
		}
		return *this;
	}


	void cleanup() noexcept;

	VkImage getImage() const { return m_image; }
    VkImageView getView() const { return m_view; }
    VkSampler getSampler() const { return m_sampler; }
    uint32_t getMipLevels() const { return m_mipLevels; }

private:
	void init(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool createSampler, bool generateMipMap);
	
	void createImage(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkSharingMode sharingMode, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags);
	void createSampler();
	
	void generateMipmaps(CommandManager* cmdManager, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void transitionImageLayout(CommandManager* cmdManager, VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(CommandManager* cmdManager, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);


	VulkanContext* m_context = nullptr;

	VkImage m_image = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VkSampler m_sampler = VK_NULL_HANDLE;
	uint32_t m_mipLevels{1};

};
