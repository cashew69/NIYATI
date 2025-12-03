#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec2 aTexCoord;

out vec2 uv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    uv = aTexCoord;
}
