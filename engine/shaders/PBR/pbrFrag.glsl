#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 ShadowCoord;

layout(location = 0) out vec4 FragColor;

// Material Uniforms
uniform vec3 uDiffuseColor;
uniform bool uHasDiffuseTexture;
uniform sampler2D uDiffuseTexture;

uniform bool uHasNormalTexture;
uniform sampler2D uNormalTexture;

uniform float uShininess;
uniform float uOpacity;
uniform bool uIsEmissive;

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

uniform int uLightType; // 0: Dir, 1: Point, 2: Spot
uniform vec3 uLightDir;
uniform float uLightRadius;
uniform float uInnerCutoff;
uniform float uOuterCutoff;

// IBL Uniforms
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform bool uHasIBL;
uniform float uIBLIntensity;
uniform float uRoughness;
uniform float uMetalness;

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
uniform float uShadowMinLight;

// Diffuse model
uniform bool uUseOrenNayar;

// Stochastic Tiling
uniform bool  uEnableStochastic;
uniform float uStochasticContrast;
uniform float uStochasticScale;
uniform float uUVScale;

// Terrain overlay (footprint normal map baked onto a terrain-UV texture)
uniform bool      uHasOverlayTexture;
uniform sampler2D uOverlayTexture;

// Aerial perspective
uniform bool      uAerialPerspective;
uniform sampler2D uAerialTransmittanceLUT;
uniform sampler2D uAerialSkyViewLUT;
uniform float     uAtmBotR;
uniform float     uAtmTopR;
uniform float     uAtmCamHeight;
uniform float     uAtmWorldScale;
uniform float     uAtmExposure;

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

// Stochastic Tiling Helpers
vec4 hash_v4(vec2 p) {
    return fract(sin(vec4(dot(p, vec2(127.1, 311.7)), 
                         dot(p, vec2(269.5, 183.3)), 
                         dot(p, vec2(419.2, 371.9)), 
                         dot(p, vec2(231.7, 124.5))) ) * 43758.5453);
}

vec3 textureStochastic(sampler2D tex, vec2 uv, float contrast) {
    vec2 p = floor(uv);
    vec2 f = fract(uv);

    vec3 color = vec3(0.0);
    float totalWeight = 0.0;

    for(int j=0; j<=1; j++) {
        for(int i=0; i<=1; i++) {
            vec2 b = vec2(float(i), float(j));
            vec4 h = hash_v4(p + b);
            
            float angle = h.z * 6.2831;
            float cosA = cos(angle);
            float sinA = sin(angle);
            mat2 rot = mat2(cosA, -sinA, sinA, cosA);
            
            vec2 offset = h.xy * 123.4;
            vec2 sampledUV = rot * uv + offset;
            
            float weight = smoothstep(1.0, 0.0, length(f - b));
            weight = pow(weight, contrast); 

            color += texture(tex, sampledUV).rgb * weight;
            totalWeight += weight;
        }
    }
    return color / max(totalWeight, 0.0001);
}

mat3 getTBN(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float magSq = max(dot(T, T), dot(B, B));
    if (magSq < 1e-10) return mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), N);

    float invmax = inversesqrt(magSq);
    return mat3(T * invmax, B * invmax, N);
}

// Oren-Nayar diffuse — models rough Lambertian surfaces where inter-microfacet
// scattering fills in grazing-angle darkness. Reduces to Lambertian at sigma=0.
// Returns full diffuse radiance (already includes NdotL and 1/PI).
vec3 orenNayarDiffuse(vec3 albedo, vec3 N, vec3 V, vec3 L, float sigma) {
    float sigma2 = sigma * sigma;
    float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
    float B = 0.45 * sigma2 / (sigma2 + 0.09);

    // Clamp strictly to [0,1] so the sqrt arguments are never negative.
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // Azimuthal cosine difference between projected L and V onto tangent plane.
    // Safe-normalize: when V≈N (top-down view) the projected vector is near-zero;
    // fall back to a harmless direction rather than producing NaN.
    vec3  Lraw       = L - N * NdotL;
    vec3  Vraw       = V - N * NdotV;
    vec3  Lperp      = length(Lraw) > 1e-4 ? normalize(Lraw) : vec3(1.0, 0.0, 0.0);
    vec3  Vperp      = length(Vraw) > 1e-4 ? normalize(Vraw) : vec3(1.0, 0.0, 0.0);
    float cosPhiDiff = max(dot(Lperp, Vperp), 0.0);

    float minDot    = min(NdotL, NdotV);
    float maxDot    = max(NdotL, NdotV);
    float sinAlpha  = sqrt(max(0.0, 1.0 - minDot * minDot));
    float tanBeta   = sqrt(max(0.0, 1.0 - maxDot * maxDot)) / max(maxDot, 0.0001);

    return albedo / PI * NdotL * (A + B * cosPhiDiff * sinAlpha * tanBeta);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 atm_sampleTransmittance(float viewH, float cosAngle) {
    float H    = sqrt(max(0.0, uAtmTopR*uAtmTopR - uAtmBotR*uAtmBotR));
    float rho  = sqrt(max(0.0, viewH*viewH - uAtmBotR*uAtmBotR));
    float disc = viewH*viewH*(cosAngle*cosAngle - 1.0) + uAtmTopR*uAtmTopR;
    if (disc < 0.0) return vec3(0.0);
    float d    = -viewH*cosAngle + sqrt(disc);
    float dMin = uAtmTopR - viewH;
    float dMax = rho + H;
    float u    = (dMax > dMin + 1e-6) ? (d - dMin) / (dMax - dMin) : 0.0;
    float v    = (H > 1e-6) ? rho / H : 0.0;
    return texture(uAerialTransmittanceLUT, vec2(clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0))).rgb;
}

vec2 atm_dirToSkyUV(vec3 dir) {
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float lon = atan(dir.x, dir.z);
    if (lon < 0.0) lon += 2.0 * PI;
    float u = lon / (2.0 * PI);
    float halfPi = PI * 0.5;
    float v;
    if (lat < 0.0) {
        float t = sqrt(clamp(-lat / halfPi, 0.0, 1.0));
        v = 0.5 * (1.0 - t);
    } else {
        float t = sqrt(clamp(lat / halfPi, 0.0, 1.0));
        v = 0.5 + 0.5 * t;
    }
    return vec2(u, v);
}

void main()
{
    float stochContrast = uStochasticContrast > 0.0 ? uStochasticContrast : 8.0;
    float stochScale = uStochasticScale > 0.0 ? uStochasticScale : 1.0;
    
    vec2 uv_base = TexCoord;
    vec2 uv_stoch = uv_base * stochScale;

    vec4 albedoSample = vec4(uDiffuseColor, 1.0);
    if (uHasDiffuseTexture && !uDebugDisableDiffuseTex) {
        if (uEnableStochastic) {
            albedoSample = vec4(textureStochastic(uDiffuseTexture, uv_stoch, stochContrast), 1.0);
        } else {
            albedoSample = texture(uDiffuseTexture, uv_base);
        }
    }

    vec3 N = length(Normal) > 0.001 ? normalize(Normal) : vec3(0.0, 1.0, 0.0);
    if (uHasNormalTexture && !uDebugDisableNormalTex) {
        vec3 tangentNormal;
        if (uEnableStochastic) {
            tangentNormal = textureStochastic(uNormalTexture, uv_stoch, stochContrast) * 2.0 - 1.0;
        } else {
            tangentNormal = texture(uNormalTexture, uv_base).xyz * 2.0 - 1.0;
        }
        mat3 TBN = getTBN(N, FragPos, uv_base);
        vec3 tn = TBN * tangentNormal;
        N = length(tn) > 0.001 ? normalize(tn) : N;
    }

    // Terrain overlay: footprint normals baked in terrain-UV space.
    // rawUV is the unscaled [0,1] terrain UV (TexCoord was multiplied by uUVScale in the TES).
    if (uHasOverlayTexture) {
        vec2 rawUV = uUVScale > 0.001 ? TexCoord / uUVScale : TexCoord;
        vec4 ovl   = texture(uOverlayTexture, rawUV);
        if (ovl.a > 0.05) {
            vec3 ovlTangent = ovl.rgb * 2.0 - 1.0;
            mat3 TBN = getTBN(normalize(Normal), FragPos, rawUV);
            N = normalize(mix(N, TBN * ovlTangent, ovl.a));
        }
    }

    float metallic = uMetalness;
    if (uHasMetallicMap && !uDebugDisableMetallicTex) {
        if (uEnableStochastic) {
            metallic *= textureStochastic(uMetallicMap, uv_stoch, stochContrast).b;
        } else {
            metallic *= texture(uMetallicMap, uv_base).b;
        }
    }
    if (uDebugOverrideMetallic) {
        metallic = uDebugMetallic;
    }

    float roughness = uRoughness;
    if (uHasRoughnessMap && !uDebugDisableRoughnessTex) {
        if (uEnableStochastic) {
            roughness *= textureStochastic(uRoughnessMap, uv_stoch, stochContrast).g;
        } else {
            roughness *= texture(uRoughnessMap, uv_base).g;
        }
    }
    if (uDebugOverrideRoughness) {
        roughness = uDebugRoughness;
    }

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
        attenuation = 1.0 / (distance * distance + 0.0001);
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

    vec3 albedo = pow(max(albedoSample.rgb, 0.0001), vec3(2.2));
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    vec3 halfDir = V + L;
    vec3 H = length(halfDir) > 0.001 ? normalize(halfDir) : vec3(0.0, 1.0, 0.0);
    vec3 radiance = uLightColor * attenuation * uLightIntensity;

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuseTerm;
    if (uUseOrenNayar) {
        diffuseTerm = orenNayarDiffuse(albedo, N, V, L, roughness) * kD;
    } else {
        diffuseTerm = kD * albedo / PI * NdotL;
    }
    Lo += (diffuseTerm + specular * NdotL) * radiance;

    float shadow = 1.0;
    if (uShadowEnabled) {
        vec4 sc = ShadowCoord;
        if (sc.w > 0.0 && sc.z <= sc.w) {
            sc.z -= uShadowBias * sc.w;

            // 8-tap Poisson disk PCF for soft shadow edges
            const vec2 poissonDisk[8] = vec2[](
                vec2(-0.9450, -0.3250),
                vec2(-0.0940,  0.9290),
                vec2( 0.3450, -0.6790),
                vec2( 0.7360,  0.4680),
                vec2(-0.4730,  0.6850),
                vec2( 0.6640, -0.5410),
                vec2(-0.6520, -0.7010),
                vec2( 0.1520,  0.2290)
            );
            vec2 texelSize = 1.5 / vec2(textureSize(uShadowMap, 0));
            shadow = 0.0;
            for (int i = 0; i < 8; i++) {
                vec4 ssc = sc;
                ssc.xy += poissonDisk[i] * texelSize * sc.w;
                shadow += textureProj(uShadowMap, ssc);
            }
            shadow /= 8.0;
        }
    }
    Lo *= mix(uShadowMinLight, 1.0, shadow);

    float ao = 1.0;
    if (uHasAOMap && !uDebugDisableAOTex) {
        if (uEnableStochastic) {
            ao *= textureStochastic(uAOMap, uv_stoch, stochContrast).r;
        } else {
            ao *= texture(uAOMap, uv_base).r;
        }
    }

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
        vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);
        ambient = (kD_ibl * diffuseIBL + specularIBL) * ao * uIBLIntensity;
    }

    vec3 emissive = vec3(0.0);
    if (uHasEmissiveMap && !uDebugDisableEmissiveTex) {
        if (uEnableStochastic) {
            emissive = textureStochastic(uEmissiveMap, uv_stoch, stochContrast).rgb * (uDebugEmissiveIntensity > 0.0 ? uDebugEmissiveIntensity : 5.0);
        } else {
            emissive = texture(uEmissiveMap, uv_base).rgb * (uDebugEmissiveIntensity > 0.0 ? uDebugEmissiveIntensity : 5.0);
        }
    } else if (uIsEmissive) {
        emissive = albedo * 2.0;
    }

    vec3 hdrColor = ambient + Lo + emissive;
    vec3 color = hdrColor / (hdrColor + vec3(1.0));
    color = pow(max(color, 0.0001), vec3(1.0 / 2.2));

    if (uAerialPerspective) {
        vec3  viewDirAP  = normalize(FragPos - uViewPos);
        float distKm   = length(FragPos - uViewPos) * uAtmWorldScale;
        float distFade = smoothstep(0.5, 20.0, distKm);
        if (distFade > 0.001) {
            float fragH = max(uAtmBotR + 0.001, uAtmBotR + FragPos.y * uAtmWorldScale);
            float cosV  = abs(viewDirAP.y);
            vec3 T_cam  = atm_sampleTransmittance(uAtmCamHeight, cosV);
            vec3 T_frag = atm_sampleTransmittance(fragH, cosV);
            vec3 T_seg  = clamp(T_cam / max(T_frag, vec3(0.001)), 0.0, 1.0);
            vec3 skyL     = texture(uAerialSkyViewLUT, atm_dirToSkyUV(viewDirAP)).rgb;
            vec3 skyHDR   = skyL * uAtmExposure;
            vec3 skyToned = pow(max(skyHDR / (skyHDR + vec3(1.0)), vec3(0.0)), vec3(1.0 / 2.2));
            vec3 aerialColor = color * T_seg + skyToned * (vec3(1.0) - T_seg);
            color = mix(color, aerialColor, distFade);
        }
    }

    if (uFogEnabled) {
        float dist = length(uViewPos - FragPos);
        float fogFactor = calculateFog(dist);
        color = mix(color, pow(max(uFogColor, 0.0), vec3(1.0/2.2)), fogFactor);
    }

    float alpha = uOpacity;
    FragColor = vec4(color, alpha);
}
