#include <fstream>
#include <iostream>

#include "shader.h"

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // Vertex shader
    std::ifstream ifs("shader.vert");
    std::string vsCode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const char* vsCodeStr = vsCode.c_str();

    uint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsCodeStr, NULL);
    glCompileShader(vs);

    int success;
    char infolog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    // Fragment shader
    ifs = std::ifstream("shader.frag");
    std::string fsCode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const char* fsCodeStr = fsCode.c_str();

    uint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsCodeStr, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    // create program
    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(id, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}
