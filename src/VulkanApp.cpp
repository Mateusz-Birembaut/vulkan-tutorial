
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // depth range [0, 1] au lieu de [-1, 1]

#include <VulkanApp/VulkanApp.h>

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Utils/Utility.h>
#include <VulkanApp/Resources/Buffer.h>
#include <VulkanApp/Utils/Uniforms.h>
#include <VulkanApp/RayTracing/Ray.h>
#include <VulkanApp/RayTracing/Intersection.h>

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
	if(m_context.getComputeQueue() != VK_NULL_HANDLE && m_context.getComputeQueue() != m_context.getGraphicsQueue()){
		m_useCompute = true;
	}else {
		m_useCompute = false;
	}

	m_swapchain.init(&m_context, m_window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	//m_renderPass.init(&m_context, &m_swapchain);

	m_commandManager.init(&m_context, g_max_frames_in_flight);

	createUniformBuffer(g_max_frames_in_flight);
	createSphereSSBOs(g_max_frames_in_flight);
	createImageStorage(g_max_frames_in_flight);

	//m_color = Image::createColorAttachment(&m_context, m_swapchain);
	//m_depth = Image::createDepthAttachment(&m_context, m_swapchain, m_renderPass);
	//m_texture = Image::createTextureFromFile(&m_context, &m_commandManager, g_texture_path, true, true);

	// rasterization
	//m_descriptors.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_texture.getView(), m_texture.getSampler());
	//m_pipeline.init(&m_context, m_renderPass.get(), m_descriptors.getSetLayout());

	m_compDescriptor.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_sphereSSBOs, m_storageImages);

	m_compPipeline.init(&m_context, m_compDescriptor.getSetLayout());

	//m_mesh.init(&m_context, &m_commandManager , g_model_path);

	m_syncObjects.init(&m_context, g_max_frames_in_flight);


    //m_swapchain.createFrameBuffers(m_renderPass.get(), m_depth.getView(), m_color.getView());
}

void VulkanApp::createUniformBuffer(uint32_t max_frames_in_flight) {

	VkDeviceSize bufferSize = sizeof(CameraUBO);

	m_uniformBuffers.clear();
	m_uniformBuffers.resize(max_frames_in_flight);

	for (int i{0}; i < max_frames_in_flight; ++i) {
		Buffer& uniformBuffer = m_uniformBuffers[i];
		uniformBuffer.init(&m_context, &m_commandManager ,bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setBufferName(&m_context, m_uniformBuffers[i].get(), "UniformBuffer");
		uniformBuffer.map();
	}
}

void VulkanApp::createSphereSSBOs(uint32_t max_frames_in_flight){

	Sphere s;
	s.geometry = glm::vec4(0.0f, 0.0f, -5.0f, 1.0f); // Pos (0,0,-5), Rayon 1.0
	s.materialInfo = glm::vec4(1.0f, 0.0f, 0.0f, static_cast<float>(2)); // Rouge, Index 2

	m_spheres.push_back(s);

	m_sphereSSBOs.clear();
	m_sphereSSBOs.resize(max_frames_in_flight);

	for (int i{0}; i < max_frames_in_flight; ++i) {
		VkDeviceSize ssboSphereSize = m_spheres.size() * sizeof(Sphere);  
		m_sphereSSBOs[i].init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		setBufferName(&m_context, m_sphereSSBOs[i].get(), "SSBO sphere");


		Buffer stagingBuffer;
		stagingBuffer.init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setBufferName(&m_context, stagingBuffer.get(), "SpheresStagingBuffer");
		stagingBuffer.write(m_spheres.data(), ssboSphereSize, 0);
		stagingBuffer.unmap();

		stagingBuffer.copyTo(m_sphereSSBOs[i].get(), ssboSphereSize);
		stagingBuffer.cleanup();	
	}
}

void VulkanApp::createImageStorage(uint32_t max_frames_in_flight){

	m_storageImages.clear();
	m_storageImages.resize(max_frames_in_flight);

	for (int i{0}; i < max_frames_in_flight; ++i) {
		m_storageImages[i] = Image::createImageStorage(&m_context, &m_commandManager, m_swapchain);
	}
}


void VulkanApp::insertImageBarrier(VkCommandBuffer cmd, VkImage image, 
                                   VkAccessFlags srcAccess, VkAccessFlags dstAccess, 
                                   VkImageLayout oldLayout, VkImageLayout newLayout, 
                                   VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    
    barrier.image = image;
    
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1; 
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;


    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}

void VulkanApp::recordCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex) {

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;		      // optional
	beginInfo.pInheritanceInfo = nullptr; // optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	auto currentStorageImg = m_storageImages[m_currentFrame].getImage();
	auto currentSwapchainImg = m_swapchain.getImages()[imageIndex];

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compPipeline.get());

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compPipeline.getLayout(), 0, 1, &m_compDescriptor.getSets()[m_currentFrame], 0, nullptr);

	auto extent = m_swapchain.getExtent();
	uint32_t workgroupSize = 16; // must match shader
	uint32_t groupCountX = (extent.width + workgroupSize - 1) / workgroupSize;
	uint32_t groupCountY = (extent.height + workgroupSize - 1) / workgroupSize;
	uint32_t groupCountZ = 1;

	vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

	// trasition storage image from general to src copy
	insertImageBarrier(commandBuffer, currentStorageImg,
		VK_ACCESS_SHADER_WRITE_BIT, // we wait for shader to finish writting
		VK_ACCESS_TRANSFER_READ_BIT, // we want to read to transfert data 
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // we want to take data from here
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT); // ready for transfert

	// transition swap chain img from undefined to dst copy
	insertImageBarrier(commandBuffer, currentSwapchainImg,
		0, 
		VK_ACCESS_TRANSFER_WRITE_BIT, // we want to read to transfert data 
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // we want to take write data here
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // synchronise with the end of compute shader
		VK_PIPELINE_STAGE_TRANSFER_BIT); // ready for transfert

	// copy img
	VkImageCopy copyRegion{};
    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.extent = {extent.width, extent.height, 1};

	vkCmdCopyImage(commandBuffer,
        currentStorageImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        currentSwapchainImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion);

	// transition storage image 
	insertImageBarrier(commandBuffer, currentStorageImg, 
		VK_ACCESS_TRANSFER_READ_BIT, // wait for read (copy)
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);

	// transition swap chain image from to dst copy to present 
	insertImageBarrier(commandBuffer, currentSwapchainImg, 
		VK_ACCESS_TRANSFER_WRITE_BIT, // wait for write (copy)
		0,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // ready to present to screen
		VK_PIPELINE_STAGE_TRANSFER_BIT, 
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
	);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage) {
	static auto startTime{std::chrono::high_resolution_clock::now()};
	auto currentTime{std::chrono::high_resolution_clock::now()};
	float time{std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count()};

	float aspectRatio = m_swapchain.getExtent().width / static_cast<float>(m_swapchain.getExtent().height);
	CameraUBO cameraUBO{};
	cameraUBO.position = glm::vec4(m_camera.getPosition(), 1.0f);
	cameraUBO.direction = glm::vec4(m_camera.getForward(), 0.0f);
	cameraUBO.far = m_camera.getFar();
	cameraUBO.near = m_camera.getNear();
	cameraUBO.fov = glm::radians(m_camera.getFOV());
	cameraUBO.aspectRatio =  aspectRatio;
	cameraUBO.viewProjInv = m_camera.getViewProjInv(aspectRatio);

	memcpy(m_uniformBuffers[currentImage].getMapped(), &cameraUBO, sizeof(CameraUBO));

}

void VulkanApp::recreateSwapChain() {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapChain();

	m_swapchain.recreate(m_window);

	createImageStorage(g_max_frames_in_flight);

	m_compDescriptor.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_sphereSSBOs, m_storageImages);

}

void VulkanApp::drawFrame() {
	// wait for previous frame to "render"
	auto flightFence =  m_syncObjects.getInFlightFences()[m_currentFrame];
	auto imgSemaphore = m_syncObjects.getImgAvailableSemaphores()[m_currentFrame];
	auto renderSemaphore = m_syncObjects.getRenderFinishedSemaphores()[m_currentFrame];

	vkWaitForFences(m_context.getDevice(), 1, &flightFence, VK_TRUE, UINT64_MAX); // blocks host 

	// acquire image from the swapchain
	uint32_t imageIndex; 
	VkResult result = vkAcquireNextImageKHR(m_context.getDevice(), m_swapchain.getSwapChain(), UINT64_MAX, imgSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	updateUniformBuffer(m_currentFrame);

	vkResetFences(m_context.getDevice(), 1, &m_syncObjects.getInFlightFences()[m_currentFrame]); // on débloque l'éxécution manuellement, ici pour eviter deadlock
	// on est sur d'avoir une image a draw
	// si on reset et que recreate swap chain est appelé, alors on sera toujours
	// sur la fence m_inFlightFences[m_currentFrame] et on sera bloqué par vkWaitForFences

	// record un command buffer pour draw sur l'image
	vkResetCommandBuffer(m_commandManager.getCommandBuffers()[m_currentFrame], 0);
	recordCommands(m_commandManager.getCommandBuffers()[m_currentFrame], imageIndex);

	// submit l'image
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[]{imgSemaphore};
	VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}; // wait before starting to compute,  use VK_PIPELINE_STAGE_TRANSFER_BIT maybe 
	// on veut attendre au niveau de l'écriture dans le frame buffer
	// ca veut dire que le gpu peut executer des shaders juste on écrit pas encore
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandManager.getCommandBuffers()[m_currentFrame];
	// les buffers a submit pour execution

	VkSemaphore signalSemaphores[] = {renderSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	// on va signal ce sémaphore apres que le command buffer soit executé

	if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, flightFence) != VK_SUCCESS) {
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
	// Clean storage images and descriptors as they depend on swapchain size
	for (auto& img : m_storageImages) {
		img.cleanup();
	}
	m_compDescriptor.cleanup();
	// m_color.cleanup(); // not used in compute mode
	// m_depth.cleanup(); // not used in compute mode
}

void VulkanApp::cleanup() {	

    vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapChain();

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		m_uniformBuffers[i].cleanup();
	}

	for (auto& ssbo : m_sphereSSBOs) {
		ssbo.cleanup();
	}

	// Storage images and descriptors are cleaned in cleanupSwapChain()
	// for (auto& img : m_storageImages) {
	//     img.cleanup();
	// }
	// m_compDescriptor.cleanup();

	m_syncObjects.cleanup();

	m_commandManager.cleanup();

	m_compPipeline.cleanup();

	m_context.cleanup();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

/* 
// uniform buffers are persistent so we keep the buffer map
void VulkanApp::createUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	m_uniformBuffers.resize(g_max_frames_in_flight);

	for (int i{0}; i < g_max_frames_in_flight; ++i) {
		Buffer& uniformBuffer = m_uniformBuffers[i];
		uniformBuffer.init(&m_context, &m_commandManager ,bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setBufferName(&m_context, m_uniformBuffers[i].get(), "UniformBuffer");
		uniformBuffer.map();
	}
}



void VulkanApp::recordRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
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
	auto meshBuffer = m_mesh.getBuffer().get();
	VkBuffer vertexBuffers[]{meshBuffer};
	VkDeviceSize offsets[]{0};

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, meshBuffer, m_mesh.getIndicesOffset(), VK_INDEX_TYPE_UINT32);
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


void VulkanApp::drawFrame() {
	// attendre frame precedente fini
	auto flightFence =  m_syncObjects.getInFlightFences()[m_currentFrame];
	auto imgSemaphore = m_syncObjects.getImgAvailableSemaphores()[m_currentFrame];
	auto renderSemaphore = m_syncObjects.getRenderFinishedSemaphores()[m_currentFrame];

	vkWaitForFences(m_context.getDevice(), 1, &flightFence, VK_TRUE, UINT64_MAX); // bloque le cpu (host)
	// prend un array de fence, ici 1 seule, VK_TRUE, on attend toutes les fences
	// UINT64_MAX en gros desactive le timeout tellement c grand

	// prendre une image de la swapchain
	uint32_t imageIndex; // index de la vkimagedans le swap chain images
	VkResult result = vkAcquireNextImageKHR(m_context.getDevice(), m_swapchain.getSwapChain(), UINT64_MAX, imgSemaphore, VK_NULL_HANDLE, &imageIndex);
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

	vkResetFences(m_context.getDevice(), 1, &m_syncObjects.getInFlightFences()[m_currentFrame]); // on débloque l'éxécution manuellement, ici pour eviter deadlock
	// on est sur d'avoir une image a draw
	// si on reset et que recreate swap chain est appelé, alors on sera toujours
	// sur la fence m_inFlightFences[m_currentFrame] et on sera bloqué par vkWaitForFences

	// record un command buffer pour draw sur l'image
	vkResetCommandBuffer(m_commandManager.getCommandBuffers()[m_currentFrame], 0);
	recordRenderCommands(m_commandManager.getCommandBuffers()[m_currentFrame], imageIndex);

	// submit l'image
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[]{imgSemaphore};
	VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// on veut attendre au niveau de l'écriture dans le frame buffer
	// ca veut dire que le gpu peut executer des shaders juste on écrit pas encore
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandManager.getCommandBuffers()[m_currentFrame];
	// les buffers a submit pour execution

	VkSemaphore signalSemaphores[] = {renderSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	// on va signal ce sémaphore apres que le command buffer soit executé

	if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, flightFence) != VK_SUCCESS) {
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
	m_color.cleanup();
	m_depth.cleanup();
}


void VulkanApp::recreateSwapChain() {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0) { // wait while minimized
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_context.getDevice());
	cleanupSwapChain();

	m_swapchain.recreate(m_window);
	m_color = Image::createColorAttachment(&m_context, m_swapchain);
	m_depth = Image::createDepthAttachment(&m_context, m_swapchain, m_renderPass);

	m_swapchain.createFrameBuffers(m_renderPass.get(), m_depth.getView(), m_color.getView());
}

void VulkanApp::cleanup() {	    // les queues sont détruites implicitement

    vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapChain();

	m_texture.cleanup();

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		m_uniformBuffers[i].cleanup();
	}

	m_mesh.cleanup();

	m_syncObjects.cleanup();

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
	ubo.model = glm::mat4(1.0f);

	ubo.view = m_camera.getViewMatrix();


	ubo.proj = glm::perspective(glm::radians(45.0f), m_swapchain.getExtent().width / static_cast<float>(m_swapchain.getExtent().height), 0.1f, 100.0f);

	ubo.proj[1][1] *= -1; // glm for opengl and y axis is inverted

	memcpy(m_uniformBuffers[currentImage].getMapped(), &ubo, sizeof(ubo));

}



 */