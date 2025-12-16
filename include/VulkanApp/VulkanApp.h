#ifndef VULKAN_APP_H
#define VULKAN_APP_H


#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Core/SwapChain.h>

#include <VulkanApp/Rendering/Pipeline.h>
#include <VulkanApp/Rendering/RenderPass.h>

#include <VulkanApp/Resources/Mesh.h>

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

constexpr int g_max_frames_in_flight{2};

const std::string g_model_path = "Models/viking_room.obj";
const std::string g_texture_path = "Textures/viking_room.png";

/* const std::string g_vertex_shader = "Shaders/vert.spv";
const std::string g_fragment_shader = "Shaders/frag.spv"; */


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
		Pipeline m_pipeline;
		RenderPass m_renderPass;

	void initWindow();

	void initVulkan();
	//void createRenderPass();
	void createDescriptorSetLayout();
	//void createGraphicsPipeline();
	//void createFrameBuffers();
	void createCommandPools();
	void createColorRessources();
	void createDepthResources();
	void createTextureImage();
	void createTextureImageView();
	void createTextureImageSampler();
	void createUniformBuffer();
	void createDescriptorPool();
	void createDescriptorSets();
	void loadMesh();
	void createMeshBuffer();

	void createGraphicsCommandBuffers();
	void createTransferCommandBuffer();
	void createSyncObjects();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createImage(uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkSharingMode sharingMode, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void copyBufferToImage(VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void createCommandBuffers();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue);
	void transitionImageLayout(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propreties);

	void recreateSwapChain();

	void updateUniformBuffer(uint32_t currentFrame);
	void drawFrame();

	void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);



	void setupDebugMessenger(VkDevice device);

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	void mainLoop();

	void cleanup();
	void cleanupSwapChain();

	bool hasStencilComponent(VkFormat format);
	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	
	//QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	//SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);



	//VkShaderModule createShaderModule(const std::vector<char>& code);

	VkImageView createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags);

	void generateMipmaps(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);




	
	VkDescriptorSetLayout m_descriptorSetLayout;
/* 	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline; */

	VkCommandPool m_commandPool;
	VkCommandPool m_commandPoolTransfer;

	Mesh m_mesh;
	VkBuffer m_meshBuffer; // combine vertices et indices, c'est ce qui est recommandé
	VkDeviceMemory m_meshBufferMemory;
	VkDeviceSize m_indicesOffset;

	VkImage m_depthImage;
	VkDeviceMemory m_depthImageMemory;
	VkImageView m_depthImageView;

	uint32_t m_mipLevels{1};
	VkImage m_textureImage;
	VkDeviceMemory m_textureImageMemory;
	VkImageView m_textureImageView;

	VkSampler m_textureSampler;

	// faut un inform buffer par frames in flight
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;
	std::vector<void*> m_uniformBuffersMapped;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	//std::vector<VkFramebuffer> m_swapChainFrameBuffers;

	std::vector<VkCommandBuffer> m_commandBuffers;

	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;

	std::vector<VkFence> m_inFlightFences;

	int m_currentFrame{0};

	bool m_framebufferResized{false};
	// VK_ERROR_OUT_OF_DATE_KHR, est pas garantie pendant le resize en fonction de la plateforme
	// donc ajout du bool pour le gerer correctement

	VkImage m_colorImage;
	VkDeviceMemory m_colorImageMemory;
	VkImageView m_colorImageView;

	VkSampleCountFlagBits m_msaaSamples;
	VkSampleCountFlagBits getMaxMsaa();

	Camera m_camera{};

	// frame timing for smooth movement
	float m_deltaTime{0.0f};
	double m_lastFrame{0.0};

	// input processing (polling each frame)
	void processInput(float dt);

	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onKey(int key, int scancode, int action, int mods);

	// DEBUG
	void setObjectName(VkBuffer buffer, const char* name);
};

#endif // VULKAN_APP_H