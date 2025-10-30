#version 430

layout(local_size_x = 128) in;

layout(std430, binding = 0) readonly buffer Neighbors { int neighbors[]; };
layout(std430, binding = 4) readonly buffer Attached  { uint attached[]; };
layout(std430, binding = 5) readonly buffer Border    { uint borderMask[]; };
layout(std430, binding = 6) writeonly buffer Boundary { uint boundary[]; };

uniform uint nSitesFull;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= nSitesFull) return;

    // skip attached and borders
    if (attached[gid] != 0u || borderMask[gid] != 0u) {
        boundary[gid] = 0u;
        return;
    }

    uint base = gid * 8u;

    // check if any neighbor is attached
    for (uint i = 0u; i < 8u; i++) {
        int j = neighbors[base + i];
        if (attached[uint(j)] != 0u) {
            boundary[gid] = 1u;
            return;
        }
    }

    boundary[gid] = 0u;
}