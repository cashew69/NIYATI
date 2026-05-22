#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 4) in mat4 aInstanceMatrix;

uniform mat4 uLightMVP; // lightProj * lightView * nodeWorldMatrix

void main() {
    gl_Position = uLightMVP * aInstanceMatrix * vec4(aPosition, 1.0);
}
