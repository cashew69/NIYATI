#version 460 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

layout(location = 0) out vec4 FragColor;

uniform vec3  uViewPos;
uniform float uTime;

// GlowMeshNodeData attributes (all have sensible defaults below)
uniform vec3  uGlowColor;      // emissive color
uniform float uIntensity;      // overall brightness scale
uniform float uFresnelPower;   // rim sharpness
uniform float uRimStrength;    // rim peak amplitude
uniform float uCoreStrength;   // ambient body glow
uniform float uPulseSpeed;     // Hz (0 = static)
uniform float uPulseAmt;       // modulation depth [0..1]

void main() {
    vec3 N = length(Normal) > 0.001 ? normalize(Normal) : vec3(0.0, 1.0, 0.0);
    vec3 V = normalize(uViewPos - FragPos);

    float NdotV  = clamp(dot(N, V), 0.0, 1.0);
    float fresnel = pow(1.0 - NdotV, uFresnelPower);

    // Pulse envelope (1.0 when pulseSpeed == 0)
    float pulse = (uPulseSpeed > 0.0)
        ? (1.0 - uPulseAmt) + uPulseAmt * (0.5 + 0.5 * sin(uTime * uPulseSpeed))
        : 1.0;
    // Secondary fast flicker on rim
    float rimPulse = (uPulseSpeed > 0.0)
        ? (1.0 - uPulseAmt * 0.4) + uPulseAmt * 0.4 * sin(uTime * uPulseSpeed * 3.7 + 1.4)
        : 1.0;

    // Core body glow — whole surface
    float coreGlow = uCoreStrength * pulse;
    vec3  coreColor = uGlowColor * coreGlow;

    // Rim / edge glow — peaks at silhouette
    float rimGlow = fresnel * fresnel * uRimStrength * rimPulse;
    vec3  rimColor = mix(uGlowColor, uGlowColor * 1.6, fresnel) * rimGlow;

    vec3  finalColor = (coreColor + rimColor) * uIntensity;
    float alpha      = clamp(coreGlow * 0.45 + rimGlow * 0.75, 0.0, 1.0) * uIntensity;

    FragColor = vec4(finalColor, alpha);
}
