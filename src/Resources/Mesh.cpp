#include <VulkanApp/Resources/Mesh.h>

#include <VulkanApp/Resources/Vertex.h>
#include <VulkanApp/Core/VulkanContext.h>
#include <VulkanApp/Commands/CommandManager.h>
#include <VulkanApp/Utils/Utility.h>

#include "tiny_obj_loader.h"


void Mesh::init(VulkanContext* context, CommandManager* commandManager,  const std::string& modelPath){
	loadMesh(modelPath);
	createBuffer(context, commandManager);
}

void Mesh::cleanup(){
	m_buffer.cleanup();
	m_indicesOffset = 0;
}

void Mesh::loadMesh(const std::string& modelPath) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	vertices.clear();
	vertices.resize(attrib.vertices.size());
	indices.clear();
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]};

			vertex.uv = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

			vertex.col = {1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}


void Mesh::createBuffer(VulkanContext* context, CommandManager* commandManager) {
	VkDeviceSize verticesSize = vertexSize() * verticesCount();
	VkDeviceSize indicesSize = indexSize() * indicesCount();

	// je dois avoir la fin du buffer aligné (un miltiple de ...)
	// comme j'ai des indices uint16, je dois avoir une size multiple de 2
	// formule pour aligner au multiple :
	// aligned = ((operand + (alignment - 1)) & ~(alignment - 1)) voir utility.h
	// m_indicesOffset = AlignTo(verticesSize, 2);
	// align 16
	m_indicesOffset = AlignTo(verticesSize, 4);
	VkDeviceSize bufferSize = m_indicesOffset + indicesSize;


	Buffer stagingBuffer;
	stagingBuffer.init(context, commandManager, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	setBufferName(context, stagingBuffer.get(), "MeshStagingBuffer");
	stagingBuffer.write(verticesData(), verticesSize, 0);
	stagingBuffer.write(indicesData(), indicesSize, m_indicesOffset);
	stagingBuffer.unmap();

	m_buffer.init(context, commandManager, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_CONCURRENT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	setBufferName(context ,m_buffer.get(), "MeshBuffer");

	stagingBuffer.copyTo(m_buffer.get(), bufferSize);
	stagingBuffer.cleanup();

}
