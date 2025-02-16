#version 330 core

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in Data
{
    vec4 position;
    vec3 normal;
    mat4 mvp;
} vdata[3];

out Data
{
    vec4 color;
} gdata;

void main()
{
    const vec4 green = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    const vec4 blue = vec4(0.0f, 0.0f, 1.0f, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        gl_Position = vdata[i].mvp * vdata[i].position;
        gdata.color = green;
        EmitVertex();

        gl_Position = vdata[i].mvp * (vdata[i].position + vec4(normalize(vdata[i].normal) * 0.2f, 0.0f));
        gdata.color = blue;
        EmitVertex();

        EndPrimitive();
    }
}
