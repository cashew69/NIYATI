#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;
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

    // Debug: visualize height
    //float h = (FragPos.y + 32.0) / 64.0; // Normalize height to 0-1
    //FragColor = vec4(h, h, h, 1.0);
    //return;
    
    // ... rest of your lighting code
    // Ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * uLightColor;
    
    // Get material color (texture or uniform)
    vec3 materialDiffuse = uDiffuseColor;
    if (uHasDiffuseTexture) {
        materialDiffuse = texture(uDiffuseTexture, TexCoord).rgb;
    }
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor * materialDiffuse;
    
    // Specular
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uLightColor * uSpecularColor;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);


}
