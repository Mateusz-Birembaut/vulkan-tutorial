
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // depth range [0, 1] au lieu de [-1, 1]

#include "VulkanApp.h"
#include "FileReader.h"
#include "Uniforms.h"
#include "Utility.h"
#include "Vertex.h"
#include "stb_image.h"
#include "tiny_obj_loader.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <string>

void VulkanApp::initWindow() {
	glfwInit(); // init la lib

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ne pas créer de context openGL
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // désactive le resize parce que c'est relou en gros

	m_window = glfwCreateWindow(g_screen_width, g_screen_height, "Vulkan App", nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, VulkanApp::framebufferResizeCallback);
	// callback glfw quand on resize
}

void VulkanApp::mainLoop() {

	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		drawFrame();
	}

	vkDeviceWaitIdle(m_device);
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->setResized(true);
}

bool VulkanApp::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	// on récupère les layer disponible
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

void VulkanApp::initVulkan() {
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPools();
	createDepthResources();
	createFrameBuffers();
	createTextureImage();
	createTextureImageView();
	createTextureImageSampler();
	loadMesh();
	createMeshBuffer();
	createUniformBuffer();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}

void VulkanApp::createSurface() {
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	} else {
		std::cout << "Window surface created!" << std::endl;
	}
}

void VulkanApp::createLogicalDevice() { // les queues sont créées en même temps que les logical device
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value()};

	float queuePriority = 1.0f; // influence l'execution du command buffer de 0.f à 1.f
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		// on peut créer plusieurs command buffers sur plusieurs threads donc ca permet de dire que celui doit etre effectué en prio si 1.0f
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{}; // le set de device features qu'on va utiliser (par exemple ceux de vkGetPhysicalDeviceFeatures)
	// pour l'instant on laisse a VK_FALSE
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo{}; // create logical device
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// c'est plus supporté, se sera ignoré, mais pour des soucis de compatibilité, on peut l'ajouter
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
}

void VulkanApp::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw std::runtime_error("failed to find GPUs with Vulkan support!");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			m_physicalDevice = device;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("failed to find a suitable GPU");
}

bool VulkanApp::checkDeviceExtensionSupport(VkPhysicalDevice device) {
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

bool VulkanApp::isDeviceSuitable(VkPhysicalDevice device) {
	QueueFamilyIndices queueFamily = findQueueFamilies(device);
	bool extensionsSupported{false};
	extensionsSupported = checkDeviceExtensionSupport(device);

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
	// donne les features supporté par le gpu

	bool swapChainAdequate{false};
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return queueFamily.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

void VulkanApp::createInstance() {

	if (!checkValidationLayerSupport() && enableValidationLayers)
		throw std::runtime_error("Validation layers requested are not available!");

	VkApplicationInfo appInfo{}; // techniquement optional, info sur l'app
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "no engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;
	// appInfo.pNext point to extension info in the future, se sera nullptr pour l'instant

	VkInstanceCreateInfo createInfo{}; // pas optional, pour définir quelles extensions et validation layers on a besoin
	// les validation layers permettent de débug car de base dans l'api y'a pas bcp de error checking
	createInfo.pApplicationInfo = &appInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

	// comme on a besoin de savoir sur quel os on est vu que vulkan est platform agnostic, on utilise glfw pour avoir ses infos
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // VK_KHR_surface enabled ici et d'autres extensions

	auto extensions = getRequiredExtensions();

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data(); // on donne les noms des extension et ca va les activer

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	// VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance); // créer l'instance avec les infos fournit
	//  souvent les fonction on une struct pour la création, un pointeur syr un allocator callback (pas utilisé dans le tuto), un pointeur pour stocker l'object créer
	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) { // créer l'instance avec les infos fournit et check si ca a marché
		throw std::runtime_error("failed to create instance!");
	} else {
		std::cout << "Vulkan instance created" << '\n';
	}
}

QueueFamilyIndices VulkanApp::findQueueFamilies(VkPhysicalDevice device) {
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

		VkBool32 presentSupport{false};
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport); // les deux indices peuvent etre les meme, c'est mieux si c'est les meme, les perfs son meilleurs

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;

		++i;
	}

	return indices;
}

SwapChainSupportDetails VulkanApp::querySwapChainSupport(VkPhysicalDevice device) {
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

VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// le format de couleur qu'on va utiliser

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}
	return availableFormats[0]; // si on a pas le srgb non linéaire supporté, on retourne le premier format support
				    // mais on pourrait rank ceux qu'ils restent et prendre le meilleur par exemple
}

VkPresentModeKHR VulkanApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	// le type de queue qu'on va utiliser pour stocker nos images a afficher a l'écran
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			// triple buffering, si le buffer est plein, on remplace les images dans le buffer, compromi, réduit quand meme la latence sans tearing
			return availablePresentMode;
	}
	return VK_PRESENT_MODE_FIFO_KHR; // le seul mode supporté par défaut
					 // ca attend si le buffer d'images est plein (l'écran trop lent par exemple)
}

VkExtent2D VulkanApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	// la résolution des images dans la swap chain
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		int height;
		int width;
		glfwGetFramebufferSize(m_window, &width, &height); // donne la taille en pixel de la fenetre,
		// on a besoin pour choisir la résolution qui doit etre exprimé en pixels

		VkExtent2D actualExtent{
		    static_cast<uint32_t>(width),
		    static_cast<uint32_t>(height),
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void VulkanApp::createSwapChain() {

	SwapChainSupportDetails swapChainSupport{querySwapChainSupport(m_physicalDevice)};

	if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
		throw std::runtime_error("No suitable swap chain formats or present modes found.");
	}

	VkSurfaceFormatKHR surfaceFormat{chooseSwapSurfaceFormat(swapChainSupport.formats)};
	VkPresentModeKHR presentMode{chooseSwapPresentMode(swapChainSupport.presentModes)};
	VkExtent2D extent{chooseSwapExtent(swapChainSupport.capabilities)};

	uint32_t imageCount{swapChainSupport.capabilities.minImageCount + 1}; // nb d'images dans notre swap chain

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) // = 0 veut dire pas de max
		imageCount = swapChainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = m_surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;			      // le nombre de couche d'une image quasiment tout le temps 1 sauf si stereoscopic 3D application
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // le type d'opérations qu'on va utiliser pour nos images
	// si on veut faire du post process, on va rendre les images dans un image d'abord
	// alors la on peut utiliser VK_IMAGE_USAGE_TRANSFER_DST_BIT à la place et utiliser une opération mémoire
	// pour transférer l'image rendue vers une image du swap chain

	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	std::set<uint32_t> uniqueIndices{indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value()};

	std::vector<uint32_t> queueFamilyIndices(uniqueIndices.begin(), uniqueIndices.end());
	// queue present et transfer peuvent etre les même

	if (indices.graphicsFamily != indices.presentFamily) { // si les deux queues sont séparé
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = queueFamilyIndices.size();
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;	   // optional
		swapChainCreateInfo.pQueueFamilyIndices = nullptr; // optional
	}

	swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	// on va pas utiliser de rotation ou flip ou quoi, on précisé alors juste la current transform
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// est ce que on a besoin de blend avec d'autres fenetre, pas souvent utilisé

	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	// si clipped, on regarde pas la couleur des pixels caché par une autre fenetre

	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	// utilise quand on resize par exemple, on doit detruire la swapchain
	// a ce moment la on utilise ça

	if (vkCreateSwapchainKHR(m_device, &swapChainCreateInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
}

void VulkanApp::createImageViews() {
	m_swapChainImageViews.resize(m_swapChainImages.size());

	for (size_t i = 0; i < m_swapChainImages.size(); i++) {

		m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, m_mipLevels, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	// std::cout << "all image views created" << std::endl;
}

void VulkanApp::createDescriptorSetLayout() {
	// défini quels type de descriptor qui peut etre bound

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	// possible d'avoir un array, pour ça qu'on a un count, peut etre utilise pour avoir des transformations sur chaque bone dans un squelette d'animation
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	// a quel stage on va utiliser le ubo, peut en avoir plusieurs ou VK_SHADER_STAGE_ALL_GRAPHICS pour tous
	uboLayoutBinding.pImmutableSamplers = nullptr; // optional
	// utilisé pour les uniforms en rapport avec de l'image sampling

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr; // optional

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void VulkanApp::createGraphicsPipeline() {
	auto vertShaderCode{FileReader::readSPV(g_vertex_shader)};
	auto fragShaderCode{FileReader::readSPV(g_fragment_shader)};

	VkShaderModule vertShaderModule{createShaderModule(vertShaderCode)};
	VkShaderModule fragShaderModule{createShaderModule(fragShaderCode)};

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // a quel endroit on va utiliser ce shader
	// y'a un opetion pour chaque étape de la pipeline graphique vertex shader, tesselation, geom shader ect..

	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";
	// ici vulkan va éxécuter le code dans main du vertexShader
	// on peut "combiner" des shaders dans 1 module mais pas le cas ici

	// vertShaderStageInfo.pSpecializationInfo permet de définir des constantes,
	//  le compilateur pourra alors mieux les optimiser plutot que d'utiliser des valeur runtime

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[]{vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	// equivalent en opengl : bind
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	// equivalent opengl : comment sont organisé les données

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	// donne comment on va afficher comme opengl, points, triangle strip ect.
	// primitiveRestartEnable = on veut réutiliser des points avec un index buffer
	// 0xFFFF ou 0xFFFFFFFF pour dire qu'on termine une triangle strip par exemple

	/*
	si pas dynamic
	VkViewport viewPort{};
	viewPort.x = 0.0f;
	...

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = m_swapChainExtent;
	*/

	std::vector<VkDynamicState> dynamicStates{
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	}; // permet de modifier dans le command buffer le scissor ou viewport

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;

	// rasterizer fais le depth testing, face culling, scissor test, et wireframe ou pas
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	// si true, les fragment en dehors du near et far plane sont clamped au lieu d'être ignoré

	rasterizer.rasterizerDiscardEnable = VK_FALSE; // si true, on skip le rasterizer
	// et on peut pas output dans le framebuffer

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // en gros c'est le mode classique
	// ici qu'on met le wireFrame ou point pour chaque pts
	// il faut activer une gpu feature si on veut une des autres options

	rasterizer.lineWidth = 1.0f; // si on veut plus de 1, on doit aussi activer une gpu feature "widelines"

	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;		// on cull classiquement
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // en Vulkan, avec viewport non inversé, l'enroulement visuel CCW devient CW

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f;	   // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f;	   // Optional
	// permet de modif la depth value en ajouter des constants ou quoi
	// utilisé pour du shadow mapping

	// multiSampling, forme d'anti aliasing, la combinaison des couleurs de fragment d'un seul pixel
	// c'est pas utilisé si pour un fragment on a qu'une couleur
	// donc c'est efficace

	VkPipelineMultisampleStateCreateInfo multisampling{}; // pour l'instant désactivé
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// depth / stencil testing
	// si on l'utilise on fait un VkPipelineDepthStencilStateCreateInfo
	// sinon nullptr
	// on verra apres

	// color blending
	// dernière étape avant framebuffer
	// deux modes soit
	// VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer and the second struct
	// VkPipelineColorBlendStateCreateInfo contains the global color blending settings

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE; // on blend pas
	// notre nouvelle image ne sera pas combiné avec l'ancienne
	// le plus commun c'est de blend en utilisant l'alpha channel

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional, permet de blend avec des opération sur les bits
	// ca désactivera la première méthode avec l'attachment, comme si on avait blendEnable = VK_FALSE;
	// pour chaque framebuffer, la on fera pas de blend
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// pipeline layout
	// uniforms spécifié ici
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;	  // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	// la on peut passer des variables dynamic a nos shader
	// par exemple utime je crois

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	} else {
		std::cout << "Pipeline layout created" << '\n';
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE; // si on passe de depth test on écrit la couleur
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE; // permet de garder les fragments qui tombe dans une certaine range de depth
	depthStencilInfo.minDepthBounds = 0.0f;		   // optional
	depthStencilInfo.maxDepthBounds = 1.0f;		   // optional
	depthStencilInfo.stencilTestEnable = VK_FALSE;
	depthStencilInfo.front = {}; // optional
	depthStencilInfo.back = {};  // optional

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2; // nb elements dans shaderStages
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;

	pipelineInfo.layout = m_pipelineLayout;

	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0; // index de la subpass

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1;		  // Optional
	// permet de créer des pipelines depuis d'autres pipelines
	// moins couteux quand fonctionnalité en commnun
	// et switch entre 2 pipeline qui ont le meme parent est plus rapide aussi

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
				      &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
	// ca prend plus de param car on peut créer plusieurs pipeline avec 1 call
	// 2eme param est un cache pour reutiliser des données utile a la création de pipeline
	// permet de speed up la creation
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	} else {
		std::cout << "Graphics pipeline created" << '\n';
	}

	vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
}

VkShaderModule VulkanApp::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	// vector gère le pire d'alignement donc c'est bon

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

void VulkanApp::createRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChainImageFormat; // devrait etre le meme que notre swapchain
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // pas de multi sampling donc 1 sample

	// comment les couleur / depth data vont etre géré
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// VK_ATTACHMENT_LOAD_OP_CLEAR, on va clear le framebuffer avant d'écrire dedans
	// VK_ATTACHMENT_LOAD_OP_LOAD, garde les valeurs dans le framebuffer, si on veut accumuler par exemple
	// VK_ATTACHMENT_LOAD_OP_DONT_CARE, le svaleurs sont undefined on s'en fou d'elles

	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// VK_ATTACHMENT_STORE_OP_STORE, les elements rendu sont stocké dans la mémoire et lu apres
	// par exemple affiché sur l'écran, utilisé dans une autre pass
	// VK_ATTACHMENT_STORE_OP_DONT_CARE, le contenu est undifined
	// depth buffer ca garde que les fragment visible et les valeurs
	// de profondeur on pas besoin d'être stocké

	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // meme chose qu'en haut mais pour les stencil data
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// les textures et framebuffers sont des VkImage avec un certain format
	// mais le layout des pixels en mémoire peut etre modifé en fonction de ce qu'on veut faire
	// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, images utilisé comme color attachment
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, images qui vont allé dans la swap chain
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, images qui vont etre utilisé dans une opération mémoire de copie

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // ce qu'il y aura avant le début de la render pass
	// on ne s'interresse pas au layout de la dernière image, les donnees sont pas forcément gardé
	// mais de toutes facon on clear avant d'écrire donc pas grave
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// indique que l'image sera utilisée pour être présentée à l'écran via la swapchain après la fin de la render pass.

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; // index dans l'array des attachmentDescription
	// le notre c'est un seul VkAttachmentDescription donc 0
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat(); // doit etre le meme format que l'image
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // on stocke pas car une fois draw on va pas le réutiliser, permet optimisations
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	// peut etre un passe de compute donc on précise que c'est une de graphisme
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	// subpass.pPreserveAttachments, données qui doivent etre gardé

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // c'est la pass implicit avec la render pass
	dependency.dstSubpass = 0;		     // 0 c'est notre render pass

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // quoi attendre
	// on doit wait que la swap chain finisse de lire l'image avant qu'on y accède
	// pour ça, on attend au niveau de la color attachment stage
	// mtn on attend aussi que le depth buffer test

	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // a quel stage ca arrive

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	// les opérations doivent attendre qu'on soit dans le stage color attachment
	// le rendu des pixels dans le framebuffer ne commence pas tant
	// qu'on est pas dans l'état ecriture en gros

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	} else {
		std::cout << "Render pass created" << '\n';
	}
}

void VulkanApp::createFrameBuffers() {
	m_swapChainFrameBuffers.resize(m_swapChainImageViews.size());

	for (size_t i{0}; i < m_swapChainImageViews.size(); ++i) {

		std::array<VkImageView, 2> attachments = {
		    m_swapChainImageViews[i],
		    m_depthImageView,
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapChainExtent.width;
		framebufferInfo.height = m_swapChainExtent.height;
		framebufferInfo.layers = 1; // nos swapchain images ont 1 seule image

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFrameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}

	// std::cout << "Frame buffers created" << '\n';
}

void VulkanApp::createCommandPools() {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, si on veut que les command buffers soit redefini souvent
	// ca peut changer l'allocation de la mémoire pour le rendre plus rapide
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, on veut changer quelques command buffers individuellement
	// sinon elle sont reset toutes ensemble sans ça
	// ici on va record les commandes buffer (commandes pour dessiner dans l'image) pour la frame actuelle
	// donc et on a pas besoin besoin de toucher aux autres donc on utilise VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	} else {
		std::cout << "Command pool created" << '\n';
	}

	VkCommandPoolCreateInfo poolTransferInfo{};
	poolTransferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolTransferInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	poolTransferInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value();

	if (vkCreateCommandPool(m_device, &poolTransferInfo, nullptr, &m_commandPoolTransfer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	} else {
		std::cout << "Command pool created" << '\n';
	}
}

void VulkanApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = sharingMode;

	auto indices = findQueueFamilies(m_physicalDevice);
	std::set<uint32_t> uniqueIndices = {
	    indices.graphicsFamily.value(),
	    indices.transferFamily.value(),
	};
	std::vector<uint32_t> queueFamilyIndices(uniqueIndices.begin(), uniqueIndices.end());

	if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
		// on défini les queues qui vont acceder a notre buffer
		bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
		bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	} else {
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
	}

	if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create vertex buffer");
	}
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, permet de ne pas flush la mémoire, on s'assure que la mémoire mappé
	// match le contenu de la mémoire alloué, peut etre moins performant que flush mais pas important pour l'instant

	if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		// alloue la mémoire, en fonction de VK_MEMORY_PROPERTY, sur le gpu ou la ram
		throw std::runtime_error("failed to allocate vertex buffer memory");
	}
	// normalement

	vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
	// fait le lien entre l'object buffer et l'espace mémoire attribué a bufferMemory
	// dernier param : offset, ici 0, si c'est différent de 0,
	// l'offset doit etre divisible par memRequirements.alignment
};

void VulkanApp::createMeshBuffer() {
	VkDeviceSize verticesSize = m_mesh.vertexSize() * m_mesh.verticesCount();
	VkDeviceSize indicesSize = m_mesh.indexSize() * m_mesh.indicesCount();

	// je dois avoir la fin du buffer aligné (un miltiple de ...)
	// comme j'ai des indices uint16, je dois avoir une size multiple de 2
	// formule pour aligner au multiple :
	// aligned = ((operand + (alignment - 1)) & ~(alignment - 1)) voir utility.h
	// m_indicesOffset = AlignTo(verticesSize, 2);
	// align 16
	m_indicesOffset = AlignTo(verticesSize, 4);
	VkDeviceSize bufferSize = m_indicesOffset + indicesSize;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(
	    bufferSize,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_SHARING_MODE_CONCURRENT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	    stagingBuffer, stagingBufferMemory);
	setObjectName(stagingBuffer, "MeshStagingBuffer");

	void* data;
	vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, m_mesh.verticesData(), static_cast<size_t>(verticesSize));

	memcpy(static_cast<char*>(data) + m_indicesOffset, m_mesh.indicesData(), static_cast<size_t>(indicesSize));
	vkUnmapMemory(m_device, stagingBufferMemory);

	createBuffer(
	    bufferSize,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	    VK_SHARING_MODE_CONCURRENT,
	    // sharing mode concurrent car va etre utilisé par la transfert queue et la graphics queue
	    // comme j'ai fais en sorte de séparer les deux si possible
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	    m_meshBuffer, m_meshBufferMemory);

	setObjectName(m_meshBuffer, "MeshsssssBuffer");

	copyBuffer(stagingBuffer, m_meshBuffer, bufferSize);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void VulkanApp::createUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	m_uniformBuffers.resize(g_max_frames_in_flight);
	m_uniformBuffersMemory.resize(g_max_frames_in_flight);
	m_uniformBuffersMapped.resize(g_max_frames_in_flight);

	for (int i{0}; i < g_max_frames_in_flight; ++i) {

		createBuffer(
		    bufferSize,
		    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		    VK_SHARING_MODE_EXCLUSIVE,
		    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		    m_uniformBuffers[i], m_uniformBuffersMemory[i]);

		setObjectName(m_uniformBuffers[i], "UniformBuffer");

		vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
	}
}

void VulkanApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(m_commandPoolTransfer);
	// commence a record sur la pool de transfer vu qu'on va copier des données

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	// commande pour effectuer le transfer

	endSingleTimeCommands(commandBuffer, m_commandPoolTransfer, m_transferQueue);
}

// on a besoin de combiner les besoin de notre app et les besoins de notre buffer
// on recupère le type de mémoire gpu qui nous permet ça
uint32_t VulkanApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

	// memProperties.memoryHeaps, ressources mémoire comme Vram, swap space in RAM (quand la vram est épuisé) ect.
	// memProperties.memoryTypes, types de mémoire dans ces heaps
	// on s'occupera pas de la heap, mais ca a un impact sur les perfs, dans d'autres applis faut vérifier

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties))
			// si i = 0 => 0001, i = 0 => 0010
			// permet de vérifier si le type est compatible mais pas suffisant, ex : si typeFilter 1010, a i =1 ca se stopperai mais pas forcement bon
			// on a rajoute le check sur les propriétés pour voir si c'est vraiment compatible
			return i;
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanApp::createCommandBuffers() {

	m_commandBuffers.resize(g_max_frames_in_flight);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	//
	allocInfo.commandBufferCount = m_commandBuffers.size();

	if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	} else {
		std::cout << "Command buffer created" << '\n';
	}
}

void VulkanApp::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	// writes command qu'on veut execute

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;		      // optional
	beginInfo.pInheritanceInfo = nullptr; // optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_renderPass;
	renderPassInfo.framebuffer = m_swapChainFrameBuffers[imageIndex];
	// on bind sur quelle swapchainFramebuffer on va écrire (qui est est lui meme relié a une swap chain image)

	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = m_swapChainExtent;
	// size of render area, pour de meilleur perfs, ca doit match la render area de l'attachment

	std::array<VkClearValue, 2> clearValues{}; // mtn un tableau, on clear l'image et la profondeur
	// l'ordre doit etre égal à l'ordre des attachments
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0}; // 1.0 = le plus éloigné

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();
	// valeurs utilisé par VK_ATTACHMENT_LOAD_OP_CLEAR

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	// render pass commence

	// les fonction ckCmd sont pour enregistrer les commandes,
	// 1 er param le command buffer, ensuite infos de la renderpass,
	// 3 eme, comment les drawing commands dans la render pass vont etre passé
	// VK_SUBPASS_CONTENTS_INLINE, on met les commandes dans le primary command buffer, pas de secondaire utilisé
	// VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS, éxécuté depuis le secondaire

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	// VK_PIPELINE_BIND_POINT_GRAPHICS, c'est une pipeline de rendu

	VkBuffer vertexBuffers[]{m_meshBuffer};
	VkDeviceSize offsets[]{0};

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, m_meshBuffer, m_indicesOffset, VK_INDEX_TYPE_UINT32);
	// VK_INDEX_TYPE_UINT32 si on veut plus d'indices, mais dans ce cas modif vecteur d'indices aussi

	// comme on a défini le viewport et scissor dynamiquement on doit les spécifier ici

	VkViewport viewport{}; // la région de l'output buffer dans laquelle on écrit ex:
	// si c'est 0,0 à width height alors on écrit dans tout le buffer
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapChainExtent.width);
	viewport.height = static_cast<float>(m_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{}; // seuls les pixels dans cette région seront rendu
	// reste ignoré.
	scissor.offset = {0, 0};
	scissor.extent = m_swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// la command pour draw 1 triangle de 3 index :
	// vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
	// avant dernier : offset dans le vertexbuffer, défini le vertex index le plus petit
	// dernier : ofsset pour les instanced rendering, def le + petit

	vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_pipelineLayout,
				0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);

	// utilisation de l'index buffer mtn
	vkCmdDrawIndexed(commandBuffer, m_mesh.indicesCount(), 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void VulkanApp::createSyncObjects() {

	m_imageAvailableSemaphores.resize(g_max_frames_in_flight);
	m_renderFinishedSemaphores.resize(g_max_frames_in_flight);
	m_inFlightFences.resize(g_max_frames_in_flight);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // créer en signaled,
	// car sinon on attend a l'infini car pour la 1ere  frame on attend de terminer de draw sauf qu'on draw pas

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
		    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
		    vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}
	std::cout << "Sync objects created" << '\n';
}

void VulkanApp::drawFrame() {
	// attendre frame precedente fini
	vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX); // bloque le cpu (host)
	// prend un array de fence, ici 1 seule, VK_TRUE, on attend toutes les fences
	// UINT64_MAX en gros desactive le timeout tellement c grand

	// prendre une image de la swapchain
	uint32_t imageIndex; // index de la vkimagedans le swap chain images
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
	// m_imageAvailableSemaphore signaled quand on a fini

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// VK_ERROR_OUT_OF_DATE_KHR, on peut plus du tout utilisé la swap chain et la surface pour rendre car elles sont devenu incompatibles
		// on la recréer et on skip ce draw
		// par contre ca peut créer un deadlock, on décale le reset lock apres le check
		recreateSwapChain();
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	// result peut être VK_SUBOPTIMAL_KHR, la surface match pas exactement la swap chain
	// mais on peut quand meme l'utiliser donc on conitnue

	updateUniformBuffer(m_currentFrame);

	vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]); // on débloque l'éxécution manuellement, ici pour eviter deadlock
	// on est sur d'avoir une image a draw
	// si on reset et que recreate swap chain est appelé, alors on sera toujours
	// sur la fence m_inFlightFences[m_currentFrame] et on sera bloqué par vkWaitForFences

	// record un command buffer pour draw sur l'image
	vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
	recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

	// submit l'image
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[]{m_imageAvailableSemaphores[m_currentFrame]};
	VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// on veut attendre au niveau de l'écriture dans le frame buffer
	// ca veut dire que le gpu peut executer des shaders juste on écrit pas encore
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
	// les buffers a submit pour execution

	VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	// on va signal ce sémaphore apres que le command buffer soit executé

	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// presenter l'image a la swap chain image pour affichage
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	// quels semaphores on va attendre avant de presenter l'image
	// tant que l'image n'est pas rendu, on att

	VkSwapchainKHR swapChains[] = {m_swapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;
	// a quel swap chain on va presenter les images
	// et quelles images (indices) le plus souvent 1 seule

	presentInfo.pResults = nullptr; // optional
	// permet de spécifier un array Vkresult
	// pour savoir si la présentation a marché dans chaque swap chain

	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	// asynchrone, donc le cpu va continuer
	// et commencer a acquerir une image pour le prochain rendu

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
		// la on recréer meme si VK_SUBOPTIMAL_KHR
		// car on veut le meilleur rendu
		m_framebufferResized = false;
		recreateSwapChain();
		return;
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	m_currentFrame = (m_currentFrame + 1) % g_max_frames_in_flight;
}

void VulkanApp::cleanupSwapChain() {
	for (auto frameBuffer : m_swapChainFrameBuffers) {
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	for (auto imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device, imageView, nullptr);
	}

	vkDestroyImageView(m_device, m_depthImageView, nullptr);
	vkDestroyImage(m_device, m_depthImage, nullptr);
	vkFreeMemory(m_device, m_depthImageMemory, nullptr);

	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

void VulkanApp::recreateSwapChain() {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}
	// quand on est minimiser width et height sont a 0, tant que c'est le cas,
	//  on fait rien, boucle infini, tant qu'on a pas re ouvert

	vkDeviceWaitIdle(m_device);

	cleanupSwapChain();

	createSwapChain();
	createImageViews();
	// les images views doivent etre recrée car sont lies au dimensions des images de la swapchain
	createDepthResources();
	createFrameBuffers();
	// pareil pour les framebuffers
}

void VulkanApp::cleanup() {	    // les queues sont détruites implicitement
	vkDeviceWaitIdle(m_device); // pour attendre que toutes les opération vk soient terminées

	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}

	cleanupSwapChain();

	vkDestroySampler(m_device, m_textureSampler, nullptr);
	vkDestroyImageView(m_device, m_textureImageView, nullptr);
	vkDestroyImage(m_device, m_textureImage, nullptr);
	vkFreeMemory(m_device, m_textureImageMemory, nullptr);

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
		vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
	vkDestroyBuffer(m_device, m_meshBuffer, nullptr);
	vkFreeMemory(m_device, m_meshBufferMemory, nullptr);

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}
	vkDestroyCommandPool(m_device, m_commandPoolTransfer, nullptr);
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	vkDestroyDevice(m_device, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage) {
	static auto startTime{std::chrono::high_resolution_clock::now()};
	auto currentTime{std::chrono::high_resolution_clock::now()};
	float time{std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count()};

	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	// rotation avec le temps qui passe sur l'éxe y

	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	// caméra en hauter et qui regarde en 0,0,0, up de la camera en 0,0,1

	ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / static_cast<float>(m_swapChainExtent.height), 0.1f, 10.0f);
	// camera d'un fov de 45, avec la taille = a celle de nos images et un near plan à 0.1F et far a 10.0f

	ubo.proj[1][1] *= -1; // car glm pour OpenGL et l'axe y est inversé par rapport a vulkan

	memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	// m_uniformBuffersMapped adresse accessible ou vont être stockées les données de l'ubo
}

void VulkanApp::createDescriptorPool() {
	// les descriptor peuvent pas etre créer directement, ils doivent etre alloué depuis un pool
	// comme les command buffer

	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(g_max_frames_in_flight);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(g_max_frames_in_flight);

	VkDescriptorPoolCreateInfo infoPool{};
	infoPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	infoPool.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	// le nombre de descriptor qui peuvent être alloué
	infoPool.pPoolSizes = poolSizes.data();
	// nombre max de descriptor
	infoPool.maxSets = static_cast<uint32_t>(g_max_frames_in_flight);

	if (vkCreateDescriptorPool(m_device, &infoPool, nullptr, &m_descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void VulkanApp::createDescriptorSets() { // les descriptors vont décrire comment acceder aux ressources depuis les shaders
	// les sets sont des paquest de descriptors
	std::vector<VkDescriptorSetLayout> layouts(g_max_frames_in_flight, m_descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(g_max_frames_in_flight);
	allocInfo.pSetLayouts = layouts.data();

	m_descriptorSets.resize(g_max_frames_in_flight);

	if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor sets!");
	}

	for (int i{0}; i < g_max_frames_in_flight; ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = m_textureImageView;
		imageInfo.sampler = m_textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		// binding de l'uniforme dans le shader
		descriptorWrites[0].dstArrayElement = 0; // un descriptor peut etre un array un array,
		// on doit spécifier le premier index, ici pas un tableaun l'indice est 0
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		// combien d'array on veut update

		descriptorWrites[0].pBufferInfo = &bufferInfo; // refers to un data buffer

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

std::vector<const char*> VulkanApp::getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
};

void VulkanApp::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.pfnUserCallback = debugCallback;
}

void VulkanApp::setupDebugMessenger() {
	if (!enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkResult VulkanApp::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanApp::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanApp::setObjectName(VkBuffer buffer, const char* name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(buffer);
	nameInfo.pObjectName = name;
	auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectNameEXT");
	if (func)
		func(m_device, &nameInfo);
}

void VulkanApp::createTextureImage() {
	int texWidth;
	int texHeight;
	int texChannels;

	stbi_uc* pixels = stbi_load(g_texture_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imgSize = texWidth * texHeight * 4;
	if (!pixels) {
		throw std::runtime_error("failed to load image!");
	}

	// pour avoir notre mipmap on prend la + grande dimension, on recupere par combien de fois on peut diviser par 2
	m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight))));

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(
	    imgSize,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_SHARING_MODE_EXCLUSIVE,
	    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	    stagingBuffer, stagingBufferMemory);

	setObjectName(stagingBuffer, "ImageStagingBuffer");

	void* data;
	vkMapMemory(m_device, stagingBufferMemory, 0, imgSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imgSize));
	vkUnmapMemory(m_device, stagingBufferMemory);

	stbi_image_free(pixels);

	createImage(texWidth, texHeight,
		    VK_FORMAT_R8G8B8A8_SRGB /*4 int8 pour chaque pixels */, m_mipLevels,
		    VK_SHARING_MODE_CONCURRENT, // besoin de la queue transfer et graphics comme j'ai les deux dans deux queues différentes
		    VK_IMAGE_TILING_OPTIMAL,	// ici pour avoir un accès le plus efficace possible
		    // tiling linéaire row major order
		    VK_IMAGE_USAGE_TRANSFER_SRC_BIT /*l'image servira de source et destinaation pour les transfert car on va generer les mipmaps*/ | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // on veut pouvoir transferer des données, et l'utiliser comme sampler
		    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,																			   // stocker de manière a avoir un accès rapide
		    m_textureImage, m_textureImageMemory);

	// modifier l'état de l'image en gros pour effectuer certaines opérations ici
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL pour copier les données
	transitionImageLayout(m_commandPoolTransfer, m_transferQueue, m_textureImage,
			      VK_FORMAT_R8G8B8A8_SRGB, m_mipLevels,
			      VK_IMAGE_LAYOUT_UNDEFINED /*on s'occupe pas de l'état precedent*/,
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyBufferToImage(m_commandPoolTransfer, m_transferQueue, stagingBuffer, m_textureImage, texWidth, texHeight);

	// une 
	generateMipmaps(m_commandPool, m_graphicsQueue, m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_mipLevels);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingBufferMemory, nullptr);
}

void VulkanApp::createImage(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, VkSharingMode sharingMode,
			    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			    VkImage& image, VkDeviceMemory& imageMemory) {

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.depth = 1;
	imageInfo.extent.height = height;
	imageInfo.extent.width = width;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = sharingMode;

	auto indices = findQueueFamilies(m_physicalDevice);
	std::set<uint32_t> uniqueIndices{indices.transferFamily.value(), indices.graphicsFamily.value()};
	std::vector<uint32_t> queueIndices(uniqueIndices.begin(), uniqueIndices.end());
	if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
		imageInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueIndices.size());
		imageInfo.pQueueFamilyIndices = queueIndices.data(); //
	}
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // multisampling
	imageInfo.flags = 0;			   // Optional, voir pour 3D voxel en grande partie vide ex => nuages

	if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(m_device, image, imageMemory, 0);
}

// record et execute un command buffer
VkCommandBuffer VulkanApp::beginSingleTimeCommands(VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	return commandBuffer;
}

void VulkanApp::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	// envoie a la queue ce qu'on a record,
	// on a pas a attendre, on veut lancer le transfert directement

	vkQueueWaitIdle(queue);
	// par contre on doit bien attendre que se soit fini, soit :
	// on utilise des fences et on pourrait lancer plusieurs transferts en meme temps et attendre qu'ils aient tous fini
	// soit on sait que le transfert est fini avec vkQueueWaitIdle

	vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
}

void VulkanApp::transitionImageLayout(VkCommandPool commandPool, VkQueue queue,
				      VkImage image, VkFormat format, uint32_t mipLevels,
				      VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

	barrier.oldLayout = oldLayout; // VK_IMAGE_LAYOUT_UNDEFINED, si on s'occupe pas de ce qu'il y a avant
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		// on passe de l'état precedent (peut importe) au trasfert

		barrier.srcAccessMask = 0;			      // met met le masque a 0
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // on va écrire

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // on peut écrire sans attendre une étape
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;    // le transfert s'effectur a cette pseudo etape de la pipeline
							      // pas une vraie étape du pipeline graphique

	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		// on veut passer dans l'état pret pour lecture par un shader
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // sera lu au niveau du fragments hader

	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer,
			     srcStage, dstStage,
			     0,
			     0, nullptr,
			     0, nullptr,
			     1, &barrier);

	endSingleTimeCommands(commandBuffer, commandPool, queue);
}

void VulkanApp::copyBufferToImage(VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
	    width,
	    height,
	    1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer, commandPool, queue);
}

VkImageView VulkanApp::createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	// peut etre 1d 2D 3d cubemaps, définit comme les donnes doivent etre interprété

	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	// défini le but de nos images, ici se sera seulement pour ecrire les couleurs
	// sans mapmapping ou différentes "couches"

	/*
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // default mapping
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	// on peut mettre tous les channel vers le channel rouge pour des textures monochrome
	*/

	VkImageView imageView;
	if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

void VulkanApp::createTextureImageView() {
	m_textureImageView = createImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, m_mipLevels, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanApp::createTextureImageSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR; // magFileter = oversampling quand checkered pattern tres rapproché par exemple
	samplerInfo.minFilter = VK_FILTER_LINEAR; // undersampling

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	// pour chaque axe on peut définir comme les uv font se comporter quand on dépasse 0, 1

	samplerInfo.anisotropyEnable = VK_TRUE;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
	// recup les info du gpu pour connaitre la valeur max du filtrage
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // couleur quand on sample en dehors de l'image

	samplerInfo.unnormalizedCoordinates = VK_FALSE; // si true coord u : [0, width-1] ; v : [0, height-1]

	samplerInfo.compareEnable = VK_FALSE; // utilise pour shadow maps : "percentage-closer filtering"
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // == 1000.0f permet d'utiliser toutes les mipmaps
	// mipmapping

	if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler)) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

VkFormat VulkanApp::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {

	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format");
}

VkFormat VulkanApp::findDepthFormat() {
	return findSupportedFormat(
	    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
	    VK_IMAGE_TILING_OPTIMAL,
	    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VulkanApp::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanApp::createDepthResources() {

	VkFormat depthFormat = findDepthFormat();

	createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat, 1,
		    VK_SHARING_MODE_EXCLUSIVE,
		    VK_IMAGE_TILING_OPTIMAL,
		    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

	m_depthImageView = createImageView(m_depthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanApp::loadMesh() {
	m_mesh.loadMesh(g_model_path);
}

void VulkanApp::generateMipmaps(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
	// on regarde si le format support le linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				     0, nullptr,
				     0, nullptr,
				     1, &barrier); // permet d'attendre que le transfert avec copy ou la mipmap soit fini avant de continuer et de pouvoir écrire

		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		// on va prendre la taille de la texture du miplevel precendent i-1

		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {
		    mipWidth > 1 ? mipWidth / 2 : 1,
		    mipHeight > 1 ? mipHeight / 2 : 1,
		    1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		// on va "écrire" le miplevel i en fonction des données du miplevel precedent

		vkCmdBlitImage(commandBuffer, // queue a besoin de la queue graphics
			       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &blit,
			       VK_FILTER_LINEAR); // interpolation

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, // attend que blit termine avant de sample
				     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				     0, nullptr,
				     0, nullptr,
				     1, &barrier);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	// on transitionne en mode shader read le dernier mipLevel
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
			     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			     0, nullptr,
			     0, nullptr,
			     1, &barrier);

	endSingleTimeCommands(commandBuffer, commandPool, queue);
}