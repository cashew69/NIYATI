#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 ShadowCoord;

layout (location = 0) out vec4 FragColor;

// Material Uniforms (matching modelloading.cpp + PBR extensions)
uniform vec3 uDiffuseColor;
uniform bool uHasDiffuseTexture;
uniform sampler2D uDiffuseTexture;

uniform bool uHasNormalTexture;
uniform sampler2D uNormalTexture; // In slot 1

// uniform vec3 uSpecularColor;
uniform float uShininess;
uniform float uOpacity;
uniform bool uIsEmissive;

// PBR specific (future proofing, not currently set by C++)
uniform sampler2D uMetallicMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;

uniform bool uHasMetallicMap;
uniform bool uHasRoughnessMap;
uniform bool uHasAOMap;
uniform bool uHasEmissiveMap;

// Lights
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec3 uViewPos;

uniform int   uLightType;     // 0: Dir, 1: Point, 2: Spot
uniform vec3  uLightDir;
uniform float uLightRadius;
uniform float uInnerCutoff;
uniform float uOuterCutoff;

// IBL Uniforms
uniform samplerCube irradianceMap;   // For Diffuse IBL
uniform samplerCube prefilterMap;    // For Specular IBL (MIP-mapped)
uniform sampler2D   brdfLUT;         // For Specular IBL (2D Look-up Table)
uniform bool        uHasIBL;         // Toggle for environment lighting
uniform float       uIBLIntensity;   // Intensity for environment lighting
uniform float       uRoughness;      // Fallback roughness
uniform float       uMetalness;      // Fallback metalness

// Debug Overrides
uniform bool uDebugDisableDiffuseTex;
uniform bool uDebugDisableNormalTex;
uniform bool uDebugDisableMetallicTex;
uniform bool uDebugDisableRoughnessTex;
uniform bool uDebugDisableAOTex;
uniform bool uDebugDisableEmissiveTex;
uniform bool uDebugOverrideRoughness;
uniform bool uDebugOverrideMetallic;
uniform float uDebugRoughness;
uniform float uDebugMetallic;
uniform float uDebugAOStrength;
uniform float uDebugEmissiveIntensity;

// Fog
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogStart;
uniform float uFogEnd;
uniform int uFogType;
uniform bool uFogEnabled;

// Shadow
layout(binding = 9) uniform sampler2DShadow uShadowMap;
uniform bool  uShadowEnabled;
uniform float uShadowBias;

float calculateFog(float dist) {
    float fogFactor = 0.0;
    if (uFogType == 0) { // Linear
        fogFactor = (uFogEnd - dist) / (uFogEnd - uFogStart);
    } else if (uFogType == 1) { // Exp
        fogFactor = exp(-uFogDensity * dist);
    } else if (uFogType == 2) { // Exp2
        fogFactor = exp(-pow(uFogDensity * dist, 2.0));
    }
    return 1.0 - clamp(fogFactor, 0.0, 1.0);
}

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// TBN Calculation without pre-computed tangents
// http://www.thetenthplanet.de/archives/1180
mat3 getTBN(vec3 N, vec3 p, vec2 uv)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // solve the linear system
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame 
    float magSq = max(dot(T,T), dot(B,B));
    if (magSq < 1e-10) return mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), N);
    
    float invmax = inversesqrt(magSq);
    return mat3(T * invmax, B * invmax, N);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // avoid divide by zero
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0000001);
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------

void main()
{
    vec4 albedoSample = vec4(uDiffuseColor, 1.0);
    if (uHasDiffuseTexture && !uDebugDisableDiffuseTex) {
        albedoSample = texture(uDiffuseTexture, TexCoord);
    }
    
    vec3 N = length(Normal) > 0.001 ? normalize(Normal) : vec3(0.0, 1.0, 0.0);
    if (uHasNormalTexture && !uDebugDisableNormalTex) {
        vec3 tangentNormal = texture(uNormalTexture, TexCoord).xyz * 2.0 - 1.0;
        mat3 TBN = getTBN(N, FragPos, TexCoord);
        vec3 tn = TBN * tangentNormal;
        N = length(tn) > 0.001 ? normalize(tn) : N;
    }
    
    // 3. Metallic / Roughness
    float metallic = uMetalness;
    if (uHasMetallicMap && !uDebugDisableMetallicTex) {
        metallic *= texture(uMetallicMap, TexCoord).b;
    }
    if (uDebugOverrideMetallic) {
        metallic = uDebugMetallic;
    }

    float roughness = uRoughness;
    if (uHasRoughnessMap && !uDebugDisableRoughnessTex) {
        roughness *= texture(uRoughnessMap, TexCoord).g;
    }
    if (uDebugOverrideRoughness) {
        roughness = uDebugRoughness;
    }

    // Fallback for legacy models with 0 roughness
    if (roughness <= 0.001 && !uHasRoughnessMap && !uDebugOverrideRoughness) {
        roughness = sqrt(2.0 / (max(uShininess, 0.001) + 2.0));
    }
    
    vec3 viewDir = uViewPos - FragPos;
    vec3 V = length(viewDir) > 0.001 ? normalize(viewDir) : vec3(0.0, 1.0, 0.0);

    vec3 L;
    float attenuation = 1.0;

    if (uLightType == 0) { // Directional
        L = normalize(-uLightDir);
        attenuation = 1.0;
    } else { // Point or Spot
        vec3 lightToFrag = uLightPos - FragPos;
        float distance = length(lightToFrag);
        L = normalize(lightToFrag);
        
        // Quadratic attenuation with range limit
        attenuation = 1.0 / (distance * distance + 0.0001);
        
        // Smoothly fade out at radius
        if (uLightRadius > 0.0) {
            float distFactor = distance / uLightRadius;
            attenuation *= clamp(1.0 - distFactor * distFactor, 0.0, 1.0);
        }

        if (uLightType == 2) { // Spot Light
            float theta = dot(L, normalize(-uLightDir));
            float epsilon = uInnerCutoff - uOuterCutoff;
            float spotIntensity = clamp((theta - uOuterCutoff) / epsilon, 0.0, 1.0);
            attenuation *= spotIntensity;
        }
    }
    
    // Calculate F0
    vec3 albedo = pow(max(albedoSample.rgb, 0.0001), vec3(2.2));
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    vec3 halfDir = V + L;
    vec3 H = length(halfDir) > 0.001 ? normalize(halfDir) : vec3(0.0, 1.0, 0.0);
    
    vec3 radiance = uLightColor * attenuation * uLightIntensity; 

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0, roughness);

    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    // Shadow — attenuates direct lighting only, ambient is unaffected
    float shadow = 1.0;
    if (uShadowEnabled) {
        vec4 sc = ShadowCoord;
        if (sc.w > 0.0 && sc.z <= sc.w) {
            sc.z -= uShadowBias * sc.w;
            shadow = textureProj(uShadowMap, sc);
        }
    }
    Lo *= shadow;

    // 4. AO (Temporarily disabled for stability)
    float ao = 1.0;

    // 5. Ambient / IBL
    vec3 ambient = vec3(0.03) * albedo * ao;
    if (uHasIBL) {
        vec3 F_ibl = fresnelSchlick(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS_ibl = F_ibl;
        vec3 kD_ibl = 1.0 - kS_ibl;
        kD_ibl *= 1.0 - metallic;	  
        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuseIBL = irradiance * albedo;
        vec3 R = reflect(-V, N); 
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;    
        vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);
        ambient = (kD_ibl * diffuseIBL + specularIBL) * ao * uIBLIntensity;
    }
    
    // Emissive
    vec3 emissive = vec3(0.0);
    if (uHasEmissiveMap && !uDebugDisableEmissiveTex) {
         emissive = texture(uEmissiveMap, TexCoord).rgb * (uDebugEmissiveIntensity > 0.0 ? uDebugEmissiveIntensity : 5.0);
    } else if (uIsEmissive) {
         emissive = albedo * 2.0; 
    }

    vec3 hdrColor = ambient + Lo + emissive;

    // Tone mapping and gamma correction
    vec3 color = hdrColor / (hdrColor + vec3(1.0));
    color = pow(max(color, 0.0001), vec3(1.0/2.2));

    if (uFogEnabled) {
        float dist = length(uViewPos - FragPos);
        float fogFactor = calculateFog(dist);
        color = mix(color, pow(max(uFogColor, 0.0), vec3(1.0/2.2)), fogFactor);
    }

    float alpha = uOpacity;
    FragColor = vec4(color, alpha);
}
