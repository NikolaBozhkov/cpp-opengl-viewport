#version 330 core

in Data
{
    vec4 color;
} gdata;

out vec4 color;

void main()
{
    color = gdata.color;
}
