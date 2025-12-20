#pragma once

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Commands/CommandManager.h>

#include <vulkan/vulkan.h>

class Buffer
{
public:
	Buffer() = default;
	~Buffer() = default;

	void init( VulkanContext* m_context, CommandManager* m_commandManager, VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties);
	void cleanup() noexcept;
	void* map();
	void unmap();
	void write(const void* src, VkDeviceSize size, VkDeviceSize offset = 0);

	VkBuffer get() const { return m_buffer; };
	VkDeviceMemory getDeviceMemory() { return m_memory; };
	void* getMapped() const { return m_mapped; };
	VkDeviceSize getSize() const { return m_size; };

	void copyTo(VkBuffer dstBuffer, VkDeviceSize size);

private:

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties);

	VulkanContext* m_context = nullptr;
	CommandManager* m_commandManager = nullptr;

	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkDeviceSize m_size = 0;
	VkBufferUsageFlags m_usage;
	VkSharingMode m_sharingMode;
	VkMemoryPropertyFlags m_properties;
	void* m_mapped = nullptr;
};

