#pragma once

#include <glm/glm.hpp>

struct alignas(16) Intersection
{
	glm::vec3 n;
	glm::vec3 p;
	bool intersected;
	float t;
	int ma_index;
};
