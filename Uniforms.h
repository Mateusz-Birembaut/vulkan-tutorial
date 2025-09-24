#ifndef UNIFORMS_H
#define UNIFORMS_H

#include <glm/glm.hpp>

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};
// doit etre aligné, voir https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-resources-layout

#endif // UNIFORMS_H