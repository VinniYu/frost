#version 430

layout(local_size_x = 128) in;

layout(std430, binding = 0) readonly buffer  Neighbors { int neighbors[]; };
layout(std430, binding = 2) readonly buffer  MassIn    { float massA[]; };
layout(std430, binding = 3) writeonly buffer MassOut   { float massB[]; };
layout(std430, binding = 4) readonly buffer  Attached  { uint attached[]; };
layout(std430, binding = 5) readonly buffer  Border    { uint borderMask[]; };

uniform uint nSitesFull;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= nSitesFull) return; // guard

    if (attached[gid] != 0) {
        massB[gid] = 0.0; // attached cells don't hold any massA
        return;
    }
    if (borderMask[gid] != 0u) {
        massB[gid] = massA[gid]; // borders just pass thru
        return;
    }

    float selfW = 1.0 - 0.89;      // keep this much of itself
    float nbrW  = 0.5 * 0.89;

    // basal average
    uint base = gid * 8; // base index in neigbors LUT
    float acc = selfW * massA[gid];

    // basal neighbors are 6,7
    for (uint i = 6u; i <= 7u; i++) {
        int j = neighbors[base + i];
        if (j >= 0) acc += nbrW * massA[uint(j)];
        else        acc += nbrW * massA[gid];
    }

    massB[gid] = acc;
}