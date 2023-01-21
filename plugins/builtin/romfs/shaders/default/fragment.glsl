#version 330 core
in vec3 normal;
out vec4 color;

void main() {
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(vec3(0, 0, -1));
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

    color = vec4(1.0f, 0.5f, 0.2f, 1.0f) * vec4(diffuse, 1.0) + 0.1;
}