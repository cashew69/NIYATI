#version 460 core

layout(isolines, equal_spacing, ccw) in;

in vec3 tcNormal[];
in vec2 tcTexCoord[];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform sampler2D uHeightMap;

void main(void)
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Texture Coordinates - Bilinear Interpolation
    vec2 texCoord00 = tcTexCoord[0]; // bottom-left
    vec2 texCoord10 = tcTexCoord[1]; // bottom-right
    vec2 texCoord01 = tcTexCoord[2]; // top-left
    vec2 texCoord11 = tcTexCoord[3]; // top-right

    vec2 texBottom = mix(texCoord00, texCoord10, u);
    vec2 texTop = mix(texCoord01, texCoord11, u);
    TexCoord = mix(texBottom, texTop, v);

    // Sample heightmap
    float height = texture(uHeightMap, TexCoord).r * 64.0 - 16.0;
// Replace texture sampling with forced wave pattern:
    //float height = 50.0 * sin(TexCoord.x * 3.14159) * sin(TexCoord.y * 3.14159);

    // Position Coordinates - Bilinear Interpolation
    vec4 p00 = gl_in[0].gl_Position; // bottom-left
    vec4 p10 = gl_in[1].gl_Position; // bottom-right
    vec4 p01 = gl_in[2].gl_Position; // top-left
    vec4 p11 = gl_in[3].gl_Position; // top-right

    vec4 pBottom = mix(p00, p10, u);
    vec4 pTop = mix(p01, p11, u);
    vec4 pos = mix(pBottom, pTop, v);

    // Normal - Bilinear Interpolation
    vec3 n0 = tcNormal[0];
    vec3 n1 = tcNormal[1];
    vec3 n2 = tcNormal[2];
    vec3 n3 = tcNormal[3];
    
    vec3 norm0 = mix(n0, n1, u);
    vec3 norm1 = mix(n2, n3, u);
    vec3 normal = normalize(mix(norm0, norm1, v));

    // Displace position by height
    pos.y += height;

    // Transform to world space
    vec4 worldPos = uModel * pos;
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(uModel))) * normal;

    // Transform to clip space
    gl_Position = uProjection * uView * worldPos;
}
