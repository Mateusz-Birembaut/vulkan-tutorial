
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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>


void VulkanApp::initWindow() {
	glfwInit(); // init la lib

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ne pas créer de context openGL

	m_window = glfwCreateWindow(g_screen_width, g_screen_height, "Vulkan App", nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);

	glfwSetFramebufferSizeCallback(m_window, VulkanApp::framebufferResizeCallback);

	glfwSetCursorPosCallback(m_window, VulkanApp::cursorPosCallback );
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 

	//glfwSetKeyCallback(m_window, VulkanApp::keyCallback);

}

void VulkanApp::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	if(!app->m_focus) return;

	float offsetX = app->lastX - xpos;
	float offsetY = app->lastY - ypos;
	app->lastX = xpos;
	app->lastY = ypos;
	Camera& cam = app->m_camera;

	float yawDelta = glm::radians(offsetX * cam.getSensitivity());
	float pitchDelta = glm::radians(offsetY * cam.getSensitivity());

	glm::quat qYaw = glm::angleAxis(yawDelta, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat qPitch = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

	glm::quat finalRotation = qYaw * cam.getRotation() * qPitch;

	cam.setRotation(finalRotation);
}

void VulkanApp::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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
	if (glfwGetKey(m_window, GLFW_KEY_ESCAPE)) {
		m_focus = false;
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 
	}
	if (glfwGetKey(m_window, GLFW_KEY_ENTER)) {
		m_focus = true;
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
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

	m_commandManager.init(&m_context, g_max_frames_in_flight);

	createUniformBuffer(g_max_frames_in_flight);
	createObjectsSSBOs(g_max_frames_in_flight);
	createImageStorage(g_max_frames_in_flight);
	createLightSSBOs(g_max_frames_in_flight);

	m_compDescriptor.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_objSSBOs, m_lightSSBOs, m_storageImages);

	m_compPipeline.init(&m_context, m_compDescriptor.getSetLayout());

	//m_mesh.init(&m_context, &m_commandManager , g_model_path);

	m_syncObjects.init(&m_context, g_max_frames_in_flight);
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

void VulkanApp::createObjectsSSBOs(uint32_t max_frames_in_flight){

	const float PI = 3.14159265358979323846f;
	float halfWidth = 3.0f;   // half width of the box
	float halfHeight = 3.0f;  // half height
	float depth = 6.0f;       // half depth (used to position back wall)

	// Back wall (blue)
	Object back;
	back.geometry = glm::vec4(0.0f, 0.0f, -depth, 1.0f); // type = 1.0 -> quad
	back.rotation = glm::vec4(0.0f, 0.0f, 0.0f, 32.0f); // rotation.xyz, w = shininess
	back.scale = glm::vec4(halfWidth, halfHeight, 0.0f, 0.0f); // half-size x,y
	back.materialInfo = glm::vec4(0.0f, 0.2f, 1.0f, 0.0f);
	m_objs.push_back(back);

	// Floor (dark gray)
	Object floorObj;
	floorObj.geometry = glm::vec4(0.0f, -halfHeight, -depth/2.0f, 1.0f);
	floorObj.rotation = glm::vec4(-PI*0.5f, 0.0f, 0.0f, 32.0f); // rotate so normal = +Y, shininess
	floorObj.scale = glm::vec4(halfWidth, depth * 0.5f, 0.0f, 0.0f); // span width x depth (half-sizes expected by shader)
	floorObj.materialInfo = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
	m_objs.push_back(floorObj);

	// Ceiling (light gray)
	Object ceilObj;
	ceilObj.geometry = glm::vec4(0.0f, halfHeight, -depth/2.0f, 1.0f);
	ceilObj.rotation = glm::vec4(PI*0.5f, 0.0f, 0.0f, 32.0f); // normal = -Y, shininess
	ceilObj.scale = glm::vec4(halfWidth, depth * 0.5f, 0.0f, 0.0f);
	ceilObj.materialInfo = glm::vec4(0.9f, 0.9f, 0.9f, 0.0f);
	m_objs.push_back(ceilObj);

	// Left wall (red)
	Object left;
	left.geometry = glm::vec4(-halfWidth, 0.0f, -depth/2.0f, 1.0f);
	left.rotation = glm::vec4(0.0f, +PI*0.5f, 0.0f, 32.0f); // normal = -X, shininess
	left.scale = glm::vec4(depth * 0.5f, halfHeight, 0.0f, 0.0f); // span depth x height (half-sizes expected)
	left.materialInfo = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	m_objs.push_back(left);

	// Right wall (green)
	Object right;
	right.geometry = glm::vec4(halfWidth, 0.0f, -depth/2.0f, 1.0f);
	right.rotation = glm::vec4(0.0f, -PI*0.5f, 0.0f, 32.0f); // normal = +X, shininess
	right.scale = glm::vec4(depth * 0.5f, halfHeight, 0.0f, 0.0f);
	right.materialInfo = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	m_objs.push_back(right);

	// Central sphere (type = 0, yellow)
	Object sphereObj;
	sphereObj.geometry = glm::vec4(0.0f, 0.0f, -depth/2.0f, 0.0f); // type 0 = sphere
	sphereObj.scale = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // w = radius
	sphereObj.rotation = glm::vec4(0.0f, 0.0f, 0.0f, 32.0f); // shininess
	sphereObj.materialInfo = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
	m_objs.push_back(sphereObj);

	m_objSSBOs.clear();
	m_objSSBOs.resize(max_frames_in_flight);

	for (int i{0}; i < max_frames_in_flight; ++i) {
		VkDeviceSize ssboSphereSize = m_objs.size() * sizeof(Object);  
		m_objSSBOs[i].init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		setBufferName(&m_context, m_objSSBOs[i].get(), "SSBO sphere");


		Buffer stagingBuffer;
		stagingBuffer.init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setBufferName(&m_context, stagingBuffer.get(), "SpheresStagingBuffer");
		stagingBuffer.write(m_objs.data(), ssboSphereSize, 0);
		stagingBuffer.unmap();

		stagingBuffer.copyTo(m_objSSBOs[i].get(), ssboSphereSize);
		stagingBuffer.cleanup();	
	}
}

void VulkanApp::createLightSSBOs(uint32_t max_frames_in_flight){
/* 
	// directional sun light
	Light sun;
	sun.positionAndType = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); 
	sun.directionAndRange = glm::vec4(0.0f, 0.0f, -1.0f, 1000.0f); 
	sun.colorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);

	m_lights.push_back(sun);
 */

	Light point;
	point.positionAndType = glm::vec4(0.0f, 2.0f, -3.0f, 1.0f); 
	point.directionAndRange = glm::vec4(0.0f, 0.0f, 0.0f, 50.0f);
	point.colorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 2.0f); 
	m_lights.push_back(point);


	Light p2;
	p2.positionAndType = glm::vec4(-2.5f, -2.5f, -2.5f, 1.0f); 
	p2.directionAndRange = glm::vec4(0.0f, 0.0f, 0.0f, 10.0f);
	p2.colorAndIntensity = glm::vec4(1.0f, 0.9f, 0.7f, 2.0f); 
	m_lights.push_back(p2);


	m_lightSSBOs.clear();
	m_lightSSBOs.resize(max_frames_in_flight);

	for (int i{0}; i < max_frames_in_flight; ++i) {
		VkDeviceSize ssboSphereSize = m_lights.size() * sizeof(Light);  
		m_lightSSBOs[i].init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		setBufferName(&m_context, m_lightSSBOs[i].get(), "SSBO light");


		Buffer stagingBuffer;
		stagingBuffer.init(&m_context, &m_commandManager, ssboSphereSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		setBufferName(&m_context, stagingBuffer.get(), "LightsStagingBuffer");
		stagingBuffer.write(m_lights.data(), ssboSphereSize, 0);
		stagingBuffer.unmap();

		stagingBuffer.copyTo(m_lightSSBOs[i].get(), ssboSphereSize);
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

	m_compDescriptor.init(&m_context, g_max_frames_in_flight, m_uniformBuffers, m_objSSBOs, m_lightSSBOs, m_storageImages);

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
	for (auto& img : m_storageImages) {
		img.cleanup();
	}
	m_compDescriptor.cleanup();
}

void VulkanApp::cleanup() {	

    vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapChain();

	for (size_t i = 0; i < g_max_frames_in_flight; i++) {
		m_uniformBuffers[i].cleanup();
	}

	for (auto& ssbo : m_objSSBOs) {
		ssbo.cleanup();
	}

	for (auto& ssbo : m_lightSSBOs) {
		ssbo.cleanup();
	}

	m_syncObjects.cleanup();

	m_commandManager.cleanup();

	m_compPipeline.cleanup();

	m_context.cleanup();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}
