#version 330 core

in vec3 fragPosition;
in vec3 normal;

uniform vec3 lightPos;

out vec4 color;

void main() {
    vec3 lightDir = normalize(lightPos - fragPosition);

    float ambientStrength = 0.2f;
    vec3 ambient = ambientStrength * vec3(1.0f);

    float diff = max(dot(normal, lightDir), 0.0f);
    vec3 diffuse = diff * vec3(1.0f);

    vec3 result = (ambient + diffuse) * vec3(1.0f, 0.5f, 0.2f);
    color = vec4(result, 1.0f);
}
