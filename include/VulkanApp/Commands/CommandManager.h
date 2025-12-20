#pragma once

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>
#include <vector>

class CommandManager
{
public:
	CommandManager() = default;
	~CommandManager() = default;

	void init(VulkanContext* context, const uint32_t max_frames_in_flight);
	void cleanup() noexcept;

	VkCommandPool getCommandPool() { return m_commandPool; };
	VkCommandPool getTransferCommandPool() { return m_commandPoolTransfer; };
	std::vector<VkCommandBuffer>& getCommandBuffers() { return m_commandBuffers; };
	VkCommandPool getComputeCommandPool() { return m_commandPoolCompute; };

	VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue);

private:
	void createCommandPools();
	void createCommandBuffers(const uint32_t max_frames_in_flight) ;

	VulkanContext* m_context = nullptr;

	VkCommandPool m_commandPool = VK_NULL_HANDLE;
	VkCommandPool m_commandPoolTransfer = VK_NULL_HANDLE;
	VkCommandPool m_commandPoolCompute = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_commandBuffers;

};



