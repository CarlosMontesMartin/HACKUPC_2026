//===-- visualization/Renderer.cpp ------------------------------*- C++ -*-===//
#include "Renderer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace warehouse {

namespace {

// Cube geometry: 8 vertices, 12 triangles, color per face.
// Vertex layout: vec3 position. Per-face color is supplied via a uniform
// (we keep the VBO simple).
const float kCubeVertices[] = {
    // 24 vertices (4 per face) so flat per-face shading works.
    // -X
    0,0,0,  0,1,0,  0,1,1,  0,0,1,
    // +X
    1,0,0,  1,1,0,  1,1,1,  1,0,1,
    // -Y (bottom)
    0,0,0,  1,0,0,  1,0,1,  0,0,1,
    // +Y (top)
    0,1,0,  1,1,0,  1,1,1,  0,1,1,
    // -Z
    0,0,0,  1,0,0,  1,1,0,  0,1,0,
    // +Z
    0,0,1,  1,0,1,  1,1,1,  0,1,1,
};

const unsigned int kCubeIndices[] = {
    0,1,2,  0,2,3,    // -X
    4,6,5,  4,7,6,    // +X
    8,9,10, 8,10,11,  // -Y
    12,14,13, 12,15,14, // +Y
    16,18,17, 16,19,18, // -Z
    20,21,22, 20,22,23, // +Z
};

// Wireframe edges: 8 unique corners + 24 indices for the 12 edges.
const float kCubeCorners[] = {
    0,0,0, 1,0,0, 1,1,0, 0,1,0,   // bottom (y=0 in cube-local: but here Y is up
                                  // so really 'bottom' = y=0 face)
    0,0,1, 1,0,1, 1,1,1, 0,1,1,
};
const unsigned int kCubeEdgeIndices[] = {
    // bottom rectangle
    0,1, 1,2, 2,3, 3,0,
    // top rectangle
    4,5, 5,6, 6,7, 7,4,
    // vertical pillars
    0,4, 1,5, 2,6, 3,7,
};

constexpr float kFaceShade[6] = {
    0.75f, 0.85f, 0.55f, 1.00f, 0.65f, 0.95f
};

const char* kSolidVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
flat out int vFaceId;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    // 4 vertices per face -> faceId = vertexId / 4
    vFaceId = gl_VertexID / 4;
}
)GLSL";

// vec4 color so the same shader can draw opaque boxes AND the translucent
// ceiling slab. uShades[] modulates the RGB only; alpha is preserved.
const char* kSolidFS = R"GLSL(
#version 330 core
flat in int vFaceId;
uniform vec4  uColor;
uniform float uShades[6];
out vec4 FragColor;
void main() {
    float s = uShades[vFaceId];
    FragColor = vec4(uColor.rgb * s, uColor.a);
}
)GLSL";

const char* kLineVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)GLSL";

const char* kLineFS = R"GLSL(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor, 1.0); }
)GLSL";

} // anonymous namespace

/*
 * Initialization and Cleanup
 * Constructor and destructor for the Renderer class, handling GLFW and OpenGL context setup.
 */

Renderer::Renderer(int width, int height, const std::string& title)
    : m_winW(width), m_winH(height) {
    if (!glfwInit())
        throw std::runtime_error("Renderer: glfwInit failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title.c_str(),
                                nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Renderer: failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        throw std::runtime_error("Renderer: glewInit failed");
    }
    // GLEW can leave a benign GL_INVALID_ENUM error; flush it.
    glGetError();

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCB);
    glfwSetMouseButtonCallback(m_window,    mouseButtonCB);
    glfwSetCursorPosCallback(m_window,      cursorPosCB);
    glfwSetKeyCallback(m_window,            keyCB);

    initGL();
}

Renderer::~Renderer() {
    if (m_cubeEBO)        glDeleteBuffers(1, &m_cubeEBO);
    if (m_cubeVBO)        glDeleteBuffers(1, &m_cubeVBO);
    if (m_cubeVAO)        glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_cubeEdgesEBO)   glDeleteBuffers(1, &m_cubeEdgesEBO);
    if (m_cubeEdgesVBO)   glDeleteBuffers(1, &m_cubeEdgesVBO);
    if (m_cubeEdgesVAO)   glDeleteVertexArrays(1, &m_cubeEdgesVAO);
    if (m_floorVBO)       glDeleteBuffers(1, &m_floorVBO);
    if (m_floorVAO)       glDeleteVertexArrays(1, &m_floorVAO);
    if (m_ceilingVBO)     glDeleteBuffers(1, &m_ceilingVBO);
    if (m_ceilingVAO)     glDeleteVertexArrays(1, &m_ceilingVAO);
    if (m_lineVBO)        glDeleteBuffers(1, &m_lineVBO);
    if (m_lineVAO)        glDeleteVertexArrays(1, &m_lineVAO);

    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

/*
 * OpenGL Setup and Configuration
 * Initializes OpenGL state, shaders, and static vertex buffers.
 */

void Renderer::initGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_solidShader = std::make_unique<Shader>(kSolidVS, kSolidFS);
    m_lineShader  = std::make_unique<Shader>(kLineVS,  kLineFS);

    // Solid cube (24 verts).
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glGenBuffers(1, &m_cubeEBO);
    glBindVertexArray(m_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices),
                 kCubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIndices),
                 kCubeIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    // Cube edges (8 verts, 24 indices).
    glGenVertexArrays(1, &m_cubeEdgesVAO);
    glGenBuffers(1, &m_cubeEdgesVBO);
    glGenBuffers(1, &m_cubeEdgesEBO);
    glBindVertexArray(m_cubeEdgesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeEdgesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeCorners),
                 kCubeCorners, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEdgesEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeEdgeIndices),
                 kCubeEdgeIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

void Renderer::buildStaticBuffers(const Scene& scene) {
    auto upload = [](GLuint& vao, GLuint& vbo,
                     const std::vector<glm::vec3>& verts) {
        if (vao == 0) {
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
        }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(verts.size() * sizeof(glm::vec3)),
                     verts.empty() ? nullptr : verts.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(glm::vec3), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
    };

    upload(m_floorVAO,   m_floorVBO,   scene.floorVertices);
    m_floorVertexCount   = static_cast<GLsizei>(scene.floorVertices.size());

    upload(m_ceilingVAO, m_ceilingVBO, scene.ceilingVertices);
    m_ceilingVertexCount = static_cast<GLsizei>(scene.ceilingVertices.size());

    upload(m_lineVAO,    m_lineVBO,    scene.perimeterLineLoop);
    m_lineVertexCount    = static_cast<GLsizei>(scene.perimeterLineLoop.size());
}

/*
 * Drawing Primitives
 * Functions to render individual elements like boxes, outlines, floor, and perimeter.
 */

void Renderer::drawBox(const BoxInstance& b, const glm::mat4& vp) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, b.position);
    if (b.rotationRadY != 0.0f) {
        model = glm::rotate(model, b.rotationRadY,
                            glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, b.size);

    m_solidShader->use();
    m_solidShader->setMat4("uMVP", vp * model);
    m_solidShader->setVec4("uColor", glm::vec4(b.color, 1.0f));
    GLint loc = glGetUniformLocation(m_solidShader->id(), "uShades");
    glUniform1fv(loc, 6, kFaceShade);

    glBindVertexArray(m_cubeVAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(sizeof(kCubeIndices) /
                                        sizeof(unsigned int)),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::drawBoxOutline(const BoxInstance& b, const glm::mat4& vp) {
    // Same model matrix as the solid pass - uses the wireframe cube and
    // the line shader. Polygon-offset is applied around the SOLID pass
    // so the outlines win the depth comparison cleanly.
    glm::mat4 model(1.0f);
    model = glm::translate(model, b.position);
    if (b.rotationRadY != 0.0f) {
        model = glm::rotate(model, b.rotationRadY,
                            glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, b.size);

    m_lineShader->use();
    m_lineShader->setMat4("uMVP", vp * model);
    m_lineShader->setVec3("uColor", glm::vec3(0.0f, 0.0f, 0.0f));
    glLineWidth(1.5f);

    glBindVertexArray(m_cubeEdgesVAO);
    glDrawElements(GL_LINES,
                   static_cast<GLsizei>(sizeof(kCubeEdgeIndices) /
                                        sizeof(unsigned int)),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::drawFloor(const Scene& scene, const glm::mat4& vp) {
    if (m_floorVertexCount == 0) return;
    m_solidShader->use();
    m_solidShader->setMat4("uMVP", vp);
    m_solidShader->setVec4("uColor", scene.floorColor);
    static const float floorShades[6] = {1, 1, 1, 1, 1, 1};
    GLint loc = glGetUniformLocation(m_solidShader->id(), "uShades");
    glUniform1fv(loc, 6, floorShades);
    glBindVertexArray(m_floorVAO);
    glDrawArrays(GL_TRIANGLES, 0, m_floorVertexCount);
    glBindVertexArray(0);
}

void Renderer::drawPerimeter(const Scene& /*scene*/, const glm::mat4& vp) {
    if (m_lineVertexCount == 0) return;
    m_lineShader->use();
    m_lineShader->setMat4("uMVP", vp);
    m_lineShader->setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
    glLineWidth(2.0f);
    glBindVertexArray(m_lineVAO);
    glDrawArrays(GL_LINE_LOOP, 0, m_lineVertexCount);
    glBindVertexArray(0);
}

void Renderer::drawCeiling(const Scene& scene, const glm::mat4& vp) {
    if (m_ceilingVertexCount == 0) return;
    // Translucent: enable blending, disable depth-write to avoid the
    // ceiling occluding itself / order-dependent alpha popping.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE); // visible from above and below

    m_solidShader->use();
    m_solidShader->setMat4("uMVP", vp);
    m_solidShader->setVec4("uColor", scene.ceilingColor);
    static const float ceilingShades[6] = {1, 1, 1, 1, 1, 1};
    GLint loc = glGetUniformLocation(m_solidShader->id(), "uShades");
    glUniform1fv(loc, 6, ceilingShades);
    glBindVertexArray(m_ceilingVAO);
    glDrawArrays(GL_TRIANGLES, 0, m_ceilingVertexCount);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawScene(const Scene& scene,
                         const glm::mat4& view,
                         const glm::mat4& proj,
                         bool topDown) {
    /*
     * Renders the complete 3D scene (floor, obstacles, bays, gaps, and perimeter).
     * This function is used for both the main 3D view and the top-down minimap.
     */
    const glm::mat4 vp = proj * view;

    // 1) Opaque pass with polygon offset so the line outlines drawn next
    //    do not z-fight with the filled triangles.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    drawFloor(scene, vp);

    for (const auto& o : scene.obstacles) drawBox(o, vp);
    for (const auto& b : scene.bays)      drawBox(b, vp);

    // Draw gaps as bright purple slabs. We override the box's per-instance
    // color via the Scene's `gapColor` so the purple is consistent and
    // strictly reserved for gaps.
    for (const auto& g : scene.gaps) {
        BoxInstance bi = g;
        // Replace the per-instance RGB with the canonical Scene colour
        // (its alpha is ignored here - opaque pass).
        bi.color = glm::vec3(scene.gapColor);
        drawBox(bi, vp);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);

    // 2) Outlines (black). Each obstacle, bay and gap gets a 12-edge
    //    wireframe so adjacent solids are visually distinguishable.
    for (const auto& o : scene.obstacles) drawBoxOutline(o, vp);
    for (const auto& b : scene.bays)      drawBoxOutline(b, vp);
    for (const auto& g : scene.gaps)      drawBoxOutline(g, vp);

    // 3) Perimeter outline above the floor.
    drawPerimeter(scene, vp);

    // 4) Translucent ceiling (only in 3D view - it would occlude the
    //    minimap top-down view).
    if (!topDown) {
        drawCeiling(scene, vp);
    }
}

/*
 * Frame Rendering Logic
 * Handles the main rendering loop, viewport configuration, and minimap rendering.
 */

void Renderer::renderMainView(const Scene& scene) {
    glViewport(0, 0, m_winW, m_winH);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 view = m_camera.getViewMatrix();

    // Use a far plane proportional to camera distance for stability with
    // very large warehouses (millimetre units).
    const float far = std::max(2.0f, m_camera.distance() * 8.0f);
    const float near = std::max(0.001f, m_camera.distance() * 0.001f);

    const glm::mat4 proj = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(m_winW) / static_cast<float>(m_winH),
        near, far);

    drawScene(scene, view, proj, /*topDown=*/false);
}

void Renderer::renderMinimap(const Scene& scene) {
    /*
     * Renders a top-down orthographic view of the warehouse layout in the
     * top-right corner of the window (picture-in-picture minimap).
     */
    // Top-right corner picture-in-picture.
    const int mmW = std::max(180, m_winW / 4);
    const int mmH = std::max(140, m_winH / 4);
    const int mmX = m_winW - mmW - 12;
    const int mmY = m_winH - mmH - 12;

    glViewport(mmX, mmY, mmW, mmH);

    // Slight scissor + clear so the minimap has its own background frame.
    glEnable(GL_SCISSOR_TEST);
    glScissor(mmX, mmY, mmW, mmH);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Top-down orthographic view of the same world.
    const float wx = scene.worldExtents.x;
    const float wz = scene.worldExtents.z;
    const float pad = 0.1f * std::max(wx, wz);
    const float halfW = 0.5f * wx + pad;
    const float halfH = 0.5f * wz + pad;

    // Aspect-correct extents: fit world inside minimap viewport.
    const float vpAspect = static_cast<float>(mmW) / static_cast<float>(mmH);
    const float worldAspect = (2.0f * halfW) / (2.0f * halfH);
    float orthoHalfW = halfW;
    float orthoHalfH = halfH;
    if (worldAspect > vpAspect) {
        orthoHalfH = halfW / vpAspect;
    } else {
        orthoHalfW = halfH * vpAspect;
    }

    const glm::mat4 proj = glm::ortho(
        -orthoHalfW, orthoHalfW, -orthoHalfH, orthoHalfH,
        -10000.0f, 10000.0f);

    // Look straight down. Up vector is +Z so North is "up" on the map.
    const glm::vec3 eye   = scene.worldCenter + glm::vec3(0.0f, 1000.0f, 0.0f);
    const glm::vec3 ctr   = scene.worldCenter;
    const glm::vec3 up    = glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::mat4 view  = glm::lookAt(eye, ctr, up);

    drawScene(scene, view, proj, /*topDown=*/true);

    glDisable(GL_SCISSOR_TEST);
}

void Renderer::run(const Scene& scene) {
    buildStaticBuffers(scene);

    // Configure camera around the scene centre.
    m_camera.setTarget(scene.worldCenter);
    const float diag =
        std::sqrt(scene.worldExtents.x * scene.worldExtents.x +
                  scene.worldExtents.z * scene.worldExtents.z);
    m_camera.setDistance(std::max(1.0f, diag * 1.1f));
    m_camera.setAngles(0.6f, 0.55f);

    glfwShowWindow(m_window);

    while (!glfwWindowShouldClose(m_window)) {
        glfwGetFramebufferSize(m_window, &m_winW, &m_winH);
        renderMainView(scene);
        renderMinimap(scene);
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}

/*
 * Input Callbacks
 * GLFW callbacks for handling window resize, mouse clicks, and keyboard events.
 */

void Renderer::framebufferSizeCB(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->m_winW = std::max(1, width);
    self->m_winH = std::max(1, height);
}

void Renderer::mouseButtonCB(GLFWwindow* w, int button, int action, int /*mods*/) {
    auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            self->m_dragging = true;
            glfwGetCursorPos(w, &self->m_lastX, &self->m_lastY);
        } else if (action == GLFW_RELEASE) {
            self->m_dragging = false;
        }
    }
}

void Renderer::cursorPosCB(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    if (self->m_dragging) {
        const double dx = x - self->m_lastX;
        const double dy = y - self->m_lastY;
        self->m_camera.rotate(static_cast<float>(dx),
                              static_cast<float>(dy));
    }
    self->m_lastX = x;
    self->m_lastY = y;
}

void Renderer::keyCB(GLFWwindow* w, int key, int /*sc*/, int action, int /*mods*/) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
    }
}

} // namespace warehouse
