#version 430 core

out vec4 FragColor;

flat in int vAttached;
flat in int vBoundary;

// 0 = opaque attached, 1 = translucent boundary
uniform int uPass; 
uniform int uShowBoundary;

void main() {
    if (uPass == 0) {
        // attached 
        if (vAttached == 0) discard;
        FragColor = vec4(0.6, 0.8, 1.0, 1.0);
        return;
    } 
    else {
        // boundary  
        if (vBoundary == 0 || vAttached != 0 || uShowBoundary == 0) discard;
        FragColor = vec4(0.8, 0.2, 0.1, 0.1);
        return;
    }
}
