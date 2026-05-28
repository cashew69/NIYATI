#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 RayDir;

// Camera & matrices
uniform vec3  u_cameraPos;
uniform mat4  u_view;
uniform mat4  u_projection;

// N-shape SDF scene (up to 8 shapes)
#define MAX_SDF_SHAPES 8
uniform vec3  u_sdfPos[MAX_SDF_SHAPES];
uniform float u_sdfRadius[MAX_SDF_SHAPES];
uniform vec3  u_sdfColor[MAX_SDF_SHAPES];
uniform int   u_sdfCount;

// Blend / operation (global, read from first node)
uniform int   u_operation;  // 0 = Union, 1 = Smooth Union
uniform float u_smoothK;

// Raymarching config (global, read from first node)
uniform int   u_maxSteps;
uniform float u_surfDist;
uniform float u_maxDist;

// Material (read from first node)
uniform float      u_opacity;
uniform int        u_hasTexture;
uniform sampler2D  u_sdf1Texture;

// Lighting — standard engine uniform names so cacheShaderLocations fills them
uniform vec3  uLightPos;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform int   uLightType;     // 0 = Directional, 1 = Point, 2 = Spot
uniform vec3  uLightDir;

const float PI      = 3.14159265359;
const float AMBIENT = 0.12;

// ---- SDF primitives ---------------------------------------------------------

float sdSphere(vec3 p, float r) { return length(p) - r; }

// ---- Scene SDF — folds all N shapes using operation -------------------------

// Returns signed distance; outputs blended surface color.
float map(vec3 p, out vec3 outColor) {
    outColor = (u_sdfCount > 0) ? u_sdfColor[0] : vec3(1.0);
    if (u_sdfCount == 0) return u_maxDist;

    float d = sdSphere(p - u_sdfPos[0], u_sdfRadius[0]);

    for (int i = 1; i < u_sdfCount; i++) {
        float di = sdSphere(p - u_sdfPos[i], u_sdfRadius[i]);
        if (u_operation == 1) {
            float k = max(u_smoothK, 0.0001);
            float h = clamp(0.5 + 0.5 * (di - d) / k, 0.0, 1.0);
            outColor = mix(u_sdfColor[i], outColor, h);
            d = mix(di, d, h) - k * h * (1.0 - h);
        } else {
            if (di < d) {
                d  = di;
                outColor = u_sdfColor[i];
            }
        }
    }
    return d;
}

// Surface normal via central differences (dummy color output discarded)
vec3 calcNormal(vec3 p) {
    float e = 0.0005;
    vec3 c;
    return normalize(vec3(
        map(p + vec3(e, 0, 0), c) - map(p - vec3(e, 0, 0), c),
        map(p + vec3(0, e, 0), c) - map(p - vec3(0, e, 0), c),
        map(p + vec3(0, 0, e), c) - map(p - vec3(0, 0, e), c)
    ));
}

// Spherical UV for shape-0 texture
vec2 sphericalUV(vec3 p, vec3 center) {
    vec3 n = normalize(p - center);
    return vec2(0.5 + atan(n.z, n.x) / (2.0 * PI),
                0.5 - asin(clamp(n.y, -1.0, 1.0)) / PI);
}

// ---- Main -------------------------------------------------------------------

void main() {
    vec3 ro = u_cameraPos;
    vec3 rd = normalize(RayDir);

    float dO  = 0.0;
    bool  hit = false;

    int   steps = clamp(u_maxSteps, 1, 512);
    float maxD  = max(u_maxDist,  1.0);
    float surfD = max(u_surfDist, 0.00001);

    for (int i = 0; i < steps; i++) {
        vec3  p  = ro + rd * dO;
        vec3  c;
        float dS = map(p, c);
        dO += dS;
        if (dO > maxD) break;
        if (abs(dS) < surfD) { hit = true; break; }
    }

    if (!hit) discard;

    vec3 p = ro + rd * dO;

    // Write correct depth so SDF composites with scene geometry
    vec4 clip    = u_projection * u_view * vec4(p, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;

    // Surface normal
    vec3 N = calcNormal(p);

    // Base color from blended SDF scene
    vec3 baseColor;
    map(p, baseColor);

    // Texture overrides shape-0's color contribution
    if (u_hasTexture != 0 && u_sdfCount > 0) {
        // Shape-0 proximity weight: how dominant shape 0 is at this point
        float d0 = sdSphere(p - u_sdfPos[0], u_sdfRadius[0]);
        float minOther = u_maxDist;
        for (int i = 1; i < u_sdfCount; i++) {
            float di = sdSphere(p - u_sdfPos[i], u_sdfRadius[i]);
            if (di < minOther) minOther = di;
        }
        float texW;
        if (u_operation == 1) {
            float k = max(u_smoothK, 0.0001);
            texW = clamp(0.5 + 0.5 * (minOther - d0) / k, 0.0, 1.0);
        } else {
            texW = (d0 <= minOther + 0.0001) ? 1.0 : 0.0;
        }
        vec3 texColor = texture(u_sdf1Texture, sphericalUV(p, u_sdfPos[0])).rgb;
        baseColor = mix(baseColor, texColor, texW);
    }

    // Light direction
    vec3 L = (uLightType == 0)
           ? normalize(-uLightDir)
           : normalize(uLightPos - p);

    float diff = max(dot(N, L), 0.0);

    // Blinn-Phong specular
    vec3  V    = normalize(u_cameraPos - p);
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // Scale engine light intensity (default 500 point → ~1.0 diffuse contrib)
    float ls = clamp(uLightIntensity * 0.004, 0.0, 1.5);

    vec3 color = baseColor * (AMBIENT + diff * ls) * uLightColor
               + uLightColor * spec * ls * 0.25;

    FragColor = vec4(color, u_opacity);
}
