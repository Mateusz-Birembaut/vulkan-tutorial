#include <VulkanApp/Sync/SyncObjects.h>

#include <iostream>
#include <algorithm>

void SyncObjects::init(VulkanContext* context, uint32_t max_frames_in_flight){
	m_context = context;
	createSyncObjects(max_frames_in_flight);
}

void SyncObjects::cleanup() noexcept {
	if(m_context){
		auto device = m_context->getDevice();
		auto count = std::max({m_imageAvailableSemaphores.size(), m_renderFinishedSemaphores.size(), m_inFlightFences.size()});
		for (size_t i = 0; i < count; ++i) {
			if (i < m_imageAvailableSemaphores.size() && m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
				m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
			}
			if (i < m_renderFinishedSemaphores.size() && m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
				m_renderFinishedSemaphores[i] = VK_NULL_HANDLE;
			}
			if (i < m_inFlightFences.size() && m_inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, m_inFlightFences[i], nullptr);
				m_inFlightFences[i] = VK_NULL_HANDLE;
			}
		}

		std::vector<VkSemaphore>().swap(m_imageAvailableSemaphores);
		std::vector<VkSemaphore>().swap(m_renderFinishedSemaphores);
		std::vector<VkFence>().swap(m_inFlightFences);
		m_context = nullptr;
	}
}

void SyncObjects::createSyncObjects(uint32_t max_frames_in_flight) {

	m_imageAvailableSemaphores.resize(max_frames_in_flight);
	m_renderFinishedSemaphores.resize(max_frames_in_flight);
	m_inFlightFences.resize(max_frames_in_flight);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // créer en signaled,
	// car sinon on attend a l'infini car pour la 1ere  frame on attend de terminer de draw sauf qu'on draw pas

	for (size_t i = 0; i < max_frames_in_flight; i++) {
		if (vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
		    vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
		    vkCreateFence(m_context->getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}
	std::cout << "Sync objects created" << '\n';
}