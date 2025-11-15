#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aTexCoord;

// Instance matrix takes locations 4, 5, 6, 7 (4 vec4s for a mat4)
layout(location = 4) in mat4 aInstanceMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // Combine base model matrix with instance matrix
    mat4 finalModel = uModel * aInstanceMatrix;
    
    FragPos = vec3(finalModel * vec4(aPosition, 1.0));
    Normal = mat3(transpose(inverse(finalModel))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
