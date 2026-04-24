#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec3 aColor;


uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
}

