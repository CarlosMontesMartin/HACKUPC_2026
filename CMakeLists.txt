#==============================================================================
# Warehouse Optimization & Visualizer - top-level CMake configuration.
#==============================================================================

cmake_minimum_required(VERSION 3.10)
project(WarehouseOptimizer
    VERSION 1.0.0
    DESCRIPTION "Greedy warehouse layout optimizer + OpenGL visualizer"
    LANGUAGES CXX C
)

#-----------------------------------------------------------------------------#
# Compiler standard and warnings.
#-----------------------------------------------------------------------------#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#-----------------------------------------------------------------------------#
# Dependencies.
#
#   * OpenGL  - system
#   * GLFW    - apt: libglfw3-dev   /  brew: glfw  /  vcpkg: glfw3
#   * GLEW    - apt: libglew-dev    /  brew: glew  /  vcpkg: glew
#   * GLM     - apt: libglm-dev     /  brew: glm   /  vcpkg: glm
#-----------------------------------------------------------------------------#
find_package(OpenGL REQUIRED)
find_package(GLEW   REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)

#-----------------------------------------------------------------------------#
# Project sources.
#-----------------------------------------------------------------------------#
set(PROJECT_SOURCES
    src/main.cpp

    src/common/Types.cpp

    src/data_input/DataInput.cpp

    src/optimization/Optimizer.cpp

    src/data_output/SceneBuilder.cpp

    src/visualization/Camera.cpp
    src/visualization/Shader.cpp
    src/visualization/Renderer.cpp
)

set(PROJECT_HEADERS
    src/common/Types.hpp
    src/data_input/DataInput.hpp
    src/optimization/Optimizer.hpp
    src/data_output/SceneBuilder.hpp
    src/visualization/Camera.hpp
    src/visualization/Shader.hpp
    src/visualization/Renderer.hpp
)

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm::glm
)

# Linux-specific runtime deps brought in by GLFW.
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

#-----------------------------------------------------------------------------#
# Copy the data directory next to the executable so the default
# './data' relative path keeps working when running from build dir.
#-----------------------------------------------------------------------------#
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
    COMMENT "Copying data files to build directory"
)
