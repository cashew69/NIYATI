#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

// Material Uniforms
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform float uShininess;
uniform float uOpacity;
uniform bool uIsEmissive;

// Textures
uniform bool uHasDiffuseTexture;
uniform sampler2D uDiffuseTexture;
uniform bool uHasNormalTexture;
uniform sampler2D uNormalTexture;

// PBR Maps (ARM = AO, Roughness, Metallic packed in R, G, B)
uniform bool uHasMetallicMap;
uniform sampler2D uMetallicMap;
uniform bool uHasRoughnessMap;
uniform sampler2D uRoughnessMap;
uniform bool uHasAOMap;
uniform sampler2D uAOMap;

// Lights
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

// IBL Uniforms
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform bool uHasIBL;
uniform float uIBLIntensity;

// Terrain Maps
uniform sampler2D uHeightMap;
uniform sampler2D uDisplacementMap;
uniform bool uHasDisplacementMap;
uniform float uDisplacementScale;
uniform float uUVScale;

const float PI = 3.14159265359;

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

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
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

vec3 getTerrainColor(float height)
{
    float h = clamp(height / max(uDisplacementScale, 0.001), 0.0, 1.0);

    vec3 waterColor = vec3(0.1, 0.3, 0.5);
    vec3 sandColor = vec3(0.76, 0.70, 0.50);
    vec3 grassColor = vec3(0.2, 0.5, 0.2);
    vec3 rockColor = vec3(0.5, 0.45, 0.4);
    vec3 snowColor = vec3(0.95, 0.95, 0.98);

    vec3 color;
    if (h < 0.1) {
        color = mix(waterColor, sandColor, h / 0.1);
    } else if (h < 0.3) {
        color = mix(sandColor, grassColor, (h - 0.1) / 0.2);
    } else if (h < 0.6) {
        color = mix(grassColor, rockColor, (h - 0.3) / 0.3);
    } else if (h < 0.85) {
        color = rockColor;
    } else {
        color = mix(rockColor, snowColor, (h - 0.85) / 0.15);
    }

    return color;
}

void main()
{
    float heightSample = texture(uHeightMap, TexCoord).r;
    float terrainHeight = (heightSample - 0.5) * uDisplacementScale;

    // Albedo
    vec3 albedo;
    if (uHasDiffuseTexture) {
        albedo = texture(uDiffuseTexture, TexCoord * uUVScale).rgb;
    } else {
        albedo = getTerrainColor(terrainHeight);
    }
    albedo = pow(albedo, vec3(2.2));

    // Normal
    vec3 N = normalize(Normal);
    if (uHasNormalTexture) {
        vec3 tangentNormal = texture(uNormalTexture, TexCoord * uUVScale).xyz * 2.0 - 1.0;
        mat3 TBN = getTBN(N, FragPos, TexCoord * uUVScale);
        N = normalize(TBN * tangentNormal);
    }

    // PBR parameters from ARM texture or height-based fallback
    float roughness;
    float metallic;
    float ao;

    if (uHasMetallicMap) {
        vec3 arm = texture(uMetallicMap, TexCoord * uUVScale).rgb;
        ao = arm.r;
        roughness = arm.g;
        metallic = arm.b;
    } else {
        roughness = mix(0.3, 0.9, clamp(heightSample, 0.0, 1.0));
        metallic = 0.0;
        ao = 1.0;
    }

    vec3 V = normalize(uViewPos - FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting
    vec3 Lo = vec3(0.0);

    vec3 L = normalize(uLightPos - FragPos);
    vec3 H = normalize(V + L);
    float distance = length(uLightPos - FragPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = uLightColor * attenuation * 500.0;

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
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    // IBL Ambient
    vec3 ambient = vec3(0.0);
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
    } else {
        ambient = vec3(0.03) * albedo * ao;
    }

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
