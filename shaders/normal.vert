#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out Data
{
    vec4 position;
    vec3 normal;
    mat4 mvp;
} vdata;

void main()
{
    vdata.mvp = projection * view * model;
    vdata.position = vec4(aPos, 1.0f);
    vdata.normal = aNormal;
}
