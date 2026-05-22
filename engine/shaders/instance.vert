#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aTexCoord;

layout(location = 4) in mat4 aInstanceMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 ShadowCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uShadowMatrix;

void main() {
    mat4 worldModel = uModel * aInstanceMatrix;
    FragPos = vec3(worldModel * vec4(aPosition, 1.0));
    Normal = mat3(transpose(inverse(worldModel))) * aNormal;
    TexCoord = aTexCoord;

    // Scale and bias to map from [-1, 1] to [0, 1]
    // uShadowMatrix should be LightProj * LightView
    vec4 sc = uShadowMatrix * vec4(FragPos, 1.0);
    ShadowCoord = vec4(sc.xyz * 0.5 + 0.5 * sc.w, sc.w);

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
