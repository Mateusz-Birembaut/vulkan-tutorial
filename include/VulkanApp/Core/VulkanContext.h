#pragma once


#include <VulkanApp/Debug/VulkanDebug.h>


#include <vulkan/vulkan.h>

#include <QVulkanInstance>
#include <QWindow>

#include <vector>
#include <optional>

// Prepare a Vulkan instance and surface for a Qt window
// Creates the QVulkanInstance (m_Qinstance) and the VkSurfaceKHR (m_surface)
// then the caller can call init(enableValidation) to pick physical device and create logical device.


struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily; // on peut voir si on a une graphics queue, pour certaines queue on est pas obligé de l'avoir forcément
	std::optional<uint32_t> presentFamily;	// see if we can present images to the surface
	std::optional<uint32_t> transferFamily;
	std::optional<uint32_t> computeFamily; // dedicated compute queue for better performance

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


	void init(bool enableValidation);
    void cleanup() noexcept;

    // Create and initialize the QVulkanInstance and window surface
    void prepareInstanceForWindow(QWindow* window, bool enableValidation);


	VkInstance getInstance() const { return m_instance; }
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
	void setSurface(VkSurfaceKHR surface) { m_surface = surface;};
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkQueue getTransferQueue() const { return m_transferQueue; }
    VkQueue getComputeQueue() const { return m_computeQueue; }
    VkSampleCountFlagBits getMsaaSamples() const { return m_msaaSamples; }
	
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	SwapChainSupportDetails getSwapChainSupport();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	QueueFamilyIndices getQueueFamilies();

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
	VulkanDebug m_debug;

	QVulkanInstance m_Qinstance;
	VkInstance m_instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;

	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_presentQueue = VK_NULL_HANDLE;
	VkQueue m_transferQueue= VK_NULL_HANDLE;
	VkQueue m_computeQueue = VK_NULL_HANDLE;

	VkSampleCountFlagBits m_msaaSamples;
	VkSampleCountFlagBits getMaxMsaa();

	//void createInstance(bool enableValidationLayers);
	void pickPhysicalDevice();
	void createLogicalDevice(bool enableValidationLayers);

	bool checkValidationLayerSupport();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);

};


