#version 430 core

layout(location=0) in vec3 aPos;
layout(location=1) in ivec3 aQRC;

uniform mat4 uModel, uView, uProj;
uniform ivec3 uHighlightQRC;

flat out int vIsHighlight;

void main() {
  gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
  gl_PointSize = 3.0;

  // mark highlights (exact axial match)
  vIsHighlight = int(all(equal(aQRC, uHighlightQRC)));
}
