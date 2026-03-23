#version 460 core

in vec3 FragPos;
out vec4 FragColor;

uniform sampler3D noiseTex3D;
uniform vec3 uCameraPos;
uniform float uTime;
uniform vec2 u_resolution;

// We replaced the static const with a dynamic macro that rotates based on uTime.
// cos() and sin() on the X and Z axes create a perfect circular orbit.
#define Cloud_SunDir normalize(vec3(cos(uTime * 0.5), 0.2, sin(uTime * 0.5)))

const vec3 Cloud_SunLum = vec3(1.0, 0.95, 0.85) * 10.0;
const vec3 Cloud_AmbLum = vec3(0.3, 0.5, 0.8) * 1.5;

const vec3 BoxMin = vec3(-250.0, 0.0, -250.0);
const vec3 BoxMax = vec3(250.0, 150.0, 250.0);

uniform vec4 uCloudSpheres[256];
uniform int uNumCloudSpheres;
const float DISPLACE_AMP = 1.5;
const float LIGHT_STEPS = 6.0;
const float STEP_SIZE = 0.5;

struct Ray {
    vec3 dir;
    vec3 origin;
};

// --- PROCEDURAL 3D NOISE ---
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
            mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
        mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
            mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// Low-octave FBM for large-scale shape displacement (cheaper)
float fbmLow(vec3 p) {
    float f = 0.0, w = 0.5;
    for (int i = 0; i < 8; i++) {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

// High-octave FBM for cloud detail
float fbm(vec3 p) {
    float f = 0.0, w = 0.5;
    for (int i = 0; i < 15; i++) {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float sampleCloudSDF(vec3 point)
{
    float d = length(point - uCloudSpheres[0].xyz) - uCloudSpheres[0].w;

    for (int i = 1; i < uNumCloudSpheres; i++) {
        float di = length(point - uCloudSpheres[i].xyz) - uCloudSpheres[i].w;
        d = smin(d, di, 2.1);
    }

    return d;
}

float raymarch(Ray r)
{
    float d = 0.0;

    for (int i = 0; i < 200; i++)
    {
        vec3 t = r.origin + r.dir * d;

        float sdf = sampleCloudSDF(t) - DISPLACE_AMP;

        if (sdf < 0.01) {
            return d;
        }

        d += sdf;

        if (d > 500.0) {
            break;
        }
    }
    return d;
}

float calcShading(vec3 p)
{
    float sdf = sampleCloudSDF(p);

    vec3 wind = vec3(2.0, 0.0, 0.5) * uTime;

    float disp = fbmLow((p + wind) * 0.1) * DISPLACE_AMP;
    float dsdf = sdf - disp;

    if (dsdf > 0.0) return 0.0;

    float profile = clamp(-dsdf / 4.0, 0.0, 1.0);

    vec3 np = p * 0.95 + wind * 0.3;
    float n = fbm(np);

    float wispy = n;
    float billowy = 1.0 - abs(n * 2.0 - 1.0);
    float nc = mix(wispy, billowy, smoothstep(0.0, 1.0, profile));

    float density = smoothstep(0.0, 0.25, profile - nc * 0.55);

    return density * 5.0;
}

// --- LIGHTING ---
float PhaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5) * 0.079577;
}

float LightMarch(vec3 p) {
    float d = 0.0;
    vec3 lp = p;
    float stepL = 5.0;
    for (int i = 0; i < LIGHT_STEPS; i++) {
        lp += Cloud_SunDir * stepL;
        d += calcShading(lp) * stepL;
        stepL *= 1.5;
    }
    return d;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    vec2 bg_uv = gl_FragCoord.xy / u_resolution.xy;

    Ray ray;
    ray.dir = normalize(FragPos - uCameraPos);
    ray.origin = uCameraPos;

    float m = raymarch(ray);

    vec3 skyColor = mix(vec3(0.8, 0.5, 0.4), vec3(0.1, 0.3, 0.7), clamp(bg_uv.y + 0.5, 0.0, 1.0));
    float sun = clamp(dot(ray.dir, -Cloud_SunDir), 0.0, 1.0);
    skyColor += vec3(1.0, 0.8, 0.4) * pow(sun, 100.0) * 2.0;

    if (m < 500.0) {
        float cosTheta = dot(ray.dir, Cloud_SunDir);
        float phase = PhaseHG(cosTheta, 0.3) * 0.7 + PhaseHG(cosTheta, -0.1) * 0.3;

        float transmittance = 1.0;
        vec3 color = vec3(0.0);
        float t = 0.0;

        for (int i = 0; i < 128; i++) {
            vec3 hitpoint = ray.origin + ray.dir * (m + t);
            float sdf = sampleCloudSDF(hitpoint);

            if (sdf <= DISPLACE_AMP + 1.0) {
                float density = calcShading(hitpoint);
                
                float volumeStep = 0.4; // Slightly smaller steps for volume integration

                if (density > 0.001) {
                    float ext = density * 0.12;
                    float stepT = exp(-ext * volumeStep);
                    float lightDen = LightMarch(hitpoint);

                    vec3 shadow = exp(-lightDen * 0.12 * vec3(0.95, 0.97, 1.0));
                    float depth = clamp(-sdf / 30.0, 0.0, 1.0);
                    vec3 ms = exp(-lightDen * 0.02 * vec3(0.95, 0.97, 1.0)) * 0.35 * depth;
                    vec3 transToSun = shadow + ms;

                    vec3 direct = Cloud_SunLum * transToSun * phase;
                    vec3 ambient = Cloud_AmbLum * (0.5 + 0.5 * (1.0 - depth));

                    vec3 S = (direct + ambient) * density;
                    color += S * transmittance * volumeStep * 0.12;
                    transmittance *= stepT;

                    if (transmittance < 0.01) break;
                }
                t += volumeStep;
            }
            else {
                t += max(sdf - DISPLACE_AMP, 0.4);
            }

            if ((m + t) > 500.0) break;
        }

        vec3 bg = skyColor;
        vec3 finalColor = color * transmittance;
        finalColor = finalColor / (1.0 + finalColor);
        finalColor = pow(finalColor, vec3(1.0 / 2.2));

        FragColor = vec4(finalColor, 1.0);
    }
    else {
        vec3 col = vec3(0.0);
        col = col / (1.0 + col);
        col = pow(col, vec3(1.0 / 2.2));
        FragColor = vec4(col, 1.0);
    }
}
