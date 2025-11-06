#version 460 core

layout(quads, equal_spacing, ccw) in;

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
    vec2 t0 = tcTexCoord[0];
    vec2 t1 = tcTexCoord[1];
    vec2 t2 = tcTexCoord[2];
    vec2 t3 = tcTexCoord[3];

    //vec2 tx = mix(t0, t1, u);
    vec2 tx = (t1 - t0) * u + t0;
    //vec2 ty = mix(t2, t3, u);
    vec2 ty = (t3 - t2) * u + t2;
    TexCoord = (ty - tx) * v + tx;

    // Sample heightmap
    float height = texture(uHeightMap, TexCoord).y * 64.0 - 0.0;

    // Position Coordinates - Bilinear Interpolation
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    vec4 p2 = gl_in[2].gl_Position;
    vec4 p3 = gl_in[3].gl_Position;

    vec4 pos0 = mix(p0, p1, u);
    vec4 pos1 = mix(p2, p3, u);
    vec4 pos = mix(pos0, pos1, v);

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
