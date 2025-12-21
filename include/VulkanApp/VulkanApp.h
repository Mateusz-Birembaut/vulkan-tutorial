#pragma once

#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Core/SwapChain.h>

#include <VulkanApp/Rendering/Pipeline.h>
#include <VulkanApp/Rendering/RenderPass.h>
#include <VulkanApp/Rendering/Descriptors.h>
#include <VulkanApp/Commands/CommandManager.h>
#include <VulkanApp/Resources/Buffer.h>
#include <VulkanApp/Resources/Image.h>
#include <VulkanApp/Sync/SyncObjects.h>
#include <VulkanApp/RayTracing/Objects/Object.h>
#include <VulkanApp/Resources/Mesh.h>
#include <VulkanApp/Rendering/ComputeDescriptor.h>
#include <VulkanApp/Rendering/ComputePipeline.h>
#include <VulkanApp/RayTracing/Objects/Light.h>

#include "Camera.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

constexpr uint32_t g_screen_width{800};
constexpr uint32_t g_screen_height{600};

constexpr uint32_t g_max_frames_in_flight{2};

const std::string g_model_path = "Models/viking_room.obj";
const std::string g_texture_path = "Textures/viking_room.png";

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif // NDEBUG

class VulkanApp {
      public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

	void setResized(bool b) {
		m_framebufferResized = b;
	}

      private:

	GLFWwindow* m_window;

	VulkanContext m_context;
	SwapChain m_swapchain;
	ComputePipeline m_compPipeline;
	ComputeDescriptor m_compDescriptor;
	
	std::vector<Buffer> m_uniformBuffers;
	std::vector<Object> m_objs;
	std::vector<Buffer> m_objSSBOs; 
	std::vector<Image> m_storageImages;
	std::vector<Light> m_lights;
	std::vector<Buffer> m_lightSSBOs; 

	CommandManager m_commandManager;

	SyncObjects m_syncObjects;

	Camera m_camera{};

	bool m_useCompute;

	void initWindow();
	void initVulkan();

	void createUniformBuffer(uint32_t max_frames_in_flight);
	void createObjectsSSBOs(uint32_t max_frames_in_flight);
	void createLightSSBOs(uint32_t max_frames_in_flight);
	void createImageStorage(uint32_t max_frames_in_flight);

	void insertImageBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

	void recordCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void recreateSwapChain();
	void cleanupSwapChain();

	void updateUniformBuffer(uint32_t currentFrame);

	void recordRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void mainLoop();
	void drawFrame();

	void cleanup();


	int m_currentFrame{0};
	bool m_framebufferResized{false};// VK_ERROR_OUT_OF_DATE_KHR is not guarentee when resizing, it fixes that 
	float m_deltaTime{0.0f};
	double m_lastFrame{0.0};
	float lastX{400};
	float lastY{300};
	bool m_focus{false};


	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

	void processInput(float dt);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onKey(int key, int scancode, int action, int mods);

};
