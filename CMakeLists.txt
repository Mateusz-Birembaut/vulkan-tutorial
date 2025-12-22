cmake_minimum_required(VERSION 3.16)
project(VulkanTutorial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- CONFIGURATION QT (Indispensable) ---
# Activer l'autogen Qt (AUTOMOC/AUTOUIC/AUTORCC) — ton chemin Qt est configuré
# via .vscode/settings.json (CMAKE_PREFIX_PATH).
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Ensure editors (clangd / MS C++ extension) can find the compilation DB.
# Create (or refresh) a symlink `compile_commands.json` in the project root
# pointing to the one generated in the build directory.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Symlinking compile_commands.json to ${CMAKE_SOURCE_DIR}"
    VERBATIM
  )
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# --- PACKAGES ---
find_package(Vulkan REQUIRED)

# On cherche Qt6 (Assure-toi que ton settings.json pointe bien vers le dossier Qt)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# --- SOURCES ---
# J'ai ajouté la racine (.) pour trouver Renderer.cpp s'il n'est pas dans src/
# Note: avoid CONFIGURE_DEPENDS here because the VerifyGlobs target can create
# dependency cycles when generated files (autogen/moc) appear in the scan.
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
# Collect top-level .cpp files without recursing into subdirectories (avoids build/)
file(GLOB TOP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
list(APPEND SOURCES ${TOP_SOURCES})

# Exclude generated files inside the build directory (e.g. autogen/moc outputs)
# Be defensive: remove any file that lives in the build dir or autogen, or any moc/ui generated files,
# to avoid CMake AUTOGEN/automoc creating self-dependencies.
list(FILTER SOURCES EXCLUDE REGEX "^${CMAKE_BINARY_DIR}/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/VulkanTutorial_autogen/.*")
# Exclude Qt-generated moc_* and ui_* sources (if present in source list)
list(FILTER SOURCES EXCLUDE REGEX ".*/moc_.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*/ui_.*\\.(h|cpp)$")

# Diagnostic check: list any sources that look like generated/autogen files
foreach(_s IN LISTS SOURCES)
  if(_s MATCHES "VulkanTutorial_autogen" OR _s MATCHES "moc_" OR _s MATCHES "ui_")
    message(WARNING "[SOURCES DIAG] suspicious source in SOURCES: ${_s}")
  endif()
endforeach()

## diagnostic output removed

# On retire main.cpp s'il est détecté deux fois (si tu as gardé l'ancien et le nouveau)
# Mais pour l'instant, le GLOB va tout prendre. Assure-toi de n'avoir qu'un seul fichier avec "main()"

set(TINYOBJ_SRC ${CMAKE_CURRENT_SOURCE_DIR}/tiny_obj_loader.cc)

# --- EXECUTABLE ---
add_executable(${PROJECT_NAME} ${SOURCES} ${TINYOBJ_SRC})

# --- INCLUDES ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# --- LINKING ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# --- CUSTOM TARGETS (Optionnels) ---
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
)

add_custom_target(run-fps
    COMMAND ${CMAKE_COMMAND} -E env MANGOHUD=1 $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
)