#version 330 core
layout(location = 0) in vec3 aPosition;  // world-space billboard corner
layout(location = 3) in vec2 aTexCoord;

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vTexCoord;

void main() {
    vTexCoord   = aTexCoord;
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
}
