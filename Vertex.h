#ifndef VERTEX_H
#define VERTEX_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 col;
	glm::vec3 normal;
	glm::vec2 uv;

	// besoin de décrire a vulkan comment passer cette struct a notre vertex shader
	// une fois chargé dans la mémoire gpu

	// VkVertexInputBindingDescription donne le nb d'octets
	// (de combien d'octet sauter pour lire le prochain vertex)
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDesc{};
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(Vertex);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// VK_VERTEX_INPUT_RATE_INSTANCE, par exemple on stocke qu'une fois notre mesh
		// si on a 100 mesh exactement pareil juste transfo qui change
		// on peut lire 100 fois le buffer, on économise de la mémoire
		// on fait pas de instance rendering donc on prend la version classique
		// ou on a toutes nos données pour chaque sommets
		return bindingDesc;
	}

	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, col);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, normal);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, uv);

		return attributeDescriptions;
	}
};

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

#endif // VERTEX_H