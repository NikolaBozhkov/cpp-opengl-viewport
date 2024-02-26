#pragma once

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#include <GL/gl.h>
#endif

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

class Shader
{
public:
    unsigned int id;

    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);

    template<typename T>
    void SetUniform(const char* name, T& value)
    {
        GLuint location = glGetUniformLocation(id, name);
        if constexpr (std::is_same_v<T, glm::mat4>)
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        else if constexpr (std::is_same_v<T, glm::vec3>)
            glUniform3fv(location, 1, glm::value_ptr(value));
    }
};
