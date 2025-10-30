#version 430 core

out vec4 FragColor;

flat in int vAttached;

void main() {
    if (vAttached == 0) discard;
    FragColor = vec4(0.1, 0.1, 0.1, 1.0);
}