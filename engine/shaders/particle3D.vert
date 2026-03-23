#version 460 core
// 3D Instancing Arrays (Attributes 0..3 are the Mesh obj attributes!)
layout (location = 0) in vec3 aMeshPos;
layout (location = 1) in vec3 aMeshNormal;
layout (location = 2) in vec3 aMeshColor;
layout (location = 3) in vec2 aMeshTex;

// Instancing Arrays (Attributes 4..6 are the Particle Emitter attributes)
layout (location = 4) in vec3 iPos;
layout (location = 5) in float iSize;
layout (location = 6) in vec4 iColor;

uniform mat4 uProjection;
uniform mat4 uView;

uniform bool uEnableDistortion;
uniform float uNoiseScale;
uniform float uDistortion;

out vec4 ParticleColor;
out vec3 ViewPos;
out vec3 WorldPos;
out float ParticleSize;
out vec3 FragNormal;
out vec2 TexCoords;

float noise3D(vec3 p) {
    vec3 i = floor(p); vec3 f = fract(p);
    vec3 u = f*f*(3.0-2.0*f);
    float n000 = fract(sin(dot(i+vec3(0,0,0), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n100 = fract(sin(dot(i+vec3(1,0,0), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n010 = fract(sin(dot(i+vec3(0,1,0), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n110 = fract(sin(dot(i+vec3(1,1,0), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n001 = fract(sin(dot(i+vec3(0,0,1), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n101 = fract(sin(dot(i+vec3(1,0,1), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n011 = fract(sin(dot(i+vec3(0,1,1), vec3(12.9898,78.233,151.7182)))*43758.5453);
    float n111 = fract(sin(dot(i+vec3(1,1,1), vec3(12.9898,78.233,151.7182)))*43758.5453);
    return mix(mix(mix(n000,n100,u.x), mix(n010,n110,u.x), u.y), mix(mix(n001,n101,u.x), mix(n011,n111,u.x), u.y), u.z);
}

void main()
{
    ParticleColor = iColor;
    ParticleSize = iSize;
    TexCoords = aMeshTex;
    
    vec3 localPos = aMeshPos;
    
    if (uEnableDistortion) {
        float n = noise3D( (iPos + localPos*iSize) * uNoiseScale );
        // Perturb along the mesh normal
        localPos += aMeshNormal * (n - 0.5) * uDistortion;
    }
    
    FragNormal = mat3(uView) * aMeshNormal;
    
    WorldPos = localPos * iSize + iPos;
    vec4 viewPos = uView * vec4(WorldPos, 1.0);
    ViewPos = viewPos.xyz;
    gl_Position = uProjection * viewPos;
}
