#version 430

layout(local_size_x = 128) in;

layout(std430, binding = 0) readonly buffer Neighbors { int neighbors[]; };
layout(std430, binding = 2) readonly buffer MassIn    { float massA[]; };
layout(std430, binding = 3) writeonly buffer MassOut    { float massB[]; };
layout(std430, binding = 4) coherent buffer Attached { uint attached[]; };
layout(std430, binding = 5) readonly buffer Border   { uint borderMask[]; };
layout(std430, binding = 6) readonly buffer Boundary { uint boundary[]; };
layout(std430, binding = 7) buffer BoundaryMass { float boundaryMass[]; };

uniform uint nSitesFull;
uniform float eps;

// turned beta table to two vectors
uniform float beta1[4];
uniform float beta2[4];

// helper to index into kappa table
float betaLookup(int nZ, int nT) {
    int idx = nT - 3;

    if (nZ == 1) return beta1[idx];
    return beta2[idx];
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= nSitesFull) return;

    // carry mass by default
    massB[gid] = massA[gid];

    if (attached[gid] != 0u || borderMask[gid] != 0u || boundary[gid] == 0u) {
        massB[gid] = massA[gid];
        return;
    }
        

    float bMass = boundaryMass[gid];
    if (bMass < 0.0) return; // safety

    // Count attached neighbors
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
    float beta = betaLookup(nZcapped, nTcapped);

    if (bMass + eps >= beta) {
        attached[gid] = 1u;
        boundaryMass[gid] = 0.0;
        massB[gid]  = 0.0;
    }
}