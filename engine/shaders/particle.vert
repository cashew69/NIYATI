#version 460 core
// 2D & Common Arrays
layout (location = 0) in vec3 aPos; 
layout (location = 1) in vec3 aNormalOrSize; // .x is Size in 2D
layout (location = 2) in vec4 aColorOrMeshColor; 

// 3D Instancing Arrays
layout (location = 4) in vec3 iPos;
layout (location = 5) in float iSize;
layout (location = 6) in vec4 iColor;

uniform mat4 uProjection;
uniform mat4 uView;

uniform bool uIs3D;
uniform bool uEnableDistortion;
uniform float uNoiseScale;
uniform float uDistortion;

out vec4 ParticleColor;
out vec3 ViewPos;
out vec3 WorldPos;
out float ParticleSize;
out vec3 FragNormal;

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
    if (uIs3D) {
        ParticleColor = iColor;
        ParticleSize = iSize;
        
        vec3 localPos = aPos;
        
        if (uEnableDistortion) {
            float n = noise3D( (iPos + localPos*iSize) * uNoiseScale );
            localPos += aNormalOrSize * (n - 0.5) * uDistortion;
        }
        
        FragNormal = mat3(uView) * aNormalOrSize;
        
        WorldPos = localPos * iSize + iPos;
        vec4 viewPos = uView * vec4(WorldPos, 1.0);
        ViewPos = viewPos.xyz;
        gl_Position = uProjection * viewPos;
    } else {
        ParticleColor = aColorOrMeshColor;
        ParticleSize = aNormalOrSize.x;
        WorldPos = aPos;
        FragNormal = vec3(0.0);
        
        vec4 viewPos = uView * vec4(aPos, 1.0);
        ViewPos = viewPos.xyz;
        gl_Position = uProjection * viewPos;
        
        gl_PointSize = (ParticleSize / -viewPos.z) * 100.0;
    }
}
