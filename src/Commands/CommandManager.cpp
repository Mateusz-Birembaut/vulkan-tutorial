#include <VulkanApp/Commands/CommandManager.h>

#include <VulkanApp/Core/VulkanContext.h>

#include <stdexcept>
#include <iostream>

void CommandManager::init(VulkanContext* context, const uint32_t max_frames_in_flight){
	m_context = context;
	createCommandPools();
	createCommandBuffers(max_frames_in_flight);
}

void CommandManager::cleanup() noexcept{
	if(m_context){
		auto device = m_context->getDevice();
		vkDeviceWaitIdle(device);

		if (!m_commandBuffers.empty() && m_commandPool != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
			m_commandBuffers.clear();
		}
		if(m_commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, m_commandPool, nullptr);
		if(m_commandPoolTransfer != VK_NULL_HANDLE && m_commandPoolTransfer != m_commandPool) vkDestroyCommandPool(device, m_commandPoolTransfer, nullptr);
	}
}


/// @brief if possible create a pool for transfer operations ans another one for draw otherwise command pool and command pool transfer will be the same
void CommandManager::createCommandPools() {
	QueueFamilyIndices queueFamilyIndices = m_context->getQueueFamilies();

	if(!queueFamilyIndices.graphicsFamily.has_value()) throw std::runtime_error("could not get a graphics queue");

	if(queueFamilyIndices.graphicsFamily == queueFamilyIndices.transferFamily){
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(m_context->getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		} else {
			std::cout << "Command pool created" << '\n';
			m_commandPoolTransfer = m_commandPool;
		}
	}else {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(m_context->getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		} else {
			std::cout << "command pool created" << '\n';
		}

		VkCommandPoolCreateInfo poolTransferInfo{};
		poolTransferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolTransferInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		poolTransferInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value();

		if (vkCreateCommandPool(m_context->getDevice(), &poolTransferInfo, nullptr, &m_commandPoolTransfer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		} else {
			std::cout << "Transfer command pool created" << '\n';
		}
	}
}

void CommandManager::createCommandBuffers(const uint32_t max_frames_in_flight) {

	m_commandBuffers.resize(max_frames_in_flight);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = m_commandBuffers.size();

	if (vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	} else {
		std::cout << "Command buffer created" << '\n';
	}
}


/// @brief records and execute a command buffer
/// @param commandPool 
/// @return VkCommandBuffer allocated 
VkCommandBuffer CommandManager::beginSingleTimeCommands(VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	return commandBuffer;
}

/// @brief finishes a command buffer, waits for the queue to end then destroys it
/// @param commandBuffer 
/// @param commandPool 
/// @param queue 
void CommandManager::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);
	vkFreeCommandBuffers(m_context->getDevice(), commandPool, 1, &commandBuffer);
}