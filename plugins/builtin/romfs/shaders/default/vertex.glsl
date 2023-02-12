#version 330 core
layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec3 in_Normal;

/*uniform float time;*/
uniform float scale;
uniform vec3 rotation;
uniform vec3 translation;

out vec3 normal;

mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
    oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
    oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
    0.0,                                0.0,                                0.0,                                1.0);
}

mat4 viewMatrix(vec3 rotation) {
    mat4 rotationX = rotationMatrix(vec3(1, 0, 0), rotation.x);
    mat4 rotationY = rotationMatrix(vec3(0, 1, 0), rotation.y);
    mat4 rotationZ = rotationMatrix(vec3(0, 0, 1), rotation.z);

    return rotationX * rotationY * rotationZ;
}

void main() {
    mat4 view = viewMatrix(rotation);
    normal = (vec4(in_Normal, 1.0) * view).xyz;
    gl_Position = vec4((in_Position + translation) * scale, 1.0) * view;
}