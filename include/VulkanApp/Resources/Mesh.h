#ifndef MESH_H
#define MESH_H

#include <VulkanApp/Resources/Vertex.h>
#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Commands/CommandManager.h>
#include <VulkanApp/Resources/Buffer.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <unordered_map>

class Mesh {

      public:

	Buffer& getBuffer() { return m_buffer;};
	VkDeviceSize getIndicesOffset() { return m_indicesOffset; };

	uint32_t verticesCount() const { return vertices.size(); }
	uint32_t indicesCount() const { return indices.size(); }
	uint32_t vertexSize() const { return sizeof(decltype(vertices)::value_type); }
	uint32_t indexSize() const { return sizeof(decltype(indices)::value_type); }

	void* verticesData() { return vertices.data(); }
	void* indicesData() { return indices.data(); }

	void init(VulkanContext* context, CommandManager* m_commandManager,  const std::string& modelPath);
	void cleanup();

      private:
	void loadMesh(const std::string& modelPath);
	void createBuffer(VulkanContext* context, CommandManager* commandManager);

	std::vector<Vertex>
	    vertices = {
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

		{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	};

	std::vector<uint32_t> indices = {0,1,2,2,3,0,4,5,6,6,7,4,};

	Buffer m_buffer;
	VkDeviceSize m_indicesOffset = 0;
	
};

#endif // MESH_H