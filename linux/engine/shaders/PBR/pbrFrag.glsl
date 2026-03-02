#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

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
uniform vec3 uViewPos;

// IBL Uniforms
uniform samplerCube irradianceMap;   // For Diffuse IBL
uniform samplerCube prefilterMap;    // For Specular IBL (MIP-mapped)
uniform sampler2D   brdfLUT;         // For Specular IBL (2D Look-up Table)
uniform bool        uHasIBL;         // Toggle for environment lighting
uniform float       uIBLIntensity;   // Intensity for environment lighting

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
    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
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
    // 1. Albedo
    vec4 albedoSample = vec4(uDiffuseColor, 1.0);
    if (uHasDiffuseTexture && !uDebugDisableDiffuseTex) {
        albedoSample = texture(uDiffuseTexture, TexCoord);
    }
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)); // Gamma correction for PBR
    float alpha = albedoSample.a * uOpacity;
    
    // 2. Normal
    vec3 N = normalize(Normal);
    if (uHasNormalTexture && !uDebugDisableNormalTex) {
        vec3 tangentNormal = texture(uNormalTexture, TexCoord).xyz * 2.0 - 1.0;
        mat3 TBN = getTBN(N, FragPos, TexCoord);
        N = normalize(TBN * tangentNormal);
    }

    // 3. Metallic / Roughness
    float metallic = 0.0;
    if (uDebugOverrideMetallic) {
        metallic = uDebugMetallic;
    } else if (uHasMetallicMap && !uDebugDisableMetallicTex) {
        // glTF ORM: R=Occlusion, G=Roughness, B=Metallic
        metallic = texture(uMetallicMap, TexCoord).b;
    }

    float roughness = 0.5;
    if (uDebugOverrideRoughness) {
        roughness = uDebugRoughness;
    } else if (uHasRoughnessMap && !uDebugDisableRoughnessTex) {
        // glTF ORM: G = Roughness
        roughness = texture(uRoughnessMap, TexCoord).g;
    } else {
        roughness = sqrt(2.0 / (uShininess + 2.0));
    }
    
    // 4. AO
    float ao = 1.0;
    if (uHasAOMap && !uDebugDisableAOTex) {
        // glTF ORM: R = Occlusion
        float aoSample = texture(uAOMap, TexCoord).r;
        ao = mix(1.0, aoSample, uDebugAOStrength > 0.0 ? uDebugAOStrength : 1.0);
    }

    // PBR Loop
    vec3 V = normalize(uViewPos - FragPos);

    // Calculate F0
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    // Only one light source for now (point light at uLightPos)
    vec3 L = normalize(uLightPos - FragPos);
    vec3 H = normalize(V + L);
    float distance = length(uLightPos - FragPos);
    
    // Attenuation (physically correct quadratic)
    float attenuation = 1.0 / (distance * distance);
    // Simple tweak to make it visible in typical game scales if light is weak
    // attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
    vec3 radiance = uLightColor * attenuation * 500.0; // Scaled up radiance for visibility

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
float G   = GeometrySmith(N, V, L, roughness);      
vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0, roughness); // Direct Fresnel

vec3 numerator    = NDF * G * F; 
float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
vec3 specular     = numerator / denominator;

vec3 kS = F;
vec3 kD = vec3(1.0) - kS;
kD *= 1.0 - metallic;

float NdotL = max(dot(N, L), 0.0);        
Lo += (kD * albedo / PI + specular) * radiance * NdotL;

// --- IBL Section ---
vec3 ambient = vec3(0.0);
if (uHasIBL) {
    // 1. Calculate Fresnel for the environment (Indirect Fresnel)
    vec3 F_ibl = fresnelSchlick(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS_ibl = F_ibl;
    vec3 kD_ibl = 1.0 - kS_ibl;
    kD_ibl *= 1.0 - metallic;	  

    // 2. Diffuse IBL (Irradiance)
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    // 3. Specular IBL (Prefilter + LUT)
    vec3 R = reflect(-V, N); 
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);

    // 4. Combine Ambient IBL with intensity
    ambient = (kD_ibl * diffuseIBL + specularIBL) * ao * uIBLIntensity;
} else {
    // Fallback: Simple ambient when IBL is disabled
    ambient = vec3(0.03) * albedo * ao;
}
    
    // Emissive
    vec3 emissive = vec3(0.0);
    float emissiveBoost = uDebugEmissiveIntensity > 0.0 ? uDebugEmissiveIntensity : 5.0;
    if (uHasEmissiveMap && !uDebugDisableEmissiveTex) {
         emissive = texture(uEmissiveMap, TexCoord).rgb * emissiveBoost;
    } else if (uIsEmissive) {
         emissive = albedo * 2.0; 
    }

    vec3 color = ambient + Lo + emissive;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, alpha);
}
