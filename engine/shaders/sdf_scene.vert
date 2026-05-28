#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 RayDir;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_cameraPos;

void main() {
    TexCoord = aTexCoord;
    
    // Calculate world-space ray direction for raymarching
    // We assume a full-screen quad or similar is being rendered
    vec4 viewSpacePos = inverse(u_projection) * vec4(aPos.xy, 1.0, 1.0);
    viewSpacePos /= viewSpacePos.w;
    vec4 worldSpacePos = inverse(u_view) * viewSpacePos;
    RayDir = normalize(worldSpacePos.xyz - u_cameraPos);
    
    gl_Position = vec4(aPos, 1.0);
}
