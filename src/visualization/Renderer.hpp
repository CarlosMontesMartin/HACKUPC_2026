#pragma once
//===-- visualization/Renderer.hpp ------------------------------*- C++ -*-===//
// Module 4 - Visualisation.
//
// Owns the GLFW window, GL resources, the orbit camera, and the render
// loop. Exposes a single `run(scene)` entry point and is constructed with
// the scene already fully baked (no GL calls happen inside Module 3).
//
// Design notes:
//   * One cube VAO is reused across all box instances via per-draw model
//     matrices; no instancing extension required (works on any 3.3 core).
//   * The minimap is rendered with a top-down orthographic projection of
//     the same scene to a small viewport in the top-right corner.
//   * Mouse drag callback path: GLFW -> static thunks -> instance method
//     via glfwGetWindowUserPointer.
//===----------------------------------------------------------------------===//

#include "../data_output/SceneBuilder.hpp"
#include "Camera.hpp"
#include "Shader.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <memory>
#include <string>

namespace warehouse {

class Renderer {
public:
    Renderer(int width, int height, const std::string& title);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Blocks until the user closes the window.
    void run(const Scene& scene);

private:
    /*
     * GLFW callbacks
     */
    static void framebufferSizeCB(GLFWwindow* w, int width, int height);
    static void mouseButtonCB(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCB(GLFWwindow* w, double x, double y);
    static void keyCB(GLFWwindow* w, int key, int sc, int action, int mods);

    /*
     * internal helpers
     */
    void initGL();
    void buildStaticBuffers(const Scene& scene);

    void renderMainView(const Scene& scene);
    void renderMinimap (const Scene& scene);

    void drawScene(const Scene& scene,
                   const glm::mat4& view,
                   const glm::mat4& proj,
                   bool topDown);

    void drawBox(const BoxInstance& box, const glm::mat4& vp);
    void drawBoxOutline(const BoxInstance& box, const glm::mat4& vp);

    void drawFloor(const Scene& scene, const glm::mat4& vp);
    void drawPerimeter(const Scene& scene, const glm::mat4& vp);
    void drawCeiling(const Scene& scene, const glm::mat4& vp);

    /*
     * members
     */
    GLFWwindow* m_window{nullptr};
    int m_winW{1280}, m_winH{800};

    OrbitCamera m_camera;

    // Mouse state.
    bool   m_dragging{false};
    double m_lastX{0.0}, m_lastY{0.0};

    // GL resources (RAII via Shader; raw GL ids managed manually but
    // released in dtor).
    std::unique_ptr<Shader> m_solidShader;   ///< coloured boxes / floor
    std::unique_ptr<Shader> m_lineShader;    ///< perimeter + outlines

    // Solid cube (24 verts, flat per-face shading via gl_VertexID/4).
    GLuint m_cubeVAO{0}, m_cubeVBO{0}, m_cubeEBO{0};

    // 12-edge wireframe cube for the bay outline pass.
    GLuint m_cubeEdgesVAO{0}, m_cubeEdgesVBO{0}, m_cubeEdgesEBO{0};

    // Polygon-clipped meshes built from the Scene.
    GLuint m_floorVAO{0},   m_floorVBO{0};
    GLsizei m_floorVertexCount{0};
    GLuint m_ceilingVAO{0}, m_ceilingVBO{0};
    GLsizei m_ceilingVertexCount{0};
    GLuint m_lineVAO{0},    m_lineVBO{0};
    GLsizei m_lineVertexCount{0};
};

} // namespace warehouse
