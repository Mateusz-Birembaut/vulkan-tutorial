#include <VulkanApp/Sync/SyncObjects.h>

#include <iostream>

void SyncObjects::init(VulkanContext* context, uint32_t max_frames_in_flight){
	m_context = context;
	createSyncObjects(max_frames_in_flight);
}

void SyncObjects::cleanup() noexcept {
	if(m_context){
		auto device = m_context->getDevice();
		for (size_t i = 0; i < m_imageAvailableSemaphores.size(); i++) {
			if(m_imageAvailableSemaphores[i] != VK_NULL_HANDLE){
				vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
				vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
				vkDestroyFence(device, m_inFlightFences[i], nullptr);
			}
		}
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