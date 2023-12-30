#version 330 core
layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec4 in_Color;
layout (location = 2) in vec3 in_Normal;
layout (location = 3) in vec2 in_TexCoord;

uniform mat4 modelScale;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform vec3 lightPosition;
uniform vec4 lightBrightness;
uniform vec3 lightColor;

out VertexData {
     vec3 normal;
     vec4 fragColor;
     vec2 texCoord;
     vec3 lightPosition;
     vec3 fragPosition;
     vec4 lightBrightness;
     vec3 lightColor;
} vertexData;

void main() {
     gl_Position                   = projectionMatrix * viewMatrix * modelScale * vec4(in_Position, 1.0);

     vertexData.normal             = mat3(transpose(inverse(modelScale))) * in_Normal;
     vertexData.fragPosition       = vec3(viewMatrix * modelScale * vec4(in_Position, 1.0));
     vertexData.fragColor          = in_Color;
     vertexData.texCoord           = in_TexCoord;
     vertexData.lightBrightness    = lightBrightness;
     vertexData.lightColor         = lightColor;

     // Transform world-space light position to view-space light position
     vertexData.lightPosition      = vec3(viewMatrix * modelMatrix * vec4(lightPosition, 1.0));
}