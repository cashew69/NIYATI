#version 460 core
in vec4 ParticleColor;
in vec3 ViewPos;
in vec3 WorldPos;
in float ParticleSize;
in vec3 FragNormal;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D uParticleNormalMap;
uniform sampler2D uDepthMask;
uniform mat4 uProjection;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uAmbientStrength;
uniform mat4 uView;

uniform bool uEnableDistortion;

// 3D Shader Controls
uniform float uEmissionMix;
uniform float uEmissionStrength;
uniform vec3  uEmissionColor;
uniform float uParticleTransparency;

void main()
{
    vec3 normalView;
    if (uEnableDistortion) {
        // Magical recalculation of surface normals using faceted derivatives
        normalView = normalize(cross(dFdx(ViewPos), dFdy(ViewPos)));
    } else {
        // Smoothly interpolated mesh normal (view space) mixed with texture (if valid)
        // Since icosphere might have weird tex coords, let's just stick to geometric normal
        normalView = normalize(FragNormal);
    }
    
    // --- Soft Particle Fade against Scene Geometry ---
    float alpha = ParticleColor.a * uParticleTransparency;
    
    // Soften 3D Mesh Silhouettes (Rim Fade)
    // Absolute dot product secures against inverted normals.
    // We constrain the fade to retain a small base opacity at the extremities 
    // to prevent small meshes from becoming entirely invisible.
    vec3 smoothNormalView = normalize(FragNormal);
    vec3 viewDir = normalize(-ViewPos); 
    float rimDot = abs(dot(smoothNormalView, viewDir));
    float rimFade = smoothstep(0.0, 0.45, rimDot);
    
    // Multiply alpha but never kill it 100% just from the rim angle alone to guarantee visibility
    alpha *= mix(0.15, 1.0, rimFade);
    
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
    
    // Light calculation
    vec3 lightPosView = vec3(uView * vec4(uLightPos, 1.0));
    vec3 lightDirView = normalize(lightPosView - ViewPos);
    
    // Wrap diffuse (soft shading for smoke particles)
    float wrapDiff = dot(normalView, lightDirView) * 0.5 + 0.5;
    vec3 diffuseLight = wrapDiff * uLightColor;
    vec3 ambientLight = uAmbientStrength * uLightColor;
    
    vec3 baseColor = (ambientLight + diffuseLight) * ParticleColor.rgb;
    
    // User requested equation:
    // "((Diffuse light mixed with emission ) mixed with transparent)"
    vec3 emission = uEmissionColor * uEmissionStrength;
    
    // Mix diffuse with emission
    vec3 mixedColor = mix(baseColor, emission, uEmissionMix);
    
    // Result
    FragColor = vec4(mixedColor, alpha);
}
