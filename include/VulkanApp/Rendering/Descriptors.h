#pragma once

#include <VulkanApp/Core/VulkanContext.h>
#include<VulkanApp/Resources/Buffer.h>

#include <vulkan/vulkan.h>

#include <vector>



class Descriptors {

      public:
	Descriptors() = default;
	~Descriptors() = default;

	void init(VulkanContext* context, const uint32_t max_frames_in_flight, const std::vector<Buffer>& uniformBuffers, VkImageView textureImageView, VkSampler textureSampler);
	void cleanup() noexcept;

	VkDescriptorSetLayout getSetLayout() { return  m_setLayout;};
	VkDescriptorPool getPool() { return m_pool; };
	std::vector<VkDescriptorSet>& getSets() { return m_sets; };

      private:
	void createSetLayout();
	void createPool( const uint32_t max_frames_in_flight );
	void createSets( const uint32_t max_frames_in_flight, const std::vector<Buffer>& uniformBuffers, VkImageView textureImageView, VkSampler textureSampler );

	VulkanContext* m_context = nullptr;

	VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
	VkDescriptorPool m_pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> m_sets;
};
