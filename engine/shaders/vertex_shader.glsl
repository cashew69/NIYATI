#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 ShadowCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uShadowMatrix; // scale_bias * lightProj * lightView * model (per object)

void main() {
    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord;
    
    // Scale and bias to map from [-1, 1] to [0, 1]
    // uShadowMatrix should now be LightProj * LightView
    vec4 sc = uShadowMatrix * vec4(FragPos, 1.0);
    ShadowCoord = vec4(sc.xyz * 0.5 + 0.5 * sc.w, sc.w);
    
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}

