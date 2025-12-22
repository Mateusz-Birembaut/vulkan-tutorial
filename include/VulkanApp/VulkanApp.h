#pragma once

#include <VulkanApp/Core/SwapChain.h>
#include <VulkanApp/Core/VulkanContext.h>

#include <VulkanApp/Commands/CommandManager.h>
#include <VulkanApp/RayTracing/Objects/Light.h>
#include <VulkanApp/RayTracing/Objects/Object.h>
#include <VulkanApp/Rendering/ComputeDescriptor.h>
#include <VulkanApp/Rendering/ComputePipeline.h>
#include <VulkanApp/Rendering/Descriptors.h>
#include <VulkanApp/Rendering/Pipeline.h>
#include <VulkanApp/Rendering/RenderPass.h>
#include <VulkanApp/Resources/Buffer.h>
#include <VulkanApp/Resources/Image.h>
#include <VulkanApp/Resources/Mesh.h>
#include <VulkanApp/Sync/SyncObjects.h>

#include "Camera.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWindow>
#include <QSet>
#include <QElapsedTimer>

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

class VulkanApp : public QWindow {
      public:
	VulkanApp();
	~VulkanApp();

	void init();

	void setResized(bool b) {
		m_framebufferResized = b;
	}

      protected:
	bool event(QEvent* e) override;
	void exposeEvent(QExposeEvent* event) override;

    // Entrées
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

      private:
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

	void initVulkan();

	void createUniformBuffer(uint32_t max_frames_in_flight);
	void createObjectsSSBOs(uint32_t max_frames_in_flight);
	void createLightSSBOs(uint32_t max_frames_in_flight);
	void createImageStorage(uint32_t max_frames_in_flight);

	void insertImageBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

	void recordCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void drawFrame();

	void recreateSwapChain();
	void cleanupSwapChain();

	void updateUniformBuffer(uint32_t currentFrame);

	void cleanup();

	int m_currentFrame{0};
	bool m_framebufferResized{false}; // VK_ERROR_OUT_OF_DATE_KHR is not guarentee when resizing, it fixes that
	QElapsedTimer m_timer;
	QSet<int> m_keysPressed;
	float lastX{400};
	float lastY{300};
	bool m_focus{false};

	void processInput(float dt);
	void handleMouseLook(float xpos, float ypos);
};
