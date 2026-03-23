#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec3 aTexCoord; // Changed from vec2 to vec3

out vec3 FragPos;
out vec3 Normal;
out vec3 TexCoord3D; // Changed to vec3 for the fragment shader

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // Calculate world-space position and normal
    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;

    // Pass the 3D texture coordinate to the fragment shader
    TexCoord3D = aTexCoord;

    // Alternative approach for volumetric rendering:
    // If you don't want to pass explicit 3D UVs from your C++ vertex data,
    // you can simply use the local vertex position as your 3D texture coordinate:
    // TexCoord3D = aPosition;

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
