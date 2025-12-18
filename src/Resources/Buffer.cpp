#include <VulkanApp/Resources/Buffer.h>

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <set>
#include <cstring>


void Buffer::init(VulkanContext* context, CommandManager* commandManager, VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties){
	m_context = context;
	m_commandManager = commandManager;
	createBuffer(size, usage, sharingMode, properties);
}

/// @brief returns the adress where the cpu can read/write buffer data
/// @return 
void* Buffer::map() {
    if (!m_mapped) {
        vkMapMemory(m_context->getDevice(), m_memory, 0, m_size, 0, &m_mapped);
    }
    return m_mapped;
}

void Buffer::unmap() {
    if (m_mapped) {
        vkUnmapMemory(m_context->getDevice(), m_memory);
        m_mapped = nullptr;
    }
}

void Buffer::write(const void* src, VkDeviceSize size, VkDeviceSize offset) {
    void* dst = map();
    std::memcpy(static_cast<char*>(dst) + offset, src, static_cast<size_t>(size));
    if (!(m_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = m_memory;
        range.offset = offset;
        range.size = size;
        vkFlushMappedMemoryRanges(m_context->getDevice(), 1, &range);
    }
}

void Buffer::cleanup() noexcept{
	if(m_context){
		if(m_mapped){
			unmap();
		}
		if(m_buffer != VK_NULL_HANDLE){
			vkDestroyBuffer(m_context->getDevice(), m_buffer, nullptr);
			m_buffer = VK_NULL_HANDLE;
		}
		if(m_memory != VK_NULL_HANDLE){
			vkFreeMemory(m_context->getDevice(), m_memory, nullptr);
			m_memory = VK_NULL_HANDLE;
		}
		m_size = 0;
	}
}

/// @brief Create a VkBuffer, allocate device memory of an appropriate memory type and bind the memory to the buffe
/// @param size 
/// @param usage 
/// @param sharingMode 
/// @param properties 
/// @param buffer 
/// @param bufferMemory 
void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties) {

	m_size = size;
	m_usage = usage;
	m_sharingMode = sharingMode;
	m_properties = properties;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = sharingMode;

	auto indices = m_context->getQueueFamilies();
	std::set<uint32_t> uniqueIndices = {
	    indices.graphicsFamily.value(),
	    indices.transferFamily.value(),
	};
	std::vector<uint32_t> queueFamilyIndices(uniqueIndices.begin(), uniqueIndices.end());

	if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
		bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
		bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	} else {
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
	}

	if (vkCreateBuffer(m_context->getDevice(), &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create vertex buffer");
	}
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_context->getDevice(), m_buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory");
	}

	vkBindBufferMemory(m_context->getDevice(), m_buffer, m_memory, 0);
};

/// @brief uses the transfer queue to copy the data from the srcBuffer to dstBuffer
/// @param srcBuffer 
/// @param dstBuffer 
/// @param size 
void Buffer::copyTo(VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBuffer commandBuffer = m_commandManager->beginSingleTimeCommands(m_commandManager->getTransferCommandPool());

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, m_buffer, dstBuffer, 1, &copyRegion);

	m_commandManager->endSingleTimeCommands(commandBuffer, m_commandManager->getTransferCommandPool(), m_context->getTransferQueue());
}
