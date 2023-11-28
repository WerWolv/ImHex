#version 330 core

in VertexData {
    vec3 normal;
    vec4 fragColor;
    vec2 texCoord;
    vec3 lightPosition;
    vec3 fragPosition;
    vec4 lightBrightness;
    vec3 lightColor;
} vertexData;

out vec4 outColor;

uniform sampler2D modelTexture;

void main() {
    vec3 ambientLightColor = vec3(1.0, 1.0, 1.0);

    // Ambient lighting
    vec3 ambient = vertexData.lightBrightness.x * ambientLightColor;

    // Diffuse lighting
    vec3 normalVector = normalize(vertexData.normal);

    vec3 lightDirection = normalize(vertexData.lightPosition - vertexData.fragPosition);
    vec3 diffuse = vertexData.lightBrightness.y * max(dot(normalVector, lightDirection), 0.0) * vertexData.lightColor;

    // Specular lighting
    vec3 viewDirection = normalize(-vertexData.fragPosition);
    vec3 reflectDirection = normalize(-reflect(lightDirection, normalVector));
    float reflectionIntensity = pow(max(dot(viewDirection, reflectDirection), 0.0), vertexData.lightBrightness.w);
    vec3 specular = vertexData.lightBrightness.z * reflectionIntensity * vertexData.lightColor;

    outColor = (texture(modelTexture, vertexData.texCoord) + vertexData.fragColor) * vec4(ambient + diffuse + specular, 1.0);
}

