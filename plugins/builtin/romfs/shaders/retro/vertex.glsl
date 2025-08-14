#version 330 core
in vec2 position;
in vec2 texCoords;

out vec2 TexCoords;

void main() {
    TexCoords = texCoords;
    gl_Position = vec4(position, 0.0, 1.0);
}