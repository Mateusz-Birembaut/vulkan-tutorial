#pragma once

#include <glm/glm.hpp>

struct alignas(16) Object
{
	// x, y, z = position
    // w       = type
    glm::vec4 geometry; 

    // x y z 
    // w shininess
    glm::vec4 rotation; 

    // x y z
    // w radius
    glm::vec4 scale; 

    // x, y, z = RGB
    // w       = mat index
    glm::vec4 materialInfo;

    
};
