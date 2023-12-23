#version 330

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec3 in_Normal;
layout (location = 2) in vec4 in_Color;



out VertexData {
    vec3 normal;
    vec4 color;
    vec3 fragPosition;
} vertexData;


void main() {
    vertexData.normal       = (modelMatrix * vec4(in_Normal,0)).xyz;
    //vertexData.normal       = mat3(transpose(inverse(modelMatrix))) * in_Normal;
    //vertexData.fragPosition = (viewMatrix * modelMatrix * vec4(in_Position, 1.0)).xyz;
    vertexData.fragPosition = (modelMatrix * vec4(in_Position, 1.0)).xyz;
    gl_Position             = projectionMatrix * viewMatrix * modelMatrix * vec4(in_Position, 1.0);
    vertexData.color        = in_Color;
}
