#pragma once

#include <glm/glm.hpp>

struct Light {
    glm::vec4 positionAndType; // xyz = position, w = type (0=dir, 1=point)
    glm::vec4 directionAndRange; // xyz = direction, w = range
    glm::vec4 colorAndIntensity; // xyz = rgb, w = intensity (write color as bgr)
};
