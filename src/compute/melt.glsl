#version 430
layout(local_size_x = 128) in;

layout(std430, binding = 0) readonly buffer  Neighbors { int   neighbors[]; };
layout(std430, binding = 2) readonly buffer  MassIn    { float massA[];    };
layout(std430, binding = 3) writeonly buffer MassOut   { float massB[];    };

layout(std430, binding = 4) readonly buffer Attached   { uint  attached[];  };
layout(std430, binding = 5) readonly buffer Border     { uint  borderMask[];};
layout(std430, binding = 6) readonly buffer Boundary   { uint  boundary[];  };

layout(std430, binding = 7) buffer BoundaryMass { float boundaryMass[]; }; // read/write

uniform uint nSitesFull;

uniform float mu1[4];  // nZcapped = 1, index by (nT-3)
uniform float mu2[4];  // nZcapped = 2

float muLookup(int nZ, int nT) {
    int idx = nT - 3;
    return (nZ == 1) ? mu1[idx] : mu2[idx];
}

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= nSitesFull) return;

    float d0 = massA[gid];         // current vapor
    // pass-through for non-melting sites
    if (attached[gid] != 0u) {          // inside the crystal
        massB[gid] = d0;
        return;
    }
    if (borderMask[gid] != 0u || boundary[gid] == 0u) { // not an outer boundary cell
        massB[gid] = d0;
        return;
    }

    // neighbor counts (same as freeze.glsl)
    uint base = gid * 8u;
    int nT = 0, nZ = 0;
    // 6 planar neighbors
    for (uint k = 0u; k < 6u; ++k) {
        int j = neighbors[base + k];
        if (j >= 0 && attached[uint(j)] != 0u) nT++;
    }
    // 2 basal neighbors
    for (uint k = 6u; k <= 7u; ++k) {
        int j = neighbors[base + k];
        if (j >= 0 && attached[uint(j)] != 0u) nZ++;
    }

    // cap counts to table domain
    int nTcapped = (nT < 3) ? 3 : ((nT > 6) ? 6 : nT);
    int nZcapped = (nZ < 1) ? 1 : ((nZ > 2) ? 2 : nZ);

    float b0   = boundaryMass[gid];
    float mu   = muLookup(nZcapped, nTcapped);

    // melt a fraction mu of boundary mass into vapor
    float take = mu * b0;

    // numerical safety
    if (take > b0) take = b0;
    if (take < 0.0) take = 0.0;

    boundaryMass[gid] = b0 - take;    // shrink boundary layer
    massB[gid]        = d0 + take;    // add back to vapor
}
