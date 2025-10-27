#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    // Simple diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    vec3 grassColor = vec3(0.2, 0.6, 0.2);
    vec3 result = grassColor * (0.3 + 0.7 * diff);  // Ambient + diffuse
    
    FragColor = vec4(result, 1.0);
}
