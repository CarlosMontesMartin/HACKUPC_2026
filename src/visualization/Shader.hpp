#pragma once
//===-- visualization/Shader.hpp --------------------------------*- C++ -*-===//
// Tiny RAII wrapper around an OpenGL shader program. Owns the GL resource
// via a non-zero program id; releases it in the destructor.
//===----------------------------------------------------------------------===//

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <string>

namespace warehouse {

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    ~Shader();

    void use() const;
    GLuint id() const { return m_id; }

    // Uniform setters (look up by name, cached internally would be a nice
    // optimisation, omitted for clarity).
    void setMat4(const std::string& name, const glm::mat4& m) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setVec4(const std::string& name, const glm::vec4& v) const;
    void setFloat(const std::string& name, float v) const;
    void setInt(const std::string& name, int v) const;

private:
    static GLuint compile(GLenum stage, const std::string& src);
    void release();

    GLuint m_id{0};
};

} // namespace warehouse
