#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec3 uViewPos;

uniform int   uLightType;     // 0: Dir, 1: Point, 2: Spot
uniform vec3  uLightDir;
uniform float uLightRadius;
uniform float uInnerCutoff;
uniform float uOuterCutoff;

uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform float uShininess;

uniform bool uIsEmissive;
uniform float uOpacity;
uniform bool uHasNormalTexture;
uniform sampler2D uNormalTexture;

uniform bool uHasDiffuseTexture;
uniform sampler2D uDiffuseTexture;
uniform sampler2D uColorTexture;

void main() {
    // Material color
    vec3 albedo = uDiffuseColor;
    if (uHasDiffuseTexture) {
        albedo = texture(uDiffuseTexture, TexCoord).rgb;
    }

    // Normal mapping
    vec3 N = normalize(Normal);
    if (uHasNormalTexture) {
        // Simple TBN-less normal mapping for non-PBR (using dFdx/dFdy trick)
        vec3 dp1 = dFdx(FragPos);
        vec3 dp2 = dFdy(FragPos);
        vec2 duv1 = dFdx(TexCoord);
        vec2 duv2 = dFdy(TexCoord);
        vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
        vec3 B = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        vec3 tangentNormal = texture(uNormalTexture, TexCoord).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    // Lighting setup
    vec3 L;
    float attenuation = 1.0;

    if (uLightType == 0) { // Directional
        L = normalize(-uLightDir);
    } else {
        vec3 lightToFrag = uLightPos - FragPos;
        float distance = length(lightToFrag);
        L = normalize(lightToFrag);
        attenuation = 1.0 / (distance * distance + 0.0001);
        
        if (uLightRadius > 0.0) {
            float distFactor = distance / uLightRadius;
            attenuation *= clamp(1.0 - distFactor * distFactor, 0.0, 1.0);
        }

        if (uLightType == 2) {
            float theta = dot(L, normalize(-uLightDir));
            float epsilon = uInnerCutoff - uOuterCutoff;
            float spotIntensity = clamp((theta - uOuterCutoff) / epsilon, 0.0, 1.0);
            attenuation *= spotIntensity;
        }
    }

    // Ambient
    vec3 ambient = 0.1 * uLightColor * albedo;
    
    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * uLightColor * albedo * attenuation * uLightIntensity * 0.01;
    
    // Specular (Blinn-Phong)
    vec3 V = normalize(uViewPos - FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);
    vec3 specular = spec * uLightColor * uSpecularColor * attenuation * uLightIntensity * 0.01;
    
    // Emissive
    vec3 emissive = vec3(0.0);
    if (uIsEmissive) {
        emissive = albedo * 2.0;
    }

    vec3 result = ambient + diffuse + specular + emissive;
    FragColor = vec4(result, uOpacity);
}
