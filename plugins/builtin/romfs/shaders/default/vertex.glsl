#version 330 core
layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec4 in_Color;
layout (location = 2) in vec3 in_Normal;
layout (location = 3) in vec2 in_TexCoord1;

uniform mat4 ScaledModel;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;
uniform vec3 LightPosition;
uniform vec4 Strength;

out vec3 normal;
out vec4 fragColor;
out vec2 texCoord;
out vec3 lightPos;
out vec3 fragPos;
out vec4 strength;

void main() {
     normal = mat3(transpose(inverse(ScaledModel))) * in_Normal;
     gl_Position = Projection * View * ScaledModel * vec4(in_Position, 1.0);
     fragPos = vec3(View * ScaledModel * vec4(in_Position, 1.0));
     fragColor = in_Color;
     texCoord = in_TexCoord1;
     strength = Strength;
     lightPos = vec3(View * Model * vec4(LightPosition, 1.0)); // Transform world-space light position to view-space light position
}