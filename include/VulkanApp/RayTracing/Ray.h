#pragma once

#include <glm/glm.hpp>

struct alignas(16) Ray
{
	glm::vec3 position;
	glm::vec3 direction;
};
