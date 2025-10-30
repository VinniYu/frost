#version 430 core

layout(location=0) in vec3 aPos;          // unit prism
layout(location=1) in vec3 aInstancePos;  // per-instance center
layout(std430, binding = 4) readonly buffer Attached { uint attached[]; };
layout(std430, binding = 6) readonly buffer Boundary { uint boundary[]; };

uniform mat4 uModel, uView, uProj;
uniform float uHexScale;
uniform float uHeight;
uniform float uEdgeEps;

flat out int vAttached;
flat out int vBoundary;

void main() {
    uint idx = uint(gl_InstanceID);
    vAttached = int(attached[idx]);
    vBoundary = int(boundary[idx]);

    vec3 p = vec3(aPos.x*uHexScale, aPos.y*uHeight, aPos.z*uHexScale);

    if (uEdgeEps > 0.0) {
        // nudge lines slightly outward in XZ and a tiny bit up in Y
        p += vec3(aPos.x, 0.2, aPos.z) * uEdgeEps;
    }

    vec3 worldPos = p + aInstancePos;
    gl_Position = uProj * uView * uModel * vec4(worldPos, 1.0);
}


