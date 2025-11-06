#version 430
in vec3 vPosObj;
out vec4 fragColor;

uniform sampler2D uHeight;  // 2D map on XZ plane
uniform vec3 uCamPosObj;    // camera position in object space
uniform float uStepMul;     // samples per unit length
uniform float uExposure;    // tonemapping

uniform float uSigmaT;
uniform float uSigmaS;
uniform vec3  uSigmaA;

bool intersectAABB(vec3 ro, vec3 rd, out float t0, out float t1){
  vec3 minB = vec3(-0.5), maxB = vec3(0.5);
  vec3 invD = 1.0 / rd;
  
  vec3 tA = (minB - ro) * invD;
  vec3 tB = (maxB - ro) * invD;

  vec3 tmin = min(tA,tB);
  vec3 tmax = max(tA,tB);

  t0 = max(max(tmin.x,tmin.y), tmin.z);
  t1 = min(min(tmax.x,tmax.y), tmax.z);
  return t1 > max(t0, 0.0);
}

void main(){
  // ray 
  vec3 ro = uCamPosObj;
  vec3 rd = normalize(vPosObj - uCamPosObj);

  float tEnter, tExit;
  if(!intersectAABB(ro, rd, tEnter, tExit)) discard;
  tEnter = max(tEnter, 0.0);

  float segLen = tExit - tEnter;
  int   steps  = max(1, int(segLen * uStepMul));
  float dt     = segLen / float(steps);

  // basic absorption
  float accum = 0.0;
  float T = 1.0;

  for (int i=0; i<steps; ++i) {
    float t = tEnter + (i+0.5)*dt;
    vec3 p = ro + rd * t; // object-space sample in [-0.5,0.5]^3

    // map to XZ plane UVs
    vec2 uvXZ = p.xz + vec2(0.5);

    // map Y
    float yNorm = (p.y + 0.5) / 0.25;

    // sample height in [0,1]
    float h = texelFetch(uHeight, ivec2(clamp(uvXZ*textureSize(uHeight,0), vec2(0), vec2(textureSize(uHeight,0))-1)), 0).r;

    // convert 0..1 height to 0..1 “column height”
    float d = smoothstep(h - 0.01, h + 0.01, yNorm);
    d = 1.0 - d;  // temp fix

    // accumulate
    float alpha = d * dt * 10.5;    
    alpha = clamp(alpha, 0.0, 1.0);

    accum += T * alpha;
    T *= (1.0 - alpha);
    if (T < 1e-3) break;
  }

  // tonemap
  float v = 1.0 - exp(-accum * uExposure);
  fragColor = vec4(vec3(v), 1.0);
}
