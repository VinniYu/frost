#version 430 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aInstancePos;

layout(std430, binding = 4) readonly buffer Attached { uint attached[]; };
layout(std430, binding = 6) readonly buffer Boundary { uint boundary[]; };

uniform mat4  uModel, uView, uProj;
uniform float uHexScale;
uniform float uHeight;

flat out int vAttached;
flat out int vBoundary;

void main() {
    uint idx = uint(gl_InstanceID);
    vAttached = int(attached[idx]);
    vBoundary = int(boundary[idx]);

    vec3 worldPos = vec3(aPos.x*uHexScale, aPos.y*uHeight, aPos.z*uHexScale) + aInstancePos;
    gl_Position = uProj * uView * uModel * vec4(worldPos, 1.0);
}
