#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	void init();
	void cleanup() noexcept;

private:
	void recordRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void drawFrame();

	void createUniformBuffer();
	void updateUniformBuffer(uint32_t currentImage);

	void recreateSwapChain(); // rename en mieux ?
	void cleanupSwapChain(); // rename en mieux ?
};
