#pragma once

#include <VulkanApp/Core/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <vector>

class SyncObjects
{

public:
	SyncObjects() = default;
	~SyncObjects() = default;

	std::vector<VkSemaphore>& getImgAvailableSemaphores() { return m_imageAvailableSemaphores; };
	std::vector<VkSemaphore>& getRenderFinishedSemaphores() { return m_renderFinishedSemaphores; };
	std::vector<VkFence>& getInFlightFences() { return m_inFlightFences; };

	void init(VulkanContext* context, uint32_t max_frames_in_flight);
	void cleanup() noexcept;

private:
	void createSyncObjects(uint32_t max_frames_in_flight);

	VulkanContext* m_context = nullptr;

	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

};



