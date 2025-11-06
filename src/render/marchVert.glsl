#version 430
layout(location=0) in vec3 inPos;

uniform mat4 uModel, uView, uProj;
out vec3 vPosObj;

void main(){
  vPosObj = inPos;                          // object space
  gl_Position = uProj * uView * uModel * vec4(inPos,1.0);
}
