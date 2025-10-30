#version 430 core

flat in int vIsHighlight;
out vec4 FragColor;

void main() {
  vec3 c = vec3(0.9, 0.9, 0.9);
  if (vIsHighlight == 1) {
    c = vec3(1.0, 0.0, 0.0);
  }
  
  FragColor = vec4(c, 1.0);
}