#version 330

in VertexData {
    vec3 normal;
    vec4 color;
    vec3 fragPosition;
} vertexData;

out vec4 outColor;

void main() {

    vec3 nLight = normalize(-vertexData.fragPosition);
    vec3 nNormal = normalize(vertexData.normal);

    float dotLN = dot(nLight, nNormal);

    float diffuse = dotLN * 0.5;

    vec3 color = (diffuse+0.7)*vertexData.color.xyz;
    outColor = vec4(color, 1.0F);
}
