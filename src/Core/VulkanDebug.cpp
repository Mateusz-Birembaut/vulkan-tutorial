#include <VulkanApp/Debug/VulkanDebug.h>

#include <GLFW/glfw3.h>

#include <iostream>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {

	const char* sev = (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN"
												    : (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)	    ? "INFO"
																					    : "VERBOSE";

	std::cerr << "[" << sev << "] " << (pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "UnknownId")
		  << " (" << pCallbackData->messageIdNumber << ")\n"
		  << pCallbackData->pMessage << "\n\n";

	return VK_FALSE;
}

VkResult VulkanDebug::createDebugMessenger(VkInstance instance) {
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);
	return CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &m_debugMessenger);
}

VkResult VulkanDebug::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanDebug::destroyDebugMessenger(VkInstance instance) {
	if (m_debugMessenger != VK_NULL_HANDLE) {
		DestroyDebugUtilsMessengerEXT(instance, m_debugMessenger, nullptr);
		m_debugMessenger = VK_NULL_HANDLE;
	}
}

void VulkanDebug::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanDebug::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.pfnUserCallback = debugCallback;
}

std::vector<const char*> VulkanDebug::getRequiredExtensions(bool enableValidation) {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidation) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}