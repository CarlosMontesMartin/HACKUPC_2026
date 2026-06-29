//===-- visualization/Shader.cpp --------------------------------*- C++ -*-===//
#include "Shader.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace warehouse {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    const GLuint vs = compile(GL_VERTEX_SHADER,   vertexSrc);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

    m_id = glCreateProgram();
    glAttachShader(m_id, vs);
    glAttachShader(m_id, fs);
    glLinkProgram(m_id);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_id, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen = 0;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<std::size_t>(std::max(1, logLen)));
        glGetProgramInfoLog(m_id, logLen, nullptr, log.data());
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(m_id);
        m_id = 0;
        throw std::runtime_error(std::string("Shader link failed: ") +
                                 log.data());
    }
    glDetachShader(m_id, vs);
    glDetachShader(m_id, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::Shader(Shader&& other) noexcept : m_id(other.m_id) { other.m_id = 0; }

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

Shader::~Shader() { release(); }

void Shader::release() {
    if (m_id) glDeleteProgram(m_id);
    m_id = 0;
}

void Shader::use() const { glUseProgram(m_id); }

GLuint Shader::compile(GLenum stage, const std::string& src) {
    const GLuint sh = glCreateShader(stage);
    const char* p = src.c_str();
    glShaderSource(sh, 1, &p, nullptr);
    glCompileShader(sh);
    GLint compiled = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<std::size_t>(std::max(1, logLen)));
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        glDeleteShader(sh);
        const char* tag = stage == GL_VERTEX_SHADER ? "VS" : "FS";
        throw std::runtime_error(std::string("Shader compile failed (") +
                                 tag + "): " + log.data());
    }
    return sh;
}

void Shader::setMat4(const std::string& n, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(m_id, n.c_str()),
                       1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setVec3(const std::string& n, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(m_id, n.c_str()), 1, glm::value_ptr(v));
}
void Shader::setVec4(const std::string& n, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(m_id, n.c_str()), 1, glm::value_ptr(v));
}
void Shader::setFloat(const std::string& n, float v) const {
    glUniform1f(glGetUniformLocation(m_id, n.c_str()), v);
}
void Shader::setInt(const std::string& n, int v) const {
    glUniform1i(glGetUniformLocation(m_id, n.c_str()), v);
}

} // namespace warehouse
