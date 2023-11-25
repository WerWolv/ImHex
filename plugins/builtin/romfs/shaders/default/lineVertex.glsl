#version 330 core
layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec4 in_Color;


uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec4 fragColor;

void main() {
    gl_Position = Projection * View * Model * vec4(in_Position, 1.0);
    fragColor = in_Color;
}
