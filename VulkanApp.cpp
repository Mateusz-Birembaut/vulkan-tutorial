#include "VulkanApp.h"
#include "FileReader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

void VulkanApp::initWindow() {
	glfwInit(); // init la lib

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ne pas créer de context openGL
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // désactive le resize parce que c'est relou en gros

	m_window = glfwCreateWindow(g_screen_width, g_screen_height, "Vulkan Triangle", nullptr, nullptr);
}

void VulkanApp::mainLoop() {

	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		drawFrame();
	}

	vkDeviceWaitIdle(m_device);
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
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageView();
	createRenderPass();
	createGraphicsPipeline();
	createFrameBuffers();
	createCommandPool();
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
	std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value()};

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

	bool swapChainAdequate{false};
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return queueFamily.isComplete() && extensionsSupported && swapChainAdequate;
}

void VulkanApp::createInstance() {

	if (!checkValidationLayerSupport() && enableValidationLayers)
		throw std::runtime_error("Validation layers requested are not available!");

	VkApplicationInfo appInfo{}; // techniquement optional, info sur l'app
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Triangle";
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

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions; // on donne les noms des extension et ca va les activer

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
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
	uint32_t queueFamilyIndices[]{indices.graphicsFamily.value(), indices.presentFamily.value()};

	if (indices.graphicsFamily != indices.presentFamily) { // si les deux queues sont séparé
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
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
	} else {
		std::cout << "Swap chain created" << std::endl;
	}

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
}

void VulkanApp::createImageView() {
	m_swapChainImageViews.resize(m_swapChainImages.size());
	for (size_t i = 0; i < m_swapChainImages.size(); i++) {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		// peut etre 1d 2D 3d cubemaps, définit comme les donnes doivent etre interprété
		imageViewCreateInfo.format = m_swapChainImageFormat;

		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // default mapping
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		// on peut mettre tous les channel vers le channel rouge pour des textures monochrome

		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		// défini le but de nos images, ici se sera seulement pour ecrire les couleurs
		// sans mapmapping ou différentes "couches"

		if (vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}
	std::cout << "all image views created" << std::endl;
}

void VulkanApp::createGraphicsPipeline() {
	auto vertShaderCode{FileReader::readSPV("Shaders/vert.spv")};
	auto fragShaderCode{FileReader::readSPV("Shaders/frag.spv")};

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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // pointeurs sur le array of structs
	// equivalent en opengl : taille des données
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;
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

	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;	// on cull classiquement
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // en Vulkan, avec viewport non inversé, l'enroulement visuel CCW devient CW

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
	pipelineLayoutInfo.setLayoutCount = 0;		  // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr;	  // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0;	  // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	// la on peut passer des variables dynamic a nos shader
	// par exemple utime je crois

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	} else {
		std::cout << "Pipeline layout created" << '\n';
	}

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

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	// peut etre un passe de compute donc on précise que c'est une de graphisme
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	// subpass.pPreserveAttachments, données qui doivent etre gardé

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // c'est la pass implicit avec la render pass
	dependency.dstSubpass = 0;		     // 0 c'est notre render pass

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // quoi attendre
	// on doit wait que la swap chain finisse de lire l'image avant qu'on y accède
	// pour ça, on attend au niveau de la color attachment stage
	dependency.srcAccessMask = 0; // a quel stage ca arrive

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// les opérations doivent attendre qu'on soit dans le stage color attachment
	// le rendu des pixels dans le framebuffer ne commence pas tant
	// qu'on est pas dans l'état ecriture en gros

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
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
		VkImageView attachments[] = {
		    m_swapChainImageViews[i],
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_swapChainExtent.width;
		framebufferInfo.height = m_swapChainExtent.height;
		framebufferInfo.layers = 1; // nos swapchain images ont 1 seule image

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFrameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}

	std::cout << "Frame buffers created" << '\n';
}

void VulkanApp::createCommandPool() {
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

	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;
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
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	// avant dernier : offset dans le vertexbuffer, défini le vertex index le plus petit
	// dernier : ofsset pour les instanced rendering, def le + petit

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
	vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]); // on débloque l'éxécution manuellement

	// prendre une image de la swapchain
	uint32_t imageIndex; // index de la vkimagedans le swap chain images
	vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
	// m_imageAvailableSemaphore signaled quand on a fini

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

	int x = 4;

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

	vkQueuePresentKHR(m_presentQueue, &presentInfo);

	m_currentFrame = (m_currentFrame+1) % g_max_frames_in_flight;
}

void VulkanApp::cleanup() { // les queues sont détruites implicitement
	
	vkDeviceWaitIdle(m_device); // pour attendre que toutes les opération vk soient terminées

	for (size_t i = 0; i < g_max_frames_in_flight; i++)
	{
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	for (auto frameBuffer : m_swapChainFrameBuffers) {
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);
	for (auto imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	vkDestroyDevice(m_device, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	glfwDestroyWindow(m_window);
	glfwTerminate();
}