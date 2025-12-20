#pragma once

#include <glm/glm.hpp>

struct alignas(16) Sphere
{
	// x, y, z = position
    // w       = radius
    glm::vec4 geometry; 

    // x, y, z = RGB
    // w       = mat index
    glm::vec4 materialInfo;
};
