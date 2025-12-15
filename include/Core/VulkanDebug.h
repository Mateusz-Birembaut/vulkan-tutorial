#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanDebug {
public:
    VulkanDebug() = default;
    ~VulkanDebug() = default;

    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	static std::vector<const char*> getRequiredExtensions(bool enableValidation);

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	
	VkResult createDebugMessenger(VkInstance instance);
	void destroyDebugMessenger(VkInstance instance);

private:
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
};