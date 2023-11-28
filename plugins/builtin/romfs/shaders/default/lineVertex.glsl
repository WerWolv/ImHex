#version 330 core

layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec4 in_Color;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec4 fragColor;

void main() {
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(in_Position, 1.0);
    fragColor   = in_Color;
}
