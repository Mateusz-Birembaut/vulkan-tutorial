#pragma once


#include <VulkanApp/Debug/VulkanDebug.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <optional>


struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily; // on peut voir si on a une graphics queue, pour certaines queue on est pas obligé de l'avoir forcément
	std::optional<uint32_t> presentFamily;	// see if we can present images to the surface
	std::optional<uint32_t> transferFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};


const std::vector<const char*> validationLayers{
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> deviceExtensions{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

class VulkanContext
{

public:
	VulkanContext() {};
	~VulkanContext() {};

    void init(GLFWwindow* window, bool enableValidation);
    void cleanup() noexcept;

	VkInstance getInstance() const { return m_instance; }
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkQueue getTransferQueue() const { return m_transferQueue; }
    VkSampleCountFlagBits getMsaaSamples() const { return m_msaaSamples; }
	
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	SwapChainSupportDetails getSwapChainSupport();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	QueueFamilyIndices getQueueFamilies();

private:
	VulkanDebug m_debug;

	VkInstance m_instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;

	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_presentQueue = VK_NULL_HANDLE;
	VkQueue m_transferQueue= VK_NULL_HANDLE;

	VkSampleCountFlagBits m_msaaSamples;
	VkSampleCountFlagBits getMaxMsaa();

	void createInstance(bool enableValidationLayers);
	void createSurface(GLFWwindow* window);
	void pickPhysicalDevice();
	void createLogicalDevice(bool enableValidationLayers);

	bool checkValidationLayerSupport();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);


};


