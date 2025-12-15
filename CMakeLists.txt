cmake_minimum_required(VERSION 3.16)
project(VulkanTutorial)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Mode Debug par défaut pour le développement
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(SOURCES
    src/main.cpp
    src/VulkanApp.cpp
    src/Core/VulkanContext.cpp
    src/Core/VulkanDebug.cpp
)

# tinyobj loader implementation (provides tinyobj::LoadObj)
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tiny_obj_loader.cc)


add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
    glfw
)