#include <VulkanApp/Core/VulkanContext.h>

#include <QVersionNumber>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <set>
#include <cstring>

/// @brief Retrieve for the given physical device, each queue family ids
/// @param device 
/// @return A struct containing : graphicsFamily, presentFamily, transferFamily, computeFamily ids
QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;

	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.transferFamily = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.computeFamily = i;
		}

		VkBool32 presentSupport{false};
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport); 

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;

		++i;
	}

	return indices;
}

/// @brief Retrieve for the current physical device, each queue family ids
/// @return A struct containing : graphicsFamily, presentFamily, transferFamily, computeFamily ids
QueueFamilyIndices VulkanContext::getQueueFamilies() {
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

	int i = 0;

	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.transferFamily = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.computeFamily = i;
		}

		VkBool32 presentSupport{false};
		vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport); 

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;

		++i;
	}

	return indices;
}

/// @brief check whether the validation layer is supported  
/// @return true if supports the validation layer for debugging
bool VulkanContext::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayer(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayer.data());

	for (auto layerName : validationLayers) {
		bool layerFound = false;
		for (const auto& layerPropreties : availableLayer) {
			if (std::strcmp(layerName, layerPropreties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;
	}

	return true;
}


/// @brief Retrieve the max msaa samples possible for the current physical device
/// @return 
VkSampleCountFlagBits VulkanContext::getMaxMsaa() {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

	VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT)
		return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT)
		return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT)
		return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT)
		return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT)
		return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT)
		return VK_SAMPLE_COUNT_2_BIT;

	return VK_SAMPLE_COUNT_1_BIT;
}

/// @brief Helper to retrieve the capabilities of the swapchain from the given physical device
/// @param device 
/// @return A struct containing, capabilities, formats, presentModes suppported by this device
SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

/// @brief Helper to retrieve the capabilities of the swapchain from the current physical device
/// @return  A struct containing, capabilities, formats, presentModes suppported by this device
SwapChainSupportDetails VulkanContext::getSwapChainSupport() {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

/// @brief check whether the device supports the required extensions
/// @param device 
/// @return true if all extensions are supported
bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

/// @brief Checks if the device supports all the necessary queues and extensions
/// @param device 
/// @return true if it is suitable
bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {
	QueueFamilyIndices queueFamily = findQueueFamilies(device);
	bool extensionsSupported{false};
	extensionsSupported = checkDeviceExtensionSupport(device);

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	bool swapChainAdequate{false};
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return queueFamily.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

/// @brief Iterate through the physical debices and picks a physical device that supports the required extensions/queues
void VulkanContext::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw std::runtime_error("failed to find GPUs with Vulkan support!");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			m_physicalDevice = device;
			m_msaaSamples = getMaxMsaa();
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("failed to find a suitable GPU");
}


/// @brief Creates the logical device and the necessary queues, uses 
/// @param enableValidationLayers 
void VulkanContext::createLogicalDevice(bool enableValidationLayers) { 
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value(), indices.computeFamily.value()};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// for compatibilty purposes
	if (enableValidationLayers) {
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		deviceCreateInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	} else {
		std::cout << "Device created!" << std::endl;
	}

	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
	vkGetDeviceQueue(m_device, indices.transferFamily.value(), 0, &m_transferQueue);
	if (indices.computeFamily.has_value()) {
		std::cout << "has compute queue" << '\n';
		vkGetDeviceQueue(m_device, indices.computeFamily.value(), 0, &m_computeQueue);
	} else {
		m_computeQueue = m_graphicsQueue;
	}
}




void VulkanContext::init(bool enableValidation){
	pickPhysicalDevice(); 
	createLogicalDevice(enableValidation); 
}

void VulkanContext::prepareInstanceForWindow(QWindow* window, bool enableValidation) {
    // Check layers support
    if (enableValidation && !checkValidationLayerSupport())
        throw std::runtime_error("Validation layers requested are not available!");

    if (enableValidation) {
        m_Qinstance.setLayers({"VK_LAYER_KHRONOS_validation"});
        m_Qinstance.setExtensions({ VK_EXT_DEBUG_UTILS_EXTENSION_NAME });
    }
    
    m_Qinstance.setApiVersion(QVersionNumber(1, 2));

    if (!m_Qinstance.create())
        throw std::runtime_error("Failed to create QVulkanInstance");

    // Lier l'instance à la fenêtre
    window->setVulkanInstance(&m_Qinstance);
    
    // Créer la surface
    m_surface = QVulkanInstance::surfaceForWindow(window);

    // Récupérer le handle brut
    m_instance = m_Qinstance.vkInstance(); // Pas besoin de static_cast, vkInstance() retourne un VkInstance

    // Créer le debug messenger
    if (enableValidation) {
        if (m_debug.createDebugMessenger(m_instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create debug messenger!");
        }
    }
}


void VulkanContext::cleanup() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        m_debug.destroyDebugMessenger(m_instance);
    }

    m_surface = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;

    m_graphicsQueue = VK_NULL_HANDLE;
    m_presentQueue = VK_NULL_HANDLE;
    m_transferQueue = VK_NULL_HANDLE;
    m_computeQueue = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}


/// @brief finds a memory type that fits the requirements 
/// @param typeFilter 
/// @param properties 
/// @return the index of the suitable memory type
uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties)) 
			// si i = 0 => 0001, i = 0 => 0010
			// permet de vérifier si le type est compatible mais pas suffisant, ex : si typeFilter 1010, a i =1 ca se stopperai mais pas forcement bon
			// on a rajoute le check sur les propriétés pour voir si c'est vraiment compatible
			return i;
	}

	throw std::runtime_error("failed to find suitable memory type!");
}


