#version 430

layout(local_size_x = 128) in;

layout(std430, binding = 0) readonly buffer  Neighbors { int neighbors[]; };
layout(std430, binding = 2) readonly buffer  MassIn    { float massA[]; };
layout(std430, binding = 3) writeonly buffer MassOut   { float massB[]; };

layout(std430, binding = 4) readonly buffer Attached { uint attached[]; };
layout(std430, binding = 5) readonly buffer Border   { uint borderMask[]; };
layout(std430, binding = 6) readonly buffer Boundary { uint boundary[]; };

layout(std430, binding = 7) buffer BoundaryMass { float boundaryMass[]; };

uniform uint nSitesFull;

// turned kappa table to two vectors
uniform float kappa1[4];
uniform float kappa2[4];

// helper to index into kappa table
float kappaLookup(int nZ, int nT) {
    int idx = nT - 3;

    if (nZ == 1) return kappa1[idx];
    return kappa2[idx];
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= nSitesFull) return;

    float d0 = massA[gid];

    // freezing only happens on the boundary
    if (attached[gid] != 0u) { 
        massB[gid] = 0.0; 
        return; 
    }
    if (borderMask[gid] != 0u || boundary[gid] == 0u) {
        massB[gid] = d0; 
        return;
    }

    uint base = gid * 8u;
    int nT = 0, nZ = 0;
    for (uint k = 0u; k < 6u; ++k) {
        int j = neighbors[base + k];
        if (j >= 0 && attached[uint(j)] != 0u) nT++;
    }
    for (uint k = 6u; k <= 7u; ++k) {
        int j = neighbors[base + k];
        if (j >= 0 && attached[uint(j)] != 0u) nZ++;
    }

    int nTcapped = (nT < 3) ? 3 : ((nT > 6) ? 6 : nT);
    int nZcapped = (nZ < 1) ? 1 : ((nZ > 2) ? 2 : nZ);

    float k = kappaLookup(nZcapped, nTcapped);

    float take = (1.0 - k) * d0;
    massB[gid] = k * d0;
    boundaryMass[gid] += take;
}