// ocean.cpp
#include <stdio.h>
#include <stdlib.h>
#include "ocean.h"

extern FILE* gpFile;

#define OCEAN_SIZE      300.0f
#define GRID_DIMENSION  300
#define NUM_VERTICES    (GRID_DIMENSION * GRID_DIMENSION)
#define NUM_INDICES     ((GRID_DIMENSION - 1) * (GRID_DIMENSION - 1) * 6)

// ---- Vertex shader -------------------------------------------------------

static const char* s_vertSrc =
    "#version 420 core\n"
    "in vec4 aPosition;\n"
    "in vec3 aNormal;\n"
    "in vec2 aTexCoord;\n"
    "\n"
    "uniform mat4  u_model_matrix;\n"
    "uniform mat4  u_view_matrix;\n"
    "uniform mat4  u_projection_matrix;\n"
    "uniform mat4  u_shadow_matrix;\n"
    "uniform float u_waveTime;\n"
    "uniform float u_waveHeight;\n"
    "uniform float u_waveSpeed;\n"
    "uniform float u_waveRadius;\n"
    "uniform float u_wavePointiness;\n"
    "\n"
    "out vec3  outWorldPos;\n"
    "out vec3  outNormal;\n"
    "out vec2  outTexCoord;\n"
    "out float outWavePeak;\n"
    "out vec4  outShadowCoord;\n"
    "\n"
    "#define PI 3.14159265359\n"
    "\n"
    "struct Wave { vec2 dir; float wavelength; float amplitude; float speed; float steepness; };\n"
    "\n"
    "Wave waves[8] = Wave[8](\n"
    "  Wave(vec2( 1.00,  0.10), 180.0, 3.2,  1.8,  0.70),\n"
    "  Wave(vec2( 0.70,  1.00), 120.0, 2.5,  2.2,  0.75),\n"
    "  Wave(vec2(-0.50,  0.40),  65.0, 1.4,  3.0,  0.65),\n"
    "  Wave(vec2( 0.30, -0.90),  38.0, 1.0,  4.0,  0.60),\n"
    "  Wave(vec2( 0.90,  0.50),  18.0, 0.45, 5.5,  0.50),\n"
    "  Wave(vec2(-0.30,  1.00),  10.0, 0.25, 7.0,  0.35),\n"
    "  Wave(vec2( 0.20,  0.80),   6.0, 0.12, 9.0,  0.25),\n"
    "  Wave(vec2(-0.90,  0.10),   3.5, 0.05, 12.0, 0.15)\n"
    ");\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    vec3  pos  = aPosition.xyz;\n"
    "    float Nx   = 0.0, Ny = 1.0, Nz = 0.0, peak = 0.0;\n"
    "\n"
    "    vec2 worldXZ = (u_model_matrix * vec4(aPosition.xyz, 1.0)).xz;\n"
    "\n"
    "    for (int i = 0; i < 8; i++)\n"
    "    {\n"
    "        float k   = 2.0 * PI / (waves[i].wavelength * u_waveRadius);\n"
    "        float c   = sqrt(9.8 / k) * waves[i].speed;\n"
    "        vec2  d   = normalize(waves[i].dir);\n"
    "        float phi = k * dot(d, worldXZ) - (c * u_waveSpeed) * u_waveTime;\n"
    "        float Q   = waves[i].steepness * u_wavePointiness;\n"
    "        float A   = waves[i].amplitude;\n"
    "        pos.x += Q * A * d.x * cos(phi);\n"
    "        pos.z += Q * A * d.y * cos(phi);\n"
    "        pos.y += (A * u_waveHeight) * sign(sin(phi)) * pow(abs(sin(phi)), u_wavePointiness);\n"
    "        peak  += A * sin(phi);\n"
    "        float kA = k * A;\n"
    "        Nx -= kA * d.x * cos(phi);\n"
    "        Ny -= Q * kA  * sin(phi);\n"
    "        Nz -= kA * d.y * cos(phi);\n"
    "    }\n"
    "\n"
    "    outWavePeak   = peak;\n"
    "    outTexCoord   = aTexCoord;\n"
    "    outNormal     = normalize(mat3(u_model_matrix) * normalize(vec3(Nx, Ny, Nz)));\n"
    "    vec4 worldPos = u_model_matrix * vec4(pos, 1.0);\n"
    "    outWorldPos   = worldPos.xyz;\n"
    "    outShadowCoord = u_shadow_matrix * vec4(outWorldPos, 1.0);\n"
    "    gl_Position   = u_projection_matrix * u_view_matrix * worldPos;\n"
    "}\n";

// ---- Fragment shader -----------------------------------------------------

static const char* s_fragSrc =
    "#version 420 core\n"
    "in vec3  outWorldPos;\n"
    "in vec3  outNormal;\n"
    "in vec2  outTexCoord;\n"
    "in float outWavePeak;\n"
    "in vec4  outShadowCoord;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform sampler2D u_sampler;      // normal/ripple map  (unit 0)\n"
    "uniform float     u_waveTime;\n"
    "\n"
    "uniform vec3  u_deepColor;\n"
    "uniform vec3  u_shallowColor;\n"
    "uniform float u_roughness;\n"
    "uniform float u_fresnelF0;\n"
    "uniform float u_foamStrength;\n"
    "uniform vec3  u_foamColor;\n"
    "\n"
    "uniform vec3  u_view_position;\n"
    "\n"
    "uniform samplerCube u_irradianceMap; // (unit 1)\n"
    "uniform samplerCube u_prefilterMap;  // (unit 2)\n"
    "uniform bool        u_hasIBL;\n"
    "uniform float       u_iblIntensity;\n"
    "\n"
    "uniform vec3  u_lightPos;\n"
    "uniform vec3  u_lightColor;\n"
    "uniform float u_lightIntensity;\n"
    "uniform int   u_lightType;\n"
    "uniform vec3  u_lightDir;\n"
    "uniform float u_lightRadius;\n"
    "\n"
    "uniform sampler2DShadow u_shadowMap; // (unit 9)\n"
    "uniform bool  u_shadowEnabled;\n"
    "uniform float u_shadowBias;\n"
    "\n"
    "uniform bool      u_aerialPerspective;\n"
    "uniform sampler2D u_aerialTransmittanceLUT; // (unit 3)\n"
    "uniform sampler2D u_aerialSkyViewLUT;        // (unit 4)\n"
    "uniform float     u_atmBotR;\n"
    "uniform float     u_atmTopR;\n"
    "uniform float     u_atmCamHeight;\n"
    "uniform float     u_atmWorldScale;\n"
    "uniform float     u_atmExposure;\n"
    "\n"
    "uniform bool  u_fogEnabled;\n"
    "uniform vec3  u_fogColor;\n"
    "uniform float u_fogDensity;\n"
    "uniform float u_fogStart;\n"
    "uniform float u_fogEnd;\n"
    "uniform int   u_fogType;\n"
    "\n"
    "#define PI 3.14159265359\n"
    "\n"
    "vec2 hash2(vec2 p) {\n"
    "    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));\n"
    "    return fract(sin(p) * 43758.5453);\n"
    "}\n"
    "\n"
    "vec3 sampleNoTile(sampler2D samp, vec2 uv) {\n"
    "    vec2 i = floor(uv); vec2 f = fract(uv);\n"
    "    vec2 w = f * f * (3.0 - 2.0 * f);\n"
    "    vec3 va = texture(samp, uv + hash2(i + vec2(0.0, 0.0))).rgb;\n"
    "    vec3 vb = texture(samp, uv + hash2(i + vec2(1.0, 0.0))).rgb;\n"
    "    vec3 vc = texture(samp, uv + hash2(i + vec2(0.0, 1.0))).rgb;\n"
    "    vec3 vd = texture(samp, uv + hash2(i + vec2(1.0, 1.0))).rgb;\n"
    "    return mix(mix(va, vb, w.x), mix(vc, vd, w.x), w.y);\n"
    "}\n"
    "\n"
    "float DistGGX(vec3 N, vec3 H, float rough) {\n"
    "    float a = rough * rough; float a2 = a * a;\n"
    "    float d = max(dot(N, H), 0.0);\n"
    "    float dn = d * d * (a2 - 1.0) + 1.0;\n"
    "    return a2 / (PI * dn * dn + 1e-5);\n"
    "}\n"
    "\n"
    "float schlick(float cosT, float F0) {\n"
    "    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0);\n"
    "}\n"
    "\n"
    "vec3 atm_sampleTransmittance(float viewH, float cosAngle) {\n"
    "    float H   = sqrt(max(0.0, u_atmTopR*u_atmTopR - u_atmBotR*u_atmBotR));\n"
    "    float rho = sqrt(max(0.0, viewH*viewH - u_atmBotR*u_atmBotR));\n"
    "    float disc = viewH*viewH*(cosAngle*cosAngle-1.0) + u_atmTopR*u_atmTopR;\n"
    "    if (disc < 0.0) return vec3(0.0);\n"
    "    float d = -viewH*cosAngle + sqrt(disc);\n"
    "    float dMin = u_atmTopR - viewH;\n"
    "    float dMax = rho + H;\n"
    "    float u = (dMax > dMin + 1e-6) ? (d - dMin) / (dMax - dMin) : 0.0;\n"
    "    float v = (H > 1e-6) ? rho / H : 0.0;\n"
    "    return texture(u_aerialTransmittanceLUT, vec2(clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0))).rgb;\n"
    "}\n"
    "\n"
    "vec2 atm_dirToSkyUV(vec3 dir) {\n"
    "    float lat = asin(clamp(dir.y, -1.0, 1.0));\n"
    "    float lon = atan(dir.x, dir.z);\n"
    "    if (lon < 0.0) lon += 2.0 * PI;\n"
    "    float u = lon / (2.0 * PI);\n"
    "    float halfPi = PI * 0.5;\n"
    "    float v = (lat < 0.0)\n"
    "        ? 0.5 * (1.0 - sqrt(clamp(-lat / halfPi, 0.0, 1.0)))\n"
    "        : 0.5 + 0.5 * sqrt(clamp(lat / halfPi, 0.0, 1.0));\n"
    "    return vec2(u, v);\n"
    "}\n"
    "\n"
    "float calcFog(float dist) {\n"
    "    float f = 0.0;\n"
    "    if      (u_fogType == 0) f = (u_fogEnd - dist) / (u_fogEnd - u_fogStart);\n"
    "    else if (u_fogType == 1) f = exp(-u_fogDensity * dist);\n"
    "    else                     f = exp(-pow(u_fogDensity * dist, 2.0));\n"
    "    return 1.0 - clamp(f, 0.0, 1.0);\n"
    "}\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    vec3 N = normalize(outNormal);\n"
    "\n"
    "    // Normal perturbation — 3-layer no-tile\n"
    "    vec2 uv1 = outTexCoord * 7.0  + vec2( 0.030,  0.020) * u_waveTime;\n"
    "    vec2 uv2 = outTexCoord * 13.0 + vec2(-0.022,  0.037) * u_waveTime;\n"
    "    vec3 d1  = sampleNoTile(u_sampler, uv1) * 2.0 - 1.0;\n"
    "    vec3 d2  = sampleNoTile(u_sampler, uv2) * 2.0 - 1.0;\n"
    "    vec2 uv3 = outTexCoord * 40.0 + vec2(0.06, -0.04) * u_waveTime;\n"
    "    vec3 d3  = sampleNoTile(u_sampler, uv3) * 2.0 - 1.0;\n"
    "    N = normalize(\n"
    "        N +\n"
    "        vec3((d1.r + d2.r) * 0.04, 0.0, (d1.g + d2.g) * 0.04) +\n"
    "        vec3(d3.r * 0.005, 0.0, d3.g * 0.005)\n"
    "    );\n"
    "\n"
    "    vec3  V     = normalize(u_view_position - outWorldPos);\n"
    "    float NdotV = max(dot(N, V), 0.001);\n"
    "\n"
    "    // Light direction (engine-style)\n"
    "    vec3  L;\n"
    "    float attenuation = 1.0;\n"
    "    if (u_lightType == 0) {\n"
    "        L = normalize(-u_lightDir);\n"
    "    } else {\n"
    "        vec3  toLight = u_lightPos - outWorldPos;\n"
    "        float dist    = length(toLight);\n"
    "        L = normalize(toLight);\n"
    "        attenuation = 1.0 / (dist * dist + 0.0001);\n"
    "        if (u_lightRadius > 0.0)\n"
    "            attenuation *= clamp(1.0 - (dist/u_lightRadius)*(dist/u_lightRadius), 0.0, 1.0);\n"
    "    }\n"
    "    vec3  H     = normalize(V + L);\n"
    "    float NdotL = max(dot(N, L), 0.0);\n"
    "\n"
    "    // Fresnel\n"
    "    float F  = schlick(NdotV, u_fresnelF0);\n"
    "    float FH = schlick(max(dot(H, V), 0.0), u_fresnelF0);\n"
    "\n"
    "    // Water color (deep/shallow blend by view angle)\n"
    "    vec3 waterColor = mix(u_deepColor, u_shallowColor, clamp(NdotV, 0.0, 1.0));\n"
    "\n"
    "    // SSS approximation — backlit crest translucency\n"
    "    float sss = pow(clamp(dot(L, -N) * 0.5 + 0.5, 0.0, 1.0), 2.0) * NdotV;\n"
    "    waterColor += u_shallowColor * sss * 0.45;\n"
    "\n"
    "    // IBL equivalents for ambient (La) and sky reflection (Ld)\n"
    "    vec3 R  = reflect(-V, N);\n"
    "    vec3 La = u_hasIBL\n"
    "        ? texture(u_irradianceMap, N).rgb * u_iblIntensity\n"
    "        : vec3(0.08, 0.10, 0.12);\n"
    "    vec3 Ld = u_hasIBL\n"
    "        ? textureLod(u_prefilterMap, R, u_roughness * 4.0).rgb * u_iblIntensity\n"
    "        : vec3(0.30, 0.35, 0.45);\n"
    "    vec3 Ls = u_lightColor;  // light tint for specular\n"
    "\n"
    "    // Shadow\n"
    "    float shadow = 1.0;\n"
    "    if (u_shadowEnabled) {\n"
    "        vec4 sc = outShadowCoord;\n"
    "        if (sc.w > 0.0 && sc.z <= sc.w) {\n"
    "            sc.z -= u_shadowBias * sc.w;\n"
    "            shadow = textureProj(u_shadowMap, sc);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Base color: Fresnel blend water toward sky reflection\n"
    "    vec3 color = mix(waterColor, Ld, F);\n"
    "    // Scale by ambient + shadowed direct diffuse\n"
    "    color *= (La + vec3(0.65 * shadow * NdotL));\n"
    "\n"
    "    // GGX specular (D term, shadowed)\n"
    "    float D    = DistGGX(N, H, u_roughness);\n"
    "    float spec = D * FH * NdotL;\n"
    "    color += Ls * spec * 2.2 * shadow;\n"
    "\n"
    "    // Foam at wave crests\n"
    "    float foam = smoothstep(0.45, 0.75, outWavePeak);\n"
    "    color = mix(color, u_foamColor, foam * u_foamStrength);\n"
    "\n"
    "    // Horizon haze — blend toward sky at glancing angles\n"
    "    float hz = pow(clamp(NdotV, 0.0, 1.0), 0.35);\n"
    "    color = mix(Ld * 0.75, color, hz);\n"
    "\n"
    "    // Reinhard tonemapping + gamma 2.2 (matches engine PBR)\n"
    "    color = color / (color + vec3(1.0));\n"
    "    color = pow(max(color, 0.0001), vec3(1.0 / 2.2));\n"
    "\n"
    "    // Aerial perspective (post-tonemap, identical to engine PBR)\n"
    "    if (u_aerialPerspective) {\n"
    "        vec3  vd     = normalize(outWorldPos - u_view_position);\n"
    "        float distKm = length(outWorldPos - u_view_position) * u_atmWorldScale;\n"
    "        float fade   = smoothstep(0.5, 20.0, distKm);\n"
    "        if (fade > 0.001) {\n"
    "            float fragH  = max(u_atmBotR + 0.001, u_atmBotR + outWorldPos.y * u_atmWorldScale);\n"
    "            float cosV   = abs(vd.y);\n"
    "            vec3  T_cam  = atm_sampleTransmittance(u_atmCamHeight, cosV);\n"
    "            vec3  T_frag = atm_sampleTransmittance(fragH, cosV);\n"
    "            vec3  T_seg  = clamp(T_cam / max(T_frag, vec3(0.001)), 0.0, 1.0);\n"
    "            vec3  skyL   = texture(u_aerialSkyViewLUT, atm_dirToSkyUV(vd)).rgb;\n"
    "            vec3  skyHDR = skyL * u_atmExposure;\n"
    "            vec3  skyT   = pow(max(skyHDR / (skyHDR + vec3(1.0)), vec3(0.0)), vec3(1.0/2.2));\n"
    "            color = mix(color, color * T_seg + skyT * (1.0 - T_seg), fade);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Fog (post-tonemap, identical to engine PBR)\n"
    "    if (u_fogEnabled) {\n"
    "        float dist = length(u_view_position - outWorldPos);\n"
    "        float ff   = calcFog(dist);\n"
    "        color = mix(color, pow(max(u_fogColor, vec3(0.0)), vec3(1.0/2.2)), ff);\n"
    "    }\n"
    "\n"
    "    float alpha = mix(0.88, 0.99, F);\n"
    "    FragColor = vec4(color, alpha);\n"
    "}\n";

// =========================================================================

Ocean::Ocean()
{
    vao = 0;
    vbo_position = 0;
    vbo_normal = 0;
    vbo_texcoord = 0;
    vbo_index = 0;
    texture = 0;
    shaderProgramObject = 0;
}

Ocean::~Ocean()
{
    uninitialize();
}

bool Ocean::initialize(GLuint textureId)
{
    texture = textureId;
    if (!compileShaders()) return false;
    createMesh();
    return true;
}

static GLuint compileShader(GLenum type, const char* src, const char* label)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char* log = (char*)malloc(len);
            if (log) {
                glGetShaderInfoLog(shader, len, NULL, log);
                if (gpFile) fprintf(gpFile, "Ocean %s Shader Error: %s\n", label, log);
                free(log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Ocean::compileShaders()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   s_vertSrc, "Vertex");
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, s_fragSrc, "Fragment");
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return false; }

    shaderProgramObject = glCreateProgram();
    glAttachShader(shaderProgramObject, vs);
    glAttachShader(shaderProgramObject, fs);
    glBindAttribLocation(shaderProgramObject, AMC_ATTRIBUTE_POSITION, "aPosition");
    glBindAttribLocation(shaderProgramObject, AMC_ATTRIBUTE_NORMAL,   "aNormal");
    glBindAttribLocation(shaderProgramObject, AMC_ATTRIBUTE_TEXCOORD, "aTexCoord");
    glLinkProgram(shaderProgramObject);

    GLint status = 0;
    glGetProgramiv(shaderProgramObject, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint len = 0;
        glGetProgramiv(shaderProgramObject, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            char* log = (char*)malloc(len);
            if (log) {
                glGetProgramInfoLog(shaderProgramObject, len, NULL, log);
                if (gpFile) fprintf(gpFile, "Ocean Program Link Error: %s\n", log);
                free(log);
            }
        }
        glDeleteShader(vs); glDeleteShader(fs);
        glDeleteProgram(shaderProgramObject); shaderProgramObject = 0;
        return false;
    }
    glDetachShader(shaderProgramObject, vs); glDeleteShader(vs);
    glDetachShader(shaderProgramObject, fs); glDeleteShader(fs);

    // Cache uniform locations
    modelMatrixUniform      = glGetUniformLocation(shaderProgramObject, "u_model_matrix");
    viewMatrixUniform       = glGetUniformLocation(shaderProgramObject, "u_view_matrix");
    projectionMatrixUniform = glGetUniformLocation(shaderProgramObject, "u_projection_matrix");
    shadowMatrixUniform     = glGetUniformLocation(shaderProgramObject, "u_shadow_matrix");
    waveTimeUniform         = glGetUniformLocation(shaderProgramObject, "u_waveTime");
    waveHeightUniform       = glGetUniformLocation(shaderProgramObject, "u_waveHeight");
    waveSpeedUniform        = glGetUniformLocation(shaderProgramObject, "u_waveSpeed");
    waveRadiusUniform       = glGetUniformLocation(shaderProgramObject, "u_waveRadius");
    wavePointinessUniform   = glGetUniformLocation(shaderProgramObject, "u_wavePointiness");
    deepColorUniform        = glGetUniformLocation(shaderProgramObject, "u_deepColor");
    shallowColorUniform     = glGetUniformLocation(shaderProgramObject, "u_shallowColor");
    roughnessUniform        = glGetUniformLocation(shaderProgramObject, "u_roughness");
    fresnelF0Uniform        = glGetUniformLocation(shaderProgramObject, "u_fresnelF0");
    foamStrengthUniform     = glGetUniformLocation(shaderProgramObject, "u_foamStrength");
    foamColorUniform        = glGetUniformLocation(shaderProgramObject, "u_foamColor");
    viewPositionUniform     = glGetUniformLocation(shaderProgramObject, "u_view_position");
    samplerUniform          = glGetUniformLocation(shaderProgramObject, "u_sampler");
    irradianceMapUniform    = glGetUniformLocation(shaderProgramObject, "u_irradianceMap");
    prefilterMapUniform     = glGetUniformLocation(shaderProgramObject, "u_prefilterMap");
    hasIBLUniform           = glGetUniformLocation(shaderProgramObject, "u_hasIBL");
    iblIntensityUniform     = glGetUniformLocation(shaderProgramObject, "u_iblIntensity");
    lightPosUniform         = glGetUniformLocation(shaderProgramObject, "u_lightPos");
    lightColorUniform       = glGetUniformLocation(shaderProgramObject, "u_lightColor");
    lightIntensityUniform   = glGetUniformLocation(shaderProgramObject, "u_lightIntensity");
    lightTypeUniform        = glGetUniformLocation(shaderProgramObject, "u_lightType");
    lightDirUniform         = glGetUniformLocation(shaderProgramObject, "u_lightDir");
    lightRadiusUniform      = glGetUniformLocation(shaderProgramObject, "u_lightRadius");
    shadowMapUniform        = glGetUniformLocation(shaderProgramObject, "u_shadowMap");
    shadowEnabledUniform    = glGetUniformLocation(shaderProgramObject, "u_shadowEnabled");
    shadowBiasUniform       = glGetUniformLocation(shaderProgramObject, "u_shadowBias");
    aerialPerspectiveUniform= glGetUniformLocation(shaderProgramObject, "u_aerialPerspective");
    aerialTransLUTUniform   = glGetUniformLocation(shaderProgramObject, "u_aerialTransmittanceLUT");
    aerialSkyViewLUTUniform = glGetUniformLocation(shaderProgramObject, "u_aerialSkyViewLUT");
    atmBotRUniform          = glGetUniformLocation(shaderProgramObject, "u_atmBotR");
    atmTopRUniform          = glGetUniformLocation(shaderProgramObject, "u_atmTopR");
    atmCamHeightUniform     = glGetUniformLocation(shaderProgramObject, "u_atmCamHeight");
    atmWorldScaleUniform    = glGetUniformLocation(shaderProgramObject, "u_atmWorldScale");
    atmExposureUniform      = glGetUniformLocation(shaderProgramObject, "u_atmExposure");
    fogEnabledUniform       = glGetUniformLocation(shaderProgramObject, "u_fogEnabled");
    fogColorUniform         = glGetUniformLocation(shaderProgramObject, "u_fogColor");
    fogDensityUniform       = glGetUniformLocation(shaderProgramObject, "u_fogDensity");
    fogStartUniform         = glGetUniformLocation(shaderProgramObject, "u_fogStart");
    fogEndUniform           = glGetUniformLocation(shaderProgramObject, "u_fogEnd");
    fogTypeUniform          = glGetUniformLocation(shaderProgramObject, "u_fogType");

    return true;
}

void Ocean::createMesh()
{
    GLfloat* vertices  = (GLfloat*)malloc(NUM_VERTICES * 3 * sizeof(GLfloat));
    GLfloat* normals   = (GLfloat*)malloc(NUM_VERTICES * 3 * sizeof(GLfloat));
    GLfloat* texcoords = (GLfloat*)malloc(NUM_VERTICES * 2 * sizeof(GLfloat));
    GLuint*  indices   = (GLuint*) malloc(NUM_INDICES  *     sizeof(GLuint));

    if (!vertices || !normals || !texcoords || !indices) {
        if (gpFile) fprintf(gpFile, "Ocean: mesh allocation failed\n");
        free(vertices); free(normals); free(texcoords); free(indices);
        return;
    }

    float halfSize   = OCEAN_SIZE / 2.0f;
    float gridSpace  = OCEAN_SIZE / (float)(GRID_DIMENSION - 1);

    for (int i = 0, vi = 0; i < GRID_DIMENSION; i++) {
        for (int j = 0; j < GRID_DIMENSION; j++, vi++) {
            vertices[vi*3+0]  = j * gridSpace - halfSize;
            vertices[vi*3+1]  = 0.0f;
            vertices[vi*3+2]  = i * gridSpace - halfSize;
            normals[vi*3+0]   = 0.0f;
            normals[vi*3+1]   = 1.0f;
            normals[vi*3+2]   = 0.0f;
            texcoords[vi*2+0] = (float)j / (GRID_DIMENSION - 1);
            texcoords[vi*2+1] = (float)i / (GRID_DIMENSION - 1);
        }
    }

    for (int i = 0, ii = 0; i < GRID_DIMENSION - 1; i++) {
        for (int j = 0; j < GRID_DIMENSION - 1; j++) {
            GLuint v0 = i * GRID_DIMENSION + j;
            GLuint v1 = v0 + 1;
            GLuint v2 = (i + 1) * GRID_DIMENSION + j;
            GLuint v3 = v2 + 1;
            indices[ii++] = v0; indices[ii++] = v2; indices[ii++] = v1;
            indices[ii++] = v1; indices[ii++] = v2; indices[ii++] = v3;
        }
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo_position);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_position);
    glBufferData(GL_ARRAY_BUFFER, NUM_VERTICES * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(AMC_ATTRIBUTE_POSITION, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(AMC_ATTRIBUTE_POSITION);

    glGenBuffers(1, &vbo_normal);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_normal);
    glBufferData(GL_ARRAY_BUFFER, NUM_VERTICES * 3 * sizeof(GLfloat), normals, GL_STATIC_DRAW);
    glVertexAttribPointer(AMC_ATTRIBUTE_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(AMC_ATTRIBUTE_NORMAL);

    glGenBuffers(1, &vbo_texcoord);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_texcoord);
    glBufferData(GL_ARRAY_BUFFER, NUM_VERTICES * 2 * sizeof(GLfloat), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(AMC_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(AMC_ATTRIBUTE_TEXCOORD);

    glGenBuffers(1, &vbo_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, NUM_INDICES * sizeof(GLuint), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    if (gpFile) fprintf(gpFile, "Ocean mesh: %d verts, %d indices\n", NUM_VERTICES, NUM_INDICES);

    free(vertices); free(normals); free(texcoords); free(indices);
}

void Ocean::render(
    float waveTime,
    mat4 modelMatrix, mat4 viewMatrix, mat4 projectionMatrix,
    vec3 cameraPosition,
    float waveHeight, float waveSpeed, float waveRadius, float wavePointiness,
    float* deepColor, float* shallowColor,
    float roughness, float fresnelF0, float foamStrength, float* foamColor,
    const OceanEngineBindings& eng)
{
    glUseProgram(shaderProgramObject);

    // Matrices
    glUniformMatrix4fv(modelMatrixUniform,      1, GL_FALSE, modelMatrix);
    glUniformMatrix4fv(viewMatrixUniform,       1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(projectionMatrixUniform, 1, GL_FALSE, projectionMatrix);

    // Shadow matrix — pass identity if no shadow so shader has valid data
    if (eng.shadowEnabled)
        glUniformMatrix4fv(shadowMatrixUniform, 1, GL_FALSE, eng.shadowMatrix);
    else
        glUniformMatrix4fv(shadowMatrixUniform, 1, GL_FALSE, mat4::identity());

    // Wave params
    glUniform1f(waveTimeUniform,       waveTime);
    glUniform1f(waveHeightUniform,     waveHeight);
    glUniform1f(waveSpeedUniform,      waveSpeed);
    glUniform1f(waveRadiusUniform,     waveRadius);
    glUniform1f(wavePointinessUniform, wavePointiness);

    // Water appearance
    glUniform3fv(deepColorUniform,    1, deepColor);
    glUniform3fv(shallowColorUniform, 1, shallowColor);
    glUniform1f(roughnessUniform,     roughness);
    glUniform1f(fresnelF0Uniform,     fresnelF0);
    glUniform1f(foamStrengthUniform,  foamStrength);
    glUniform3fv(foamColorUniform,    1, foamColor);

    // View
    glUniform3fv(viewPositionUniform, 1, cameraPosition);

    // Normal map — unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(samplerUniform, 0);

    // IBL — units 1, 2
    glUniform1i(hasIBLUniform,      eng.hasIBL ? 1 : 0);
    glUniform1f(iblIntensityUniform, eng.iblIntensity);
    glUniform1i(irradianceMapUniform, 1);
    glUniform1i(prefilterMapUniform,  2);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, eng.irradianceMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, eng.prefilterMap);

    // Light
    glUniform3fv(lightPosUniform,       1, eng.lightPos);
    glUniform3fv(lightColorUniform,     1, eng.lightColor);
    glUniform1f (lightIntensityUniform,    eng.lightIntensity);
    glUniform1i (lightTypeUniform,         eng.lightType);
    glUniform3fv(lightDirUniform,       1, eng.lightDir);
    glUniform1f (lightRadiusUniform,       eng.lightRadius);

    // Shadow — unit 9
    glUniform1i(shadowEnabledUniform, eng.shadowEnabled ? 1 : 0);
    glUniform1f(shadowBiasUniform,    eng.shadowBias);
    glUniform1i(shadowMapUniform,     9);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, eng.shadowDepthTex);

    // Aerial perspective — units 3, 4
    glUniform1i(aerialPerspectiveUniform, eng.aerialActive ? 1 : 0);
    glUniform1i(aerialTransLUTUniform,   3);
    glUniform1i(aerialSkyViewLUTUniform, 4);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, eng.aerialTransLUT);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, eng.aerialSkyViewLUT);
    glUniform1f(atmBotRUniform,      eng.atmBotR);
    glUniform1f(atmTopRUniform,      eng.atmTopR);
    glUniform1f(atmCamHeightUniform, eng.atmCamHeight);
    glUniform1f(atmWorldScaleUniform,eng.atmWorldScale);
    glUniform1f(atmExposureUniform,  eng.atmExposure);

    // Fog
    glUniform1i(fogEnabledUniform,  eng.fogEnabled ? 1 : 0);
    glUniform3fv(fogColorUniform,   1, eng.fogColor);
    glUniform1f(fogDensityUniform,  eng.fogDensity);
    glUniform1f(fogStartUniform,    eng.fogStart);
    glUniform1f(fogEndUniform,      eng.fogEnd);
    glUniform1i(fogTypeUniform,     eng.fogType);

    // Draw
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);

    // Restore active texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void Ocean::uninitialize()
{
    if (vbo_index)    { glDeleteBuffers(1, &vbo_index);    vbo_index    = 0; }
    if (vbo_texcoord) { glDeleteBuffers(1, &vbo_texcoord); vbo_texcoord = 0; }
    if (vbo_normal)   { glDeleteBuffers(1, &vbo_normal);   vbo_normal   = 0; }
    if (vbo_position) { glDeleteBuffers(1, &vbo_position); vbo_position = 0; }
    if (vao)          { glDeleteVertexArrays(1, &vao);     vao          = 0; }

    if (shaderProgramObject)
    {
        glUseProgram(shaderProgramObject);
        GLint n = 0;
        glGetProgramiv(shaderProgramObject, GL_ATTACHED_SHADERS, &n);
        if (n > 0) {
            GLuint* sh = (GLuint*)malloc(n * sizeof(GLuint));
            if (sh) {
                glGetAttachedShaders(shaderProgramObject, n, NULL, sh);
                for (int i = 0; i < n; i++) {
                    glDetachShader(shaderProgramObject, sh[i]);
                    glDeleteShader(sh[i]);
                }
                free(sh);
            }
        }
        glUseProgram(0);
        glDeleteProgram(shaderProgramObject);
        shaderProgramObject = 0;
    }
}
