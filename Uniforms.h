#ifndef UNIFORMS_H 
#define UNIFORMS_H 

#include <glm/glm.hpp>

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};


#endif // UNIFORMS_H   