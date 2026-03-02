#version 460 core

layout(quads, equal_spacing, ccw) in;

// Inputs from main_tcs.glsl
in vec3 tcFragPos[];
in vec3 tcNormal[];
in vec2 tcTexCoord[];

// Outputs to pbrFrag.glsl
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 uView;
uniform mat4 uProjection;
uniform sampler2D uHeightMap;
uniform sampler2D uDisplacementMap;
uniform bool uHasDisplacementMap;
uniform float uDisplacementScale;
uniform float uUVScale;

float sampleHeight(vec2 uv) {
    float h = (texture(uHeightMap, uv).r - 0.5) * uDisplacementScale;
    if (uHasDisplacementMap) {
        float micro = texture(uDisplacementMap, uv * 100.0).r;
        h += (micro - 0.5) * uDisplacementScale * 0.02;
    }
    return h;
}

void main(void)
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec2 texCoord00 = tcTexCoord[0];
    vec2 texCoord10 = tcTexCoord[1];
    vec2 texCoord01 = tcTexCoord[2];
    vec2 texCoord11 = tcTexCoord[3];

    vec2 texBottom = mix(texCoord00, texCoord10, u);
    vec2 texTop = mix(texCoord01, texCoord11, u);
    vec2 baseTexCoord = mix(texBottom, texTop, v);

    // Sample height using unscaled UVs
    float height = sampleHeight(baseTexCoord);

    // FragPos is already world space from vertex shader, so just interpolate
    vec3 p00 = tcFragPos[0];
    vec3 p10 = tcFragPos[1];
    vec3 p01 = tcFragPos[2];
    vec3 p11 = tcFragPos[3];

    vec3 pBottom = mix(p00, p10, u);
    vec3 pTop = mix(p01, p11, u);
    vec3 pos = mix(pBottom, pTop, v);

    // Apply displacement
    pos.y += height;

    // Compute normal from heightmap using finite differences (using unscaled UVs)
    float texelSize = 1.0 / textureSize(uHeightMap, 0).x;
    float hL = sampleHeight(baseTexCoord + vec2(-texelSize, 0.0));
    float hR = sampleHeight(baseTexCoord + vec2( texelSize, 0.0));
    float hD = sampleHeight(baseTexCoord + vec2(0.0, -texelSize));
    float hU = sampleHeight(baseTexCoord + vec2(0.0,  texelSize));

    vec3 normal = normalize(vec3(hL - hR, 2.0, hD - hU));

    // Outputs for pbrFrag.glsl
    FragPos = pos;
    Normal = normal; 
    TexCoord = baseTexCoord * uUVScale; // scale UV for the PBR fragment textures!

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
