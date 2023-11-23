#version 330 core
in vec3 normal;
in vec4 fragColor;
in vec2 texCoord;
in vec3 lightPos;
in vec3 fragPos;
in vec4 strength;
out vec4 outColor;

uniform sampler2D ourTexture;

void main() {

    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // ambient
    vec3 ambient = strength.x * lightColor;

    // diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos-fragPos);
    vec3 diffuse = strength.y * max(dot(norm, lightDir), 0.0)*lightColor;

    // specular
    vec3 viewDir = normalize(-fragPos);
    //vec3 halfwayDir = normalize(lightDir + viewDir);
    //float spec = pow(max(dot(norm, halfwayDir), 0.0), strength.w);
    vec3 reflectDir = normalize(-reflect(lightDir, norm));
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), strength.w);
    vec3 specular = strength.z * spec * lightColor;

    outColor = (texture(ourTexture, texCoord) + fragColor) * vec4(ambient+diffuse+specular, 1.0);
}

