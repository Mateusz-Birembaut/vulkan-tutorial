
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // depth range [0, 1] au lieu de [-1, 1]

#include <VulkanApp/VulkanApp.h>

#include <VulkanApp/Core/VulkanContext.h>

#include <VulkanApp/Resources/Buffer.h>
#include <VulkanApp/Utils/Uniforms.h>


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

	m_window = glfwCreateWindow(g_screen_width, g_screen_height, "Vulkan App", nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, VulkanApp::framebufferResizeCallback);

	glfwSetKeyCallback(m_window, VulkanApp::keyCallback);
}

void VulkanApp::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	//std::cout << "keyCallback: key=" << key << " action=" << action << " scancode=" << scancode << " mods=" << mods << '\n';
	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	if (app) app->onKey(key, scancode, action, mods);
}

void VulkanApp::onKey(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(m_window, GLFW_TRUE);
	}
}

void VulkanApp::processInput(float dt) {

	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_Z) == GLFW_PRESS) {
		m_camera.setPosition(m_camera.getPosition() + m_camera.getForward() * (m_camera.getSpeed() * dt));
	}
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) {
		m_camera.setPosition(m_camera.getPosition() - m_camera.getForward() * (m_camera.getSpeed() * dt));
	}
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) {
		m_camera.setPosition(m_camera.getPosition() + m_camera.getRight() * (m_camera.getSpeed() * dt));
	}
	if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) {
		m_camera.setPosition(m_camera.getPosition() - m_camera.getRight() * (m_camera.getSpeed() * dt));
	}
}

void VulkanApp::mainLoop() {

	while (!glfwWindowShouldClose(m_window)) {

		double current = glfwGetTime();
		m_deltaTime = static_cast<float>(current - m_lastFrame);
		m_lastFrame = current;

		processInput(m_deltaTime);
		glfwPollEvents();
		drawFrame();
	}

	vkDeviceWaitIdle(m_context.getDevice());
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->setResized(true);
}


void VulkanApp::initVulkan() {

	m_context.init(m_window, true);
	m_swapchain.init(&m_context, m_window);
	m_renderPass.init(&m_context, &m_swapchain);

	m_commandManager.init(&m_context, g_max_frames_in_flight);

	createUniformBuffer();

	createTextureImage();
	createTextureImageView();
	createTextureImageSampler();

	m_descriptors.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_textureImageView, m_textureSampler);

	m_pipeline.init(&m_context, m_renderPass.get(), m_descriptors.getSetLayout());

	createColorRessources();
	createDepthResources();

	loadMesh();
	createMeshBuffer();

	createSyncObjects();

    m_swapchain.createFrameBuffers(m_renderPass.get(), m_depthImageView, m_colorImageView);
}


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


	Buffer stagingBuffer;
	stagingBuffer.init(&m_context, &m_commandManager, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	setObjectName(stagingBuffer.get(), "MeshStagingBuffer");
	stagingBuffer.write(m_mesh.verticesData(), verticesSize, 0);
	stagingBuffer.write(m_mesh.indicesData(), indicesSize, m_indicesOffset);
	stagingBuffer.unmap();

	m_mBuffer.init(&m_context, &m_commandManager, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	setObjectName(m_mBuffer.get(), "MeshBuffer");

	stagingBuffer.copyTo(m_mBuffer.get(), bufferSize);
	stagingBuffer.cleanup();
}

// uniform buffers are persistent so we keep the buffer map
void VulkanApp::createUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	m_uniformBuffers.resize(g_max_frames_in_flight);

	for (int i{0}; i < g_max_frames_in_flight; ++i) {
		Buffer& uniformBuffer = m_uniformBuffers[i];
		uniformBuffer.init(&m_context, &m_commandManager ,bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setObjectName(m_uniformBuffers[i].get(), "UniformBuffer");
		uniformBuffer.map();
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
	renderPassInfo.renderPass = m_renderPass.get();
	renderPassInfo.framebuffer = m_swapchain.getFramebuffers()[imageIndex];
	// on bind sur quelle swapchainFramebuffer on va écrire (qui est est lui meme relié a une swap chain image)

	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = m_swapchain.getExtent();
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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.get());
	// VK_PIPELINE_BIND_POINT_GRAPHICS, c'est une pipeline de rendu

	VkBuffer vertexBuffers[]{m_mBuffer.get()};
	VkDeviceSize offsets[]{0};

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, m_mBuffer.get(), m_indicesOffset, VK_INDEX_TYPE_UINT32);
	// VK_INDEX_TYPE_UINT32 si on veut plus d'indices, mais dans ce cas modif vecteur d'indices aussi

	// comme on a défini le viewport et scissor dynamiquement on doit les spécifier ici

	VkViewport viewport{}; // la région de l'output buffer dans laquelle on écrit ex:
	// si c'est 0,0 à width height alors on écrit dans tout le buffer
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchain.getExtent().width);
	viewport.height = static_cast<float>(m_swapchain.getExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{}; // seuls les pixels dans cette région seront rendu
	// reste ignoré.
	scissor.offset = {0, 0};
	scissor.extent = m_swapchain.getExtent();
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// la command pour draw 1 triangle de 3 index :
	// vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
	// avant dernier : offset dans le vertexbuffer, défini le vertex index le plus petit
	// dernier : ofsset pour les instanced rendering, def le + petit

	vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_pipeline.getLayout(),
				0, 1, &m_descriptors.getSets()[m_currentFrame], 0, nullptr);

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
		if (vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
		    vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
		    vkCreateFence(m_context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}
	std::cout << "Sync objects created" << '\n';
}

void VulkanApp::drawFrame() {
	// attendre frame precedente fini
	vkWaitForFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX); // bloque le cpu (host)
	// prend un array de fence, ici 1 seule, VK_TRUE, on attend toutes les fences
	// UINT64_MAX en gros desactive le timeout tellement c grand

	// prendre une image de la swapchain
	uint32_t imageIndex; // index de la vkimagedans le swap chain images
	VkResult result = vkAcquireNextImageKHR(m_context.getDevice(), m_swapchain.getSwapChain(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
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

	vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]); // on débloque l'éxécution manuellement, ici pour eviter deadlock
	// on est sur d'avoir une image a draw
	// si on reset et que recreate swap chain est appelé, alors on sera toujours
	// sur la fence m_inFlightFences[m_currentFrame] et on sera bloqué par vkWaitForFences

	// record un command buffer pour draw sur l'image
	vkResetCommandBuffer(m_commandManager.getCommandBuffers()[m_currentFrame], 0);
	recordCommandBuffer(m_commandManager.getCommandBuffers()[m_currentFrame], imageIndex);

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
	submitInfo.pCommandBuffers = &m_commandManager.getCommandBuffers()[m_currentFrame];
	// les buffers a submit pour execution

	VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	// on va signal ce sémaphore apres que le command buffer soit executé

	if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// presenter l'image a la swap chain image pour affichage
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	// quels semaphores on va attendre avant de presenter l'image
	// tant que l'image n'est pas rendu, on att

	VkSwapchainKHR swapChains[] = {m_swapchain.getSwapChain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;
	// a quel swap chain on va presenter les images
	// et quelles images (indices) le plus souvent 1 seule

	presentInfo.pResults = nullptr; // optional
	// permet de spécifier un array Vkresult
	// pour savoir si la présentation a marché dans chaque swap chain

	result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);
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

	m_swapchain.cleanup();

	vkDestroyImageView(m_context.getDevice(), m_colorImageView, nullptr);
	vkDestroyImage(m_context.getDevice(), m_colorImage, nullptr);
	vkFreeMemory(m_context.getDevice(), m_colorImageMemory, nullptr);

	vkDestroyImageView(m_context.getDevice(), m_depthImageView, nullptr);
	vkDestroyImage(m_context.getDevice(), m_depthImage, nullptr);
	vkFreeMemory(m_context.getDevice(), m_depthImageMemory, nullptr);


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

	vkDeviceWaitIdle(m_context.getDevice());

	m_swapchain.recreate(m_window);

	// les images views doivent etre recrée car sont lies au dimensions des images de la swapchain
	createColorRessources();
	createDepthResources();

	m_swapchain.createFrameBuffers(m_renderPass.get(), m_depthImageView, m_colorImageView);
}

void VulkanApp::cleanup() {	    // les queues sont détruites implicitement

    vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapChain();

	vkDestroySampler(m_context.getDevice(), m_textureSampler, nullptr);
	vkDestroyImageView(m_context.getDevice(), m_textureImageView, nullptr);
	vkDestroyImage(m_context.getDevice(), m_textureImage, nullptr);
	vkFreeMemory(m_context.getDevice(), m_textureImageMemory, nullptr);

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		m_uniformBuffers[i].cleanup();
	}

	m_mBuffer.cleanup();


	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		vkDestroySemaphore(m_context.getDevice(), m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_context.getDevice(), m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_context.getDevice(), m_inFlightFences[i], nullptr);
	}

	m_commandManager.cleanup();

	m_pipeline.cleanup();

	m_descriptors.cleanup();

	m_renderPass.cleanup();

	m_context.cleanup();

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

	ubo.view = m_camera.getViewMatrix();
	// caméra en hauter et qui regarde en 0,0,0, up de la camera en 0,0,1

	ubo.proj = glm::perspective(glm::radians(45.0f), m_swapchain.getExtent().width / static_cast<float>(m_swapchain.getExtent().height), 0.1f, 10.0f);
	// camera d'un fov de 45, avec la taille = a celle de nos images et un near plan à 0.1F et far a 10.0f

	ubo.proj[1][1] *= -1; // car glm pour OpenGL et l'axe y est inversé par rapport a vulkan

	memcpy(m_uniformBuffers[currentImage].getMapped(), &ubo, sizeof(ubo));
	// m_uniformBuffersMapped adresse accessible ou vont être stockées les données de l'ubo
}


void VulkanApp::setObjectName(VkBuffer buffer, const char* name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(buffer);
	nameInfo.pObjectName = name;
	auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_context.getDevice(), "vkSetDebugUtilsObjectNameEXT");
	if (func)
		func(m_context.getDevice(), &nameInfo);
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

	Buffer stagingBuffer;
	
	stagingBuffer.init(&m_context, &m_commandManager,imgSize,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_SHARING_MODE_EXCLUSIVE,
	    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);


	setObjectName(stagingBuffer.get(), "ImageStagingBuffer");

	stagingBuffer.map();
	stagingBuffer.write(pixels, imgSize, 0);
	stagingBuffer.unmap();

	stbi_image_free(pixels);

	createImage(texWidth, texHeight,
		    VK_FORMAT_R8G8B8A8_SRGB /*4 int8 pour chaque pixels */, m_mipLevels,
		    VK_SAMPLE_COUNT_1_BIT,
		    VK_SHARING_MODE_CONCURRENT, // besoin de la queue transfer et graphics comme j'ai les deux dans deux queues différentes
		    VK_IMAGE_TILING_OPTIMAL,	// ici pour avoir un accès le plus efficace possible
		    // tiling linéaire row major order
		    VK_IMAGE_USAGE_TRANSFER_SRC_BIT /*l'image servira de source et destinaation pour les transfert car on va generer les mipmaps*/ | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // on veut pouvoir transferer des données, et l'utiliser comme sampler
		    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,																			   // stocker de manière a avoir un accès rapide
		    m_textureImage, m_textureImageMemory);

	// modifier l'état de l'image en gros pour effectuer certaines opérations ici
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL pour copier les données
	transitionImageLayout(m_commandManager.getTransferCommandPool(), m_context.getTransferQueue(), m_textureImage,
			      VK_FORMAT_R8G8B8A8_SRGB, m_mipLevels,
			      VK_IMAGE_LAYOUT_UNDEFINED /*on s'occupe pas de l'état precedent*/,
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyBufferToImage(m_commandManager.getTransferCommandPool(), m_context.getTransferQueue(), stagingBuffer.get(), m_textureImage, texWidth, texHeight);

	// une
	generateMipmaps(m_commandManager.getCommandPool(), m_context.getGraphicsQueue(), m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_mipLevels);

	stagingBuffer.cleanup();
}

void VulkanApp::createImage(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels,
			    VkSampleCountFlagBits numSamples, VkSharingMode sharingMode,
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
	imageInfo.samples = numSamples;

	auto indices = m_context.getQueueFamilies();
	std::set<uint32_t> uniqueIndices{indices.transferFamily.value(), indices.graphicsFamily.value()};
	std::vector<uint32_t> queueIndices(uniqueIndices.begin(), uniqueIndices.end());
	if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
		imageInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueIndices.size());
		imageInfo.pQueueFamilyIndices = queueIndices.data(); //
	}

	imageInfo.flags = 0; // Optional, voir pour 3D voxel en grande partie vide ex => nuages

	if (vkCreateImage(m_context.getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_context.getDevice(), image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = m_context.findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_context.getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(m_context.getDevice(), image, imageMemory, 0);
}





void VulkanApp::transitionImageLayout(VkCommandPool commandPool, VkQueue queue,
				      VkImage image, VkFormat format, uint32_t mipLevels,
				      VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = m_commandManager.beginSingleTimeCommands(commandPool);

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

	m_commandManager.endSingleTimeCommands(commandBuffer, commandPool, queue);
}

void VulkanApp::copyBufferToImage(VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = m_commandManager.beginSingleTimeCommands(commandPool);

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

	m_commandManager.endSingleTimeCommands(commandBuffer, commandPool, queue);
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
	if (vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
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
	vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);
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

	if (vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_textureSampler)) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}




bool VulkanApp::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanApp::createDepthResources() {

	VkFormat depthFormat = m_renderPass.findDepthFormat();

	createImage(m_swapchain.getExtent().width, m_swapchain.getExtent().height , depthFormat, 1, m_context.getMsaaSamples(),
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
	vkGetPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = m_commandManager.beginSingleTimeCommands(commandPool);

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

	m_commandManager.endSingleTimeCommands(commandBuffer, commandPool, queue);
}


void VulkanApp::createColorRessources() {
	VkFormat colorFormat = m_swapchain.getImageFormat();

	createImage(m_swapchain.getExtent().width, m_swapchain.getExtent().height, colorFormat, 1, m_context.getMsaaSamples(),
		    VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_TILING_OPTIMAL,
		    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_colorImage, m_colorImageMemory);

	m_colorImageView = createImageView(m_colorImage, colorFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT);
}