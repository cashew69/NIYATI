#version 460 core
in vec4 ParticleColor;
in vec3 ViewPos;
in vec3 WorldPos;
in float ParticleSize;

out vec4 FragColor;

uniform sampler2D uParticleNormalMap;
uniform sampler2D uDepthMask;
uniform mat4 uProjection;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uAmbientStrength;
uniform mat4 uView;

uniform bool uIs3D;
uniform bool uEnableDistortion;

// Noise and Merge options provided via C++ ParticleEmitter
uniform float uNoiseScale;
uniform float uDistortion;
uniform float uMergeNormals;

// 3D Value Noise for spatial structural distortion
float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    
    float n000 = fract(sin(dot(i + vec3(0.0, 0.0, 0.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n100 = fract(sin(dot(i + vec3(1.0, 0.0, 0.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n010 = fract(sin(dot(i + vec3(0.0, 1.0, 0.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n110 = fract(sin(dot(i + vec3(1.0, 1.0, 0.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n001 = fract(sin(dot(i + vec3(0.0, 0.0, 1.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n101 = fract(sin(dot(i + vec3(1.0, 0.0, 1.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n011 = fract(sin(dot(i + vec3(0.0, 1.0, 1.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    float n111 = fract(sin(dot(i + vec3(1.0, 1.0, 1.0), vec3(12.9898, 78.233, 151.7182))) * 43758.5453);
    
    float mixX0 = mix(n000, n100, u.x);
    float mixX1 = mix(n010, n110, u.x);
    float mixX2 = mix(n001, n101, u.x);
    float mixX3 = mix(n011, n111, u.x);
    
    float mixY0 = mix(mixX0, mixX1, u.y);
    float mixY1 = mix(mixX2, mixX3, u.y);
    
    return mix(mixY0, mixY1, u.z);
}

void main()
{
    float alpha = ParticleColor.a;
    vec3 normalView;
    vec3 fragWorldPos;
    
    if (!uIs3D) {
        // Point coordinate inside point sprite
        vec2 coord = gl_PointCoord - vec2(0.5);
        
        // Estimate fragment world position using inverse view matrix (camera transform)
        mat3 invViewMat = inverse(mat3(uView));
        vec3 rightWorld = invViewMat * vec3(1.0, 0.0, 0.0);
        vec3 upWorld = invViewMat * vec3(0.0, 1.0, 0.0);
        
        fragWorldPos = WorldPos + (coord.x * rightWorld - coord.y * upWorld) * ParticleSize;
        
        // 1. Distort Sphere Shape with 3D Noise
        float nVal = 0.5;
        if (uEnableDistortion) {
            nVal = noise3D(fragWorldPos * uNoiseScale); // 0.0 to 1.0
        }
        
        float baseDist = length(coord);
        float distanceToCenter = baseDist - (nVal - 0.5) * uDistortion;
        if (distanceToCenter > 0.5) discard;
        alpha *= smoothstep(0.5, 0.1, distanceToCenter);
        
        // 2. Normal Map & Merge
        vec2 uv = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
        vec3 texNormal = texture(uParticleNormalMap, uv).rgb * 2.0 - 1.0;
        
        float internalDepth = sqrt(max(0.0, 1.0 - (baseDist * 2.0)*(baseDist * 2.0)));
        vec3 sphereNormal = vec3(coord.x * 2.0, -coord.y * 2.0, internalDepth);
        
        vec3 baseNormalView = mix(sphereNormal, texNormal, 0.5);
        
        if (uEnableDistortion) {
            float nx = noise3D(fragWorldPos * uNoiseScale + vec3(10.0)) * 2.0 - 1.0;
            float ny = noise3D(fragWorldPos * uNoiseScale + vec3(20.0)) * 2.0 - 1.0;
            vec3 noiseNormal = vec3(nx, ny, 0.0) * uDistortion * 2.0;    
            baseNormalView = normalize(baseNormalView + noiseNormal);
        }
        
        float mergeFactor = clamp((distanceToCenter * 2.0) * uMergeNormals, 0.0, 1.0);
        vec3 flatNormal = vec3(0.0, 0.0, 1.0);
        
        normalView = normalize(mix(baseNormalView, flatNormal, mergeFactor));
    } else {
        // 3D Geometry Instancing Mode
        fragWorldPos = WorldPos;
        
        if (uEnableDistortion) {
            // Magical recalculation of surface normals! 
            // the distorted icosphere will have perfectly calculated facet normals 
            // simply derived from screen-space vertex derivatives!
            normalView = normalize(cross(dFdx(ViewPos), dFdy(ViewPos)));
        } else {
            // For perfectly smooth icosphere shading without distortion
            // (Assuming FragNormal is supplied correctly, but for this context
            // we will also geometrically resolve it to maintain safety)
            normalView = normalize(cross(dFdx(ViewPos), dFdy(ViewPos)));
        }
    }
    
    // --- Soft Particle Fade against Scene Geometry (Applies to both 2D and 3D) ---
    ivec2 screenTexCoords = ivec2(gl_FragCoord.xy);
    ivec2 texSize = textureSize(uDepthMask, 0);
    if (texSize.x > 1) {
        float sceneDepth = texelFetch(uDepthMask, screenTexCoords, 0).r;
        if (sceneDepth > 0.0 && sceneDepth < 1.0 && gl_FragCoord.z < sceneDepth) {
            float ndcZ = sceneDepth * 2.0 - 1.0;
            vec4 clipPos = vec4(0.0, 0.0, ndcZ, 1.0);
            vec4 viewPosScene = inverse(uProjection) * clipPos;
            float sceneZ = viewPosScene.z / viewPosScene.w;
            
            float depthDiff = ViewPos.z - sceneZ;
            float softFade = clamp(depthDiff * 0.5, 0.0, 1.0);
            alpha *= softFade;
        }
    }
    
    // Light is provided in world space
    vec3 lightPosView = vec3(uView * vec4(uLightPos, 1.0));
    vec3 lightDirView = normalize(lightPosView - ViewPos);
    
    float wrapDiff = dot(normalView, lightDirView) * 0.5 + 0.5;
    vec3 diffuse = wrapDiff * uLightColor;
    vec3 ambient = uAmbientStrength * uLightColor;
    
    vec3 finalColor = (ambient + diffuse) * ParticleColor.rgb;
    
    FragColor = vec4(finalColor, alpha);
}
