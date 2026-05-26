// Unity-build file — included by engine.h after all engine headers.
// Do NOT include engine/core/gl/structs.h here: structs.h includes engine.h,
// which would create a circular dependency that crashes cmake's dep scanner.
// All engine types (vec3, mat4, GLuint, SceneNode, ...) are already defined
// by the time engine.h reaches this file.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
// stb_image_write implementation lives in terrain.cpp (unity build).
extern int stbi_write_png(const char*, int, int, int, const void*, int);

// ---- Engine externs --------------------------------------------------------
extern FILE*       gpFile;
extern char*       readShaderFile(const char* filename);
extern Bool        buildShaderProgramFromFiles(const char**, int, ShaderProgram**, const char**, GLint*, int);
extern mat4        viewMatrix;
extern mat4        perspectiveProjectionMatrix;
extern const char* attribNames[4];
extern GLint       attribIndices[4];
extern float       platformGetTime();

// Forward decl — defined in the public helper section at the bottom but
// referenced from sg_InitVolumetricCloudNode for auto-load on session restore.
bool vcCloud_LoadNVDF(SceneNode* node, const char* path);
bool vcCloud_LoadWeatherMap(SceneNode* node, const char* path);

// ---- Shared (per-process) shader state -------------------------------------
static GLuint        s_ComputeProg    = 0;
static GLuint        s_NoiseGenProg   = 0;
static GLuint        s_WeatherGenProg = 0;
static ShaderProgram* s_QuadProg     = nullptr;
static GLuint        s_EmptyVAO      = 0;
static GLint         s_QuadCloudTexLoc = -1;
static GLint         s_QuadDepthTexLoc = -1;
static GLint         s_QuadDepthLoc    = -1;

static struct {
    GLint baseRes, detailRes, mode;
} s_NoiseGenLoc;

static struct {
    GLint width, height, seed;
    GLint patternType;
    GLint centerX, centerY;
    GLint arms;
    GLint tightness, falloffRadius;
    GLint bandAngle, bandWidth, bandSpacing, bandTurbulence;
    GLint noiseFreq, coverageScale;
    GLint coverageMin, coverageMax;
} s_WeatherGenLoc;

// Noise textures are expensive to generate and identical across all cloud nodes,
// so they are shared globals created once and freed at process exit.
static GLuint        s_NoiseTexBase   = 0;  // 64^3 RGBA8: Perlin-Worley + Worley×3
static GLuint        s_NoiseTexDetail = 0;  // 32^3 RGBA8: Worley high-freq erosion (changed to RGBA8 for GPU bake)
static GLuint        s_BlueNoiseTex   = 0;  // 32×32 R8 blue noise for ray jitter

// ---- GPU-side noise baking --------------------------------------------------

static void vcCloud_BakeBaseNoiseGPU(GLuint tex, int N) {
    if (!s_NoiseGenProg) return;
    glUseProgram(s_NoiseGenProg);
    glUniform1i(s_NoiseGenLoc.baseRes, N);
    glUniform1i(s_NoiseGenLoc.mode, 0);
    glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute((N + 3) / 4, (N + 3) / 4, (N + 3) / 4);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

static void vcCloud_BakeDetailNoiseGPU(GLuint tex, int N) {
    if (!s_NoiseGenProg) return;
    glUseProgram(s_NoiseGenProg);
    glUniform1i(s_NoiseGenLoc.detailRes, N);
    glUniform1i(s_NoiseGenLoc.mode, 1);
    glBindImageTexture(1, tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute((N + 3) / 4, (N + 3) / 4, (N + 3) / 4);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

static void vcCloud_BakeWeatherMapGPU(GLuint tex, int w, int h, unsigned seed,
                                      VolumetricCloudNodeData* c) {
    if (!s_WeatherGenProg) return;
    glUseProgram(s_WeatherGenProg);
    glUniform1i (s_WeatherGenLoc.width,  w);
    glUniform1i (s_WeatherGenLoc.height, h);
    glUniform1ui(s_WeatherGenLoc.seed,   seed);

    // Pattern parameters — use defaults if no node data supplied
    int   patternType   = c ? c->weatherGen.patternType   : 0;
    float centerX       = c ? c->weatherGen.centerX       : 0.5f;
    float centerY       = c ? c->weatherGen.centerY       : 0.5f;
    int   arms          = c ? c->weatherGen.arms          : 2;
    float tightness     = c ? c->weatherGen.tightness     : 5.0f;
    float falloffRadius = c ? c->weatherGen.falloffRadius : 0.4f;
    float bandAngle     = c ? c->weatherGen.bandAngle     : 0.0f;
    float bandWidth     = c ? c->weatherGen.bandWidth     : 0.4f;
    float bandSpacing   = c ? c->weatherGen.bandSpacing   : 0.15f;
    float bandTurbulence= c ? c->weatherGen.bandTurbulence: 0.05f;
    float noiseFreq     = c ? c->weatherGen.noiseFreq     : 1.0f;
    float coverageScale = c ? c->weatherGen.coverageScale : 0.6f;
    float coverageMin   = c ? c->weatherGen.coverageMin   : 0.0f;
    float coverageMax   = c ? c->weatherGen.coverageMax   : 1.0f;

    glUniform1i (s_WeatherGenLoc.patternType,    patternType);
    glUniform1f (s_WeatherGenLoc.centerX,        centerX);
    glUniform1f (s_WeatherGenLoc.centerY,        centerY);
    glUniform1i (s_WeatherGenLoc.arms,           arms);
    glUniform1f (s_WeatherGenLoc.tightness,      tightness);
    glUniform1f (s_WeatherGenLoc.falloffRadius,  falloffRadius);
    glUniform1f (s_WeatherGenLoc.bandAngle,      bandAngle);
    glUniform1f (s_WeatherGenLoc.bandWidth,      bandWidth);
    glUniform1f (s_WeatherGenLoc.bandSpacing,    bandSpacing);
    glUniform1f (s_WeatherGenLoc.bandTurbulence, bandTurbulence);
    glUniform1f (s_WeatherGenLoc.noiseFreq,      noiseFreq);
    glUniform1f (s_WeatherGenLoc.coverageScale,  coverageScale);
    glUniform1f (s_WeatherGenLoc.coverageMin,    coverageMin);
    glUniform1f (s_WeatherGenLoc.coverageMax,    coverageMax);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute((w + 7) / 8, (h + 7) / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

// ============================================================================
// CPU-side noise texture generation  (Nubis / HZD style pre-baked 3D noise)
// ============================================================================

// Integer hash for noise lattice — wraps at `period` to ensure tiling
static float vcn_hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    z = ((z % period) + period) % period;
    unsigned n = (unsigned)(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013);
    n ^= n >> 13;  n *= 0xb5297a4du;
    n ^= n >> 7;   n *= 0x68e31da4u;
    n ^= n >> 11;
    return (float)(n & 0x00ffffffu) / (float)0x01000000u;
}

// Trilinear value noise, tiled at `period` lattice steps.
// Sample coordinates should be in [0, period].
static float vcn_valueNoise(float x, float y, float z, int period) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float u = x-x0, v = y-y0, w = z-z0;
    u = u*u*(3.0f-2.0f*u); v = v*v*(3.0f-2.0f*v); w = w*w*(3.0f-2.0f*w);
    auto h = [&](int a, int b, int c){ return vcn_hash(a,b,c,period,0); };
    auto lp = [](float a, float b, float t){ return a+(b-a)*t; };
    return lp(lp(lp(h(x0,y0,z0),  h(x0+1,y0,z0),  u),
                  lp(h(x0,y0+1,z0),h(x0+1,y0+1,z0),u), v),
               lp(lp(h(x0,y0,z0+1),h(x0+1,y0,z0+1),u),
                  lp(h(x0,y0+1,z0+1),h(x0+1,y0+1,z0+1),u), v), w);
}

// FBM of value noise, tiled at `baseFreq`.
static float vcn_fbm(float x, float y, float z, int baseFreq, int oct) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < oct; i++) {
        int f = baseFreq << i;
        v += a * vcn_valueNoise(x*f, y*f, z*f, f);
        a *= 0.5f;
    }
    return v;
}

// Worley (cellular) noise with tiling at `freq` cells.
// Input x,y,z in [0,1]. Returns inverted distance: 1 = cell centre, 0 = edge.
static float vcn_worley(float x, float y, float z, int freq) {
    float fx = x*freq, fy = y*freq, fz = z*freq;
    int cx = (int)floorf(fx), cy = (int)floorf(fy), cz = (int)floorf(fz);
    float md = 9.0f;
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        int nx = cx+dx, ny = cy+dy, nz = cz+dz;
        // Hash uses tiled coords; feature point is in UNWRAPPED cell space
        float px = (float)nx + vcn_hash(nx,ny,nz,freq,0);
        float py = (float)ny + vcn_hash(nx,ny,nz,freq,1);
        float pz = (float)nz + vcn_hash(nx,ny,nz,freq,2);
        float d  = (fx-px)*(fx-px)+(fy-py)*(fy-py)+(fz-pz)*(fz-pz);
        if (d < md) md = d;
    }
    // Normalize: max possible is sqrt(3) ≈ 1.73, return INVERTED so centre = 1
    return 1.0f - sqrtf(md) / 1.73205f;
}

// Perlin-Worley composite (Schneider 2015):
// Map FBM Perlin into [invWorley, 1.0] so cloud centres are always solid
// and edges blend naturally with the Worley cellular structure.
static float vcn_perlinWorley(float x, float y, float z, int freq) {
    float perlin   = vcn_fbm(x, y, z, freq, 3) * 1.5f;  // ~[0, 1]
    perlin = perlin < 0.0f ? 0.0f : (perlin > 1.0f ? 1.0f : perlin);
    float invW     = vcn_worley(x, y, z, freq);           // [0,1], centre=1
    // remap(perlin, 0, 1, invW, 1) = invW + perlin*(1-invW)
    return invW + perlin * (1.0f - invW);
}

static inline float vcn_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// ---- 3D Noise Persistent Storage ------------------------------------------

typedef struct {
    char     magic[4];   // "N3DA" (RGBA) or "N3D " (RGB)
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t channels;   // 3 or 4
} N3DHeader;

static bool vcCloud_SaveNoiseToFile(const char* path, int N, int channels, const unsigned char* data) {
    if (!path || !path[0]) return false;
    FILE* f = fopen(path, "wb");
    if (!f) { LOG_E("vcCloud: Failed to open %s for writing", path); return false; }

    N3DHeader hdr;
    memcpy(hdr.magic, channels == 4 ? "N3DA" : "N3D ", 4);
    hdr.version = 1;
    hdr.width = N;
    hdr.height = N;
    hdr.depth = N;
    hdr.channels = channels;

    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(data, (size_t)N * N * N * channels, 1, f);
    fclose(f);
    LOG_I("vcCloud: Saved %d^3 noise (%d ch) to %s", N, channels, path);
    return true;
}

static unsigned char* vcCloud_LoadNoiseFromFile(const char* path, int* N, int* channels) {
    if (!path || !path[0]) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr; // Silent fail is fine, means we need to bake

    N3DHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        LOG_E("vcCloud: Failed to read header from %s", path);
        fclose(f); return nullptr;
    }

    if (strncmp(hdr.magic, "N3D", 3) != 0) {
        LOG_E("vcCloud: Invalid magic in %s", path);
        fclose(f); return nullptr;
    }

    *N = hdr.width;
    *channels = hdr.channels;
    size_t size = (size_t)(*N) * (*N) * (*N) * (*channels);
    unsigned char* data = (unsigned char*)malloc(size);
    if (!data) {
        LOG_E("vcCloud: OOM loading noise from %s", path);
        fclose(f); return nullptr;
    }

    if (fread(data, size, 1, f) != 1) {
        LOG_E("vcCloud: Failed to read voxel data from %s", path);
        free(data); fclose(f); return nullptr;
    }

    fclose(f);
    LOG_I("vcCloud: Loaded %d^3 noise (%d ch) from %s", *N, *channels, path);
    return data;
}

// Build the base noise texture.
// R = Perlin-Worley (4 cells)   — primary billowy cloud mass
// G = Worley (4 cells)          — first erosion octave
// B = Worley (8 cells)          — second erosion octave
// A = Worley (16 cells)         — third erosion octave
static GLuint vcCloud_GenBaseNoise(int N, const char* cachePath) {
    if (N <= 0) N = 64;

    int loadedN, loadedCh;
    unsigned char* data = vcCloud_LoadNoiseFromFile(cachePath, &loadedN, &loadedCh);

    if (data) {
        if (loadedN != N || loadedCh != 4) {
            LOG_W("vcCloud: Cached noise %s size/ch mismatch (got %d^3/%d, want %d^3/4). Re-baking.", 
                  cachePath, loadedN, loadedCh, N);
            free(data);
            data = nullptr;
        }
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_3D, id);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, N, N, N);

    if (data) {
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N, GL_RGBA, GL_UNSIGNED_BYTE, data);
        free(data);
    } else {
        LOG_I("vcCloud: Baking %d^3 base noise on GPU...", N);
        vcCloud_BakeBaseNoiseGPU(id, N);

        // Optional: Read back and save to cache
        if (cachePath && cachePath[0]) {
            data = (unsigned char*)malloc((size_t)N * N * N * 4);
            glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            vcCloud_SaveNoiseToFile(cachePath, N, 4, data);
            free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);

    LOG_I("vcCloud: base noise 3D tex %dx%dx%d id=%u", N,N,N, id);
    return id;
}

// Build the detail noise texture.
// R = Worley (4 cells)   — fine edge erosion
// G = Worley (8 cells)
// B = Worley (16 cells)
static GLuint vcCloud_GenDetailNoise(int N, const char* cachePath) {
    if (N <= 0) N = 32;

    int loadedN, loadedCh;
    unsigned char* data = vcCloud_LoadNoiseFromFile(cachePath, &loadedN, &loadedCh);

    if (data) {
        if (loadedN != N || (loadedCh != 3 && loadedCh != 4)) {
            LOG_W("vcCloud: Cached noise %s size/ch mismatch (got %d^3/%d, want %d^3/3 or 4). Re-baking.", 
                  cachePath, loadedN, loadedCh, N);
            free(data);
            data = nullptr;
        }
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_3D, id);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, N, N, N);

    if (data) {
        GLenum format = (loadedCh == 3) ? GL_RGB : GL_RGBA;
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N, format, GL_UNSIGNED_BYTE, data);
        free(data);
    } else {
        LOG_I("vcCloud: Baking %d^3 detail noise on GPU...", N);
        vcCloud_BakeDetailNoiseGPU(id, N);

        // Optional: Read back and save to cache (using 4 channels now)
        if (cachePath && cachePath[0]) {
            data = (unsigned char*)malloc((size_t)N * N * N * 4);
            glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            vcCloud_SaveNoiseToFile(cachePath, N, 4, data);
            free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);

    LOG_I("vcCloud: detail noise 3D tex %dx%dx%d id=%u", N,N,N, id);
    return id;
}

// ---- Blue noise (void-and-cluster) -----------------------------------------
//
// Generates a 32×32 R8 blue-noise texture once at startup. Used for ray-step
// jitter in stepJitterAdaptive — its flat-low-frequency / energy-at-high-freq
// spectrum lets TAA resolve the jitter pattern much faster than white noise.
//
// Algorithm (Ulichney 1993, simplified): iteratively place samples at the
// pixel with the minimum current "energy". Each placed sample adds a Gaussian
// bump to the energy field, so the next sample is forced into voids. The
// rank-based 0..255 output preserves spatial blue-noise structure.
static GLuint vcCloud_GenBlueNoise()
{
    const int N = 32;
    const int R = 5;                  // kernel half-width
    const int K = (2*R + 1);
    float* energy = (float*)calloc(N*N, sizeof(float));
    unsigned char* out = (unsigned char*)calloc(N*N, 1);
    bool*  placed = (bool*) calloc(N*N, sizeof(bool));
    float* kernel = (float*)malloc(K*K*sizeof(float));
    if (!energy || !out || !placed || !kernel) {
        free(energy); free(out); free(placed); free(kernel);
        LOG_E("vcCloud: OOM generating blue noise"); return 0;
    }

    for (int dy = -R; dy <= R; dy++)
    for (int dx = -R; dx <= R; dx++)
        kernel[(dy+R)*K + (dx+R)] = expf(-(float)(dx*dx + dy*dy) * 0.3f);

    for (int rank = 0; rank < N*N; rank++) {
        int   bestIdx = -1;
        float bestE   = 1e30f;
        for (int i = 0; i < N*N; i++) {
            if (placed[i]) continue;
            if (energy[i] < bestE) { bestE = energy[i]; bestIdx = i; }
        }
        if (bestIdx < 0) break;
        placed[bestIdx] = true;
        out[bestIdx] = (unsigned char)((rank * 255) / (N*N - 1));

        int px = bestIdx % N, py = bestIdx / N;
        for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            int x = ((px + dx) % N + N) % N;     // wrap
            int y = ((py + dy) % N + N) % N;
            energy[y*N + x] += kernel[(dy+R)*K + (dx+R)];
        }
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, N, N, 0, GL_RED, GL_UNSIGNED_BYTE, out);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(energy); free(out); free(placed); free(kernel);
    LOG_I("vcCloud: blue noise %dx%d id=%u", N, N, id);
    return id;
}

// ============================================================================
// Procedural weather map (noise-based, GL_CLAMP_TO_EDGE — covers world once)
//
// RGBA channels:
//   R = coverage       — Perlin-Worley at low freq; 2-4 large cloud groups
//   G = precipitation  — derived from high-coverage regions
//   B = cloud type     — Worley at independent freq; 0=stratus 1=cumulus
//   A = height scale   — FBM; 0=flat/stratus layer, 1=tall cumulus towers
//
// Sampled in the shader as:
//   box mode    → UV centered on the box,  extent = box XZ size
//   sphere mode → UV centered on camera,   extent = planetRadius
// ============================================================================

static GLuint vcCloud_GenProceduralWeatherMap(int width, int height, unsigned seed,
                                              VolumetricCloudNodeData* c = nullptr)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    if (s_WeatherGenProg) {
        LOG_I("vcCloud: Baking weather map on GPU (seed=%u, pattern=%d)...",
              seed, c ? c->weatherGen.patternType : 0);
        vcCloud_BakeWeatherMapGPU(id, width, height, seed, c);
    } else {
        LOG_W("vcCloud: Weather gen shader not ready, falling back to CPU baking.");
        unsigned char* data = (unsigned char*)malloc((size_t)width * height * 4);
        if (!data) { LOG_E("vcCloud: OOM generating weather map"); return 0; }

        float ox = (float)((seed * 1619u) & 0xffffu) / 65535.0f;
        float oz = (float)((seed * 31337u) & 0xffffu) / 65535.0f;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float u = (float)x / (float)width  + ox;
                float v = (float)y / (float)height + oz;
                float cov = vcn_perlinWorley(u, 0.45f, v, 3);
                cov = cov < 0.42f ? 0.0f : (cov - 0.42f) / 0.58f;
                cov = cov * cov * (3.0f - 2.0f * cov);
                float prec = vcn_clamp01((cov - 0.72f) * 3.5f);
                float type = vcn_worley(u * 1.17f + 0.13f, 0.5f, v * 0.93f + 0.07f, 5);
                type = type * type;
                float hs = vcn_fbm(u * 0.61f + 0.19f, 0.5f, v * 0.73f + 0.11f, 2, 3);
                hs = vcn_clamp01(hs * 1.9f - 0.35f);
                int idx = (y * width + x) * 4;
                data[idx+0] = (unsigned char)(vcn_clamp01(cov)  * 255 + 0.5f);
                data[idx+1] = (unsigned char)(vcn_clamp01(prec) * 255 + 0.5f);
                data[idx+2] = (unsigned char)(vcn_clamp01(type) * 255 + 0.5f);
                data[idx+3] = (unsigned char)(vcn_clamp01(hs)   * 255 + 0.5f);
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        free(data);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    LOG_I("vcCloud: procedural weather map %dx%d id=%u", width, height, id);
    return id;
}

// Generate (or regenerate) the auto weather map for a node.
// Destroys any existing auto-generated texture first.
static void vcCloud_RebuildAutoWeatherMap(SceneNode* node)
{
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;
    if (!c->autoWeatherMap) return;
    if (c->weatherMapTex) { glDeleteTextures(1, &c->weatherMapTex); c->weatherMapTex = 0; }
    // Seed from node address so every node gets a different base pattern.
    unsigned seed = (unsigned)(uintptr_t)(void*)node;
    int res = 256;
    if (c->weatherGen.texResolution == 1) res = 512;
    else if (c->weatherGen.texResolution == 2) res = 1024;
    c->weatherMapTex = vcCloud_GenProceduralWeatherMap(res, res, seed, c);
    c->useWeatherMap = true;
}

// ---- Compute shader program builder ----------------------------------------

static GLuint vcCloud_BuildComputeProg(const char* path)
{
    char* src = readShaderFile(path);
    if (!src) {
        LOG_E("vcCloud: failed to read compute shader: %s", path);
        return 0;
    }

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, (const GLchar**)&src, NULL);
    glCompileShader(shader);
    free(src);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_E("vcCloud: compute shader compile error:\n%s", log);
        glDeleteShader(shader);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glDetachShader(prog, shader);
    glDeleteShader(shader);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOG_E("vcCloud: compute program link error:\n%s", log);
        glDeleteProgram(prog);
        return 0;
    }

    LOG_I("vcCloud: compute program linked (id=%u)", prog);
    return prog;
}

// Rebuild the base noise texture for a node.
void vcCloud_RegenerateNoise(SceneNode* node)
{
    if (!node || node->type != ENTITY_VOLUMETRIC_CLOUD) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;

    if (c->noiseBaseTex)   glDeleteTextures(1, &c->noiseBaseTex);
    if (c->noiseDetailTex) glDeleteTextures(1, &c->noiseDetailTex);

    int baseRes = c->noiseRes;
    if (baseRes <= 0) baseRes = 64;
    int detailRes = baseRes / 2;
    if (detailRes < 8) detailRes = 8;

    c->noiseBaseTex   = vcCloud_GenBaseNoise(baseRes, c->noiseBaseTexPath);
    c->noiseDetailTex = vcCloud_GenDetailNoise(detailRes, c->noiseDetailTexPath);
}

// ---- Shared shader init (lazy, called once) --------------------------------

static bool vcCloud_InitShaders()
{
    if (s_ComputeProg && s_NoiseGenProg && s_WeatherGenProg && s_QuadProg && s_EmptyVAO) return true;

    if (!s_ComputeProg) {
        s_ComputeProg = vcCloud_BuildComputeProg(
            "engine/effects/clouds/cloud_compute.comp.glsl");
        if (!s_ComputeProg) return false;
    }

    if (!s_NoiseGenProg) {
        s_NoiseGenProg = vcCloud_BuildComputeProg(
            "engine/effects/clouds/noise_gen.comp.glsl");
        if (!s_NoiseGenProg) return false;
        s_NoiseGenLoc.baseRes   = glGetUniformLocation(s_NoiseGenProg, "uBaseRes");
        s_NoiseGenLoc.detailRes = glGetUniformLocation(s_NoiseGenProg, "uDetailRes");
        s_NoiseGenLoc.mode      = glGetUniformLocation(s_NoiseGenProg, "uMode");
    }

    if (!s_WeatherGenProg) {
        s_WeatherGenProg = vcCloud_BuildComputeProg(
            "engine/effects/clouds/weather_gen.comp.glsl");
        if (!s_WeatherGenProg) return false;
        s_WeatherGenLoc.width        = glGetUniformLocation(s_WeatherGenProg, "uWidth");
        s_WeatherGenLoc.height       = glGetUniformLocation(s_WeatherGenProg, "uHeight");
        s_WeatherGenLoc.seed         = glGetUniformLocation(s_WeatherGenProg, "uSeed");
        s_WeatherGenLoc.patternType  = glGetUniformLocation(s_WeatherGenProg, "uPatternType");
        s_WeatherGenLoc.centerX      = glGetUniformLocation(s_WeatherGenProg, "uCenterX");
        s_WeatherGenLoc.centerY      = glGetUniformLocation(s_WeatherGenProg, "uCenterY");
        s_WeatherGenLoc.arms         = glGetUniformLocation(s_WeatherGenProg, "uArms");
        s_WeatherGenLoc.tightness    = glGetUniformLocation(s_WeatherGenProg, "uTightness");
        s_WeatherGenLoc.falloffRadius= glGetUniformLocation(s_WeatherGenProg, "uFalloffRadius");
        s_WeatherGenLoc.bandAngle    = glGetUniformLocation(s_WeatherGenProg, "uBandAngle");
        s_WeatherGenLoc.bandWidth    = glGetUniformLocation(s_WeatherGenProg, "uBandWidth");
        s_WeatherGenLoc.bandSpacing  = glGetUniformLocation(s_WeatherGenProg, "uBandSpacing");
        s_WeatherGenLoc.bandTurbulence=glGetUniformLocation(s_WeatherGenProg, "uBandTurbulence");
        s_WeatherGenLoc.noiseFreq    = glGetUniformLocation(s_WeatherGenProg, "uNoiseFreq");
        s_WeatherGenLoc.coverageScale= glGetUniformLocation(s_WeatherGenProg, "uCoverageScale");
        s_WeatherGenLoc.coverageMin  = glGetUniformLocation(s_WeatherGenProg, "uCoverageMin");
        s_WeatherGenLoc.coverageMax  = glGetUniformLocation(s_WeatherGenProg, "uCoverageMax");
    }

    if (!s_QuadProg) {
        const char* files[5] = {
            "engine/effects/clouds/cloud_quad.vert.glsl",
            NULL, NULL, NULL,
            "engine/effects/clouds/cloud_quad.frag.glsl"
        };
        buildShaderProgramFromFiles(files, 5, &s_QuadProg, attribNames, attribIndices, 4);
        if (!s_QuadProg) return false;
        s_QuadProg->name = "CloudQuad";
        s_QuadCloudTexLoc = glGetUniformLocation(s_QuadProg->id, "u_cloudTex");
        s_QuadDepthTexLoc = glGetUniformLocation(s_QuadProg->id, "u_depthTex");
        s_QuadDepthLoc    = glGetUniformLocation(s_QuadProg->id, "u_depth");
    }

    if (!s_EmptyVAO)
        glGenVertexArrays(1, &s_EmptyVAO);

    if (!s_BlueNoiseTex)
        s_BlueNoiseTex = vcCloud_GenBlueNoise();

    // Initialize shared noise textures if not already done
    if (!s_NoiseTexBase)
        s_NoiseTexBase = vcCloud_GenBaseNoise(64, "engine/effects/clouds/nvdf_textures/base_noise.n3d");
    if (!s_NoiseTexDetail)
        s_NoiseTexDetail = vcCloud_GenDetailNoise(32, "engine/effects/clouds/nvdf_textures/detail_noise.n3d");

    return true;
}

// ---- Sphere grid generation ------------------------------------------------

static void vcCloud_GenerateSpheres(SceneNode* node)
{
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;

    srand((unsigned int)time(NULL) ^ (unsigned int)(size_t)node);

    if (!c->sphereData)
        c->sphereData = (float*)malloc(256 * 4 * sizeof(float));
    c->sphereCount = 0;

    // Spheres are in LOCAL space (centred at origin). The shader transforms
    // them to world space via u_worldMatrix. This means node translate/rotate/scale
    // all work correctly without regenerating spheres.
    float startX = -(c->gridX  - 1) * c->gridSpacing * 0.5f;
    float startZ = -(c->gridZ  - 1) * c->gridSpacing * 0.5f;

    int perMin = c->spheresPerCloudMin;
    int perMax = c->spheresPerCloudMax;
    if (perMin > perMax) perMin = perMax;

    auto rnd = [](float lo, float hi) -> float {
        return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
    };

    for (int gx = 0; gx < c->gridX && c->sphereCount < 255; gx++) {
        for (int gz = 0; gz < c->gridZ && c->sphereCount < 255; gz++) {
            float cx = startX + gx * c->gridSpacing;
            float cy = 4.0f;
            float cz = startZ + gz * c->gridSpacing;

            int cnt = perMin + (rand() % (perMax - perMin + 1));
            if (c->sphereCount + cnt > 256) cnt = 256 - c->sphereCount;
            if (cnt <= 0) break;

            int numLow = (int)(cnt * 0.6f);
            if (numLow < 2 && cnt >= 3) numLow = 2;
            int numUp  = cnt - numLow;

            for (int k = 0; k < numLow && c->sphereCount < 256; k++) {
                float* s = &c->sphereData[c->sphereCount * 4];
                s[0] = cx + rnd(-7.5f, 7.5f)  * c->gridScale;
                s[1] = cy + rnd(-0.25f, 0.25f) * c->gridScale;
                s[2] = cz + rnd(-1.5f, 1.5f)   * c->gridScale;
                s[3] = (2.0f + rnd(0.0f, 1.0f)) * c->gridScale;
                c->sphereCount++;
            }
            for (int k = 0; k < numUp && c->sphereCount < 256; k++) {
                float* s = &c->sphereData[c->sphereCount * 4];
                s[0] = cx + rnd(-6.5f, 6.5f) * c->gridScale;
                s[1] = cy + rnd(2.0f, 3.0f)  * c->gridScale;
                s[2] = cz + rnd(-1.0f, 1.0f) * c->gridScale;
                s[3] = (1.0f + rnd(0.0f, 0.6f)) * c->gridScale;
                c->sphereCount++;
            }
        }
    }

    c->spheresDirty = true;
    LOG_I("vcCloud: generated %d local-space spheres (%dx%d grid)", c->sphereCount, c->gridX, c->gridZ);
}

// ---- Output texture creation -----------------------------------------------

static GLuint vcCloud_MakeRGBA16FTex(int w, int h)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Clear to transparent black so TAA history starts clean
    static const float zero[4] = {};
    glClearTexImage(id, 0, GL_RGBA, GL_FLOAT, zero);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

static GLuint vcCloud_MakeR32FTex(int w, int h)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    static const float one = 1.0f;
    glClearTexImage(id, 0, GL_RED, GL_FLOAT, &one);  // default = far plane
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

static void vcCloud_CreateOutputTex(VolumetricCloudNodeData* c)
{
    if (c->outputTex)     glDeleteTextures(1, &c->outputTex);
    if (c->historyTex)    glDeleteTextures(1, &c->historyTex);
    if (c->farOutputTex)  glDeleteTextures(1, &c->farOutputTex);
    if (c->farHistoryTex) glDeleteTextures(1, &c->farHistoryTex);

    c->outputTex  = vcCloud_MakeRGBA16FTex(c->outputW, c->outputH);
    c->historyTex = vcCloud_MakeRGBA16FTex(c->outputW, c->outputH);

    if (c->useDualPass) {
        c->farOutputTex  = vcCloud_MakeRGBA16FTex(c->farOutputW, c->farOutputH);
        c->farHistoryTex = vcCloud_MakeRGBA16FTex(c->farOutputW, c->farOutputH);
    } else {
        c->farOutputTex = c->farHistoryTex = 0;
    }
    
    c->frameIndex = 0;
    LOG_I("vcCloud: textures %dx%d initialized", c->outputW, c->outputH);
}

// ---- SSBO update -----------------------------------------------------------

static void vcCloud_UploadSpheres(VolumetricCloudNodeData* c)
{
    if (!c->spheresDirty || c->sphereCount == 0) return;

    size_t bytes = (size_t)c->sphereCount * 4 * sizeof(float);

    if (!c->sphereSSBO) {
        glGenBuffers(1, &c->sphereSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, c->sphereSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, c->sphereData, GL_DYNAMIC_DRAW);
    } else {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, c->sphereSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, c->sphereData, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    c->spheresDirty = false;
}

// ============================================================================
// Public: scene-graph hooks
// ============================================================================

void sg_InitVolumetricCloudNode(SceneNode* node)
{
    if (!node) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;

    // Apply defaults only when uninitialised (renderScale == 0 means fresh)
    if (c->renderScale <= 0.0f) {
        // Appearance — realistic defaults inspired by real cumulus clouds
        c->cloudColorTop    = vec3(0.96f, 0.97f, 1.00f); // near-white with slight sky blue
        c->cloudColorBottom = vec3(0.55f, 0.60f, 0.70f); // blue-gray shadow underside
        c->absorption       = 0.18f;
        c->coverage         = 0.5f;   // slightly positive = decent cloud fill
        c->erosion          = 0.50f;  // balanced solid/wispy
        c->silverLining     = 0.6f;
        c->flatBottom       = false;
        c->cloudType        = 0.85f;  // close to cumulus
        c->noiseScale       = 0.006f; // ~3 noise cells across 500-unit box
        c->detailScale      = 0.035f; // ~17 detail cells across box
        c->noiseRes         = 64;     // Default 64^3
        c->noiseBaseTexPath[0] = '\0';
        c->noiseDetailTexPath[0] = '\0';
        c->sunDirection     = normalize(vec3(0.0f, 0.342f, 0.940f));
        c->sunColor         = vec3(1.0f, 0.97f, 0.90f); // near-white sun
        c->sunIntensity     = 8.0f;
        c->ambientStrength  = 1.2f;
        c->scatterG         = 0.35f;
        c->densityScale     = 4.0f;
        c->maxSteps         = 96;    // more steps for 500-unit box scale
        c->stepSize         = 1.5f;  // world-unit step; adaptive skip handles empty regions
        c->turbulence         = 0.5f;
        c->windSpeed          = 0.5f;
        c->localNoiseSpeed    = 0.5f;
        c->boxSize            = vec3(500.0f, 150.0f, 500.0f);
        c->gridX              = 3;
        c->gridZ              = 3;
        c->gridSpacing        = 20.0f;
        c->gridScale          = 1.0f;
        c->spheresPerCloudMin = 3;
        c->spheresPerCloudMax = 8;
        c->renderScale        = 0.50f; // 50% res — checkerboard halves effective cost vs 35% non-CB
        c->enableTAA          = false;
        c->taaBlend           = 0.1f;
        // NVDF defaults — off by default, kicks in once a .nvdf path is set.
        c->useNVDF        = false;
        c->nvdfPath[0]    = '\0';
        c->hwModStrength  = 0.6f;
        c->hwModScale     = 0.0025f;
        c->curlStrength   = 0.4f;
        c->nvdfTileScale  = 0.002f;   // one tile = 500 m (fits a default 500×500 cloud volume)
        c->nvdfRotAngle   = 0.0f;
        c->nvdfRotX       = 0.0f;
        c->nvdfRotZ       = 0.0f;
        c->nvdfWorldOffset= vec3(0.0f);
        c->nvdfYOffset    = 0.0f;
        // Circle field
        c->useCircleField = false;
        c->circleRadius   = 250.0f;
        // Sphere field — defaults give a gentle dome over a few km of view
        c->useSphereField  = false;
        c->planetRadius    = 6000.0f;   // ~6 km curvature radius
        c->cloudBaseHeight = 500.0f;
        c->cloudThickness  = 800.0f;
        c->domeExtent      = 0.5f;     // Default to hemisphere (top half only)
        // Weather map
        c->useWeatherMap        = false;        c->weatherMapPath[0]    = '\0';
        c->weatherMapScale      = 0.002f;
        c->weatherMapTex        = 0;
        c->autoWeatherMap       = true;   // on by default; provides placement + variety
        c->weatherMapGridExtent = 0.0f;   // 0 = auto-derive from box/sphere size in shader
        c->weatherGen.patternType    = 0;
        c->weatherGen.centerX        = 0.5f;
        c->weatherGen.centerY        = 0.5f;
        c->weatherGen.arms           = 2;
        c->weatherGen.tightness      = 5.0f;
        c->weatherGen.falloffRadius  = 0.4f;
        c->weatherGen.bandAngle      = 0.0f;
        c->weatherGen.bandWidth      = 0.4f;
        c->weatherGen.bandSpacing    = 0.15f;
        c->weatherGen.bandTurbulence = 0.05f;
        c->weatherGen.noiseFreq      = 1.0f;
        c->weatherGen.coverageScale  = 0.6f;
        c->weatherGen.coverageMin    = 0.0f;
        c->weatherGen.coverageMax    = 1.0f;
        c->useBaseNoise     = true;
        c->useWorleyErosion = true;
        c->useDetailNoise   = true;
        c->weatherGen.texResolution  = 0;        // 0=256
        c->weatherGen.followCamera   = true;
        c->weatherGen.worldAnchorX   = 0.0f;
        c->weatherGen.worldAnchorZ   = 0.0f;
        // Atmospheric fog
        c->fogColor   = vec3(0.70f, 0.78f, 0.86f);
        c->fogDensity = 0.0f;
        c->fogStart   = 300.0f;
        // Adaptive raymarching
        c->adaptiveFactor    = 0.003f;  // step hits 2× stepSize at ~1000m (cap is 4×)
        c->jitterSwitchDist  = 200.0f;
        // Dual-pass
        c->useDualPass  = false;
        c->nearFarSplit = 200.0f;
        c->nearOutputW  = 480;  c->nearOutputH  = 270;
        c->farOutputW   = 960;  c->farOutputH   = 540;
    }

    // Anti-zero fallbacks (project saved before these fields existed)
    if (c->hwModScale      <= 0.0f) c->hwModScale      = 0.0025f;
    if (c->nvdfTileScale   <= 0.0f) c->nvdfTileScale   = 0.002f;
    if (c->adaptiveFactor  <= 0.0f) c->adaptiveFactor  = 0.003f;
    if (c->jitterSwitchDist<= 0.0f) c->jitterSwitchDist= 200.0f;
    if (c->nearFarSplit    <= 0.0f) c->nearFarSplit     = 200.0f;
    if (c->nearOutputW     <= 0)    c->nearOutputW = 480;
    if (c->nearOutputH     <= 0)    c->nearOutputH = 270;
    if (c->farOutputW      <= 0)    c->farOutputW  = 960;
    if (c->farOutputH      <= 0)    c->farOutputH  = 540;
    // localNoiseSpeed added after initial release — old scenes load it as 0.
    // Inherit windSpeed so the cloud surface still animates as before.
    if (c->localNoiseSpeed <= 0.0f)
        c->localNoiseSpeed = c->windSpeed > 0.0f ? c->windSpeed : 0.5f;

    // Per-entity NVDF randomization — generated once, then persisted with scene.
    // Using a simple hash of the node pointer for the first-ever init.
    if (c->nvdfRotAngle == 0.0f && c->nvdfWorldOffset[0] == 0.0f
        && c->nvdfWorldOffset[2] == 0.0f) {
        uintptr_t seed = (uintptr_t)(void*)node;
        seed ^= seed >> 33;  seed *= 0xff51afd7ed558ccdULL;
        seed ^= seed >> 33;  seed *= 0xc4ceb9fe1a85ec53ULL;
        seed ^= seed >> 33;
        auto rndF = [](uintptr_t s, int n) -> float {
            s ^= (uintptr_t)n * 0x9e3779b97f4a7c15ULL;
            s ^= s >> 17;  s *= 0xbf58476d1ce4e5b9ULL;
            return (float)(s & 0xffffffu) * (1.0f / (float)0x1000000u);
        };
        c->nvdfRotAngle    = rndF(seed, 0) * 6.28318530f;
        c->nvdfWorldOffset = vec3(rndF(seed, 1) * 1200.0f - 600.0f,
                                  0.0f,
                                  rndF(seed, 2) * 1200.0f - 600.0f);
    }

    // Always apply Nubis-specific defaults if they were never set.
    // These fields are new and won't be present in projects saved before the
    // Nubis rewrite, so gating only on renderScale==0 would leave them at 0.
    if (c->cloudType   <= 0.0f) c->cloudType   = 0.85f;
    if (c->noiseScale  <= 0.0f) c->noiseScale  = 0.006f;
    if (c->detailScale <= 0.0f) c->detailScale = 0.035f;
    if (c->noiseRes    <= 0)    c->noiseRes    = 64;

    extern int viewportWidth, viewportHeight;
    c->outputW = (int)(viewportWidth  * c->renderScale);
    c->outputH = (int)(viewportHeight * c->renderScale);

    if (!vcCloud_InitShaders()) {
        LOG_E("vcCloud: shader init failed for node '%s'", node->name);
        return;
    }

    if (!c->noiseBaseTex || !c->noiseDetailTex)
        vcCloud_RegenerateNoise(node);

    // Cache uniform locations once — avoids driver round-trips every frame.
    auto& L = c->computeLoc;
    GLuint p = s_ComputeProg;
    L.cameraPos      = glGetUniformLocation(p, "u_cameraPos");
    L.view           = glGetUniformLocation(p, "u_view");
    L.proj              = glGetUniformLocation(p, "u_proj");
    L.nonJitteredProj   = glGetUniformLocation(p, "u_nonJitteredProj");
    L.invView           = glGetUniformLocation(p, "u_invView"); // We'll just pass u_view and let shader invert
    L.worldMatrix    = glGetUniformLocation(p, "u_worldMatrix");
    L.time           = glGetUniformLocation(p, "u_time");
    L.sphereCount    = glGetUniformLocation(p, "u_sphereCount");
    L.densityScale   = glGetUniformLocation(p, "u_densityScale");
    L.maxSteps       = glGetUniformLocation(p, "u_maxSteps");
    L.stepSize       = glGetUniformLocation(p, "u_stepSize");
    L.turbulence      = glGetUniformLocation(p, "u_turbulence");
    L.windSpeed       = glGetUniformLocation(p, "u_windSpeed");
    L.localNoiseSpeed = glGetUniformLocation(p, "u_localNoiseSpeed");
    L.boxMin         = glGetUniformLocation(p, "u_boxMin");
    L.boxMax         = glGetUniformLocation(p, "u_boxMax");
    L.sunDir         = glGetUniformLocation(p, "u_sunDir");
    L.sunColor       = glGetUniformLocation(p, "u_sunColor");
    L.sunIntensity   = glGetUniformLocation(p, "u_sunIntensity");
    L.ambientStrength= glGetUniformLocation(p, "u_ambientStrength");
    L.scatterG       = glGetUniformLocation(p, "u_scatterG");
    L.cloudColorTop  = glGetUniformLocation(p, "u_cloudColorTop");
    L.cloudColorBottom=glGetUniformLocation(p, "u_cloudColorBottom");
    L.absorption     = glGetUniformLocation(p, "u_absorption");
    L.coverage       = glGetUniformLocation(p, "u_coverage");
    L.erosion        = glGetUniformLocation(p, "u_erosion");
    L.silverLining   = glGetUniformLocation(p, "u_silverLining");
    L.flatBottom     = glGetUniformLocation(p, "u_flatBottom");
    L.cloudType      = glGetUniformLocation(p, "u_cloudType");
    L.noiseScale     = glGetUniformLocation(p, "u_noiseScale");
    L.detailScale    = glGetUniformLocation(p, "u_detailScale");
    L.noiseBase      = glGetUniformLocation(p, "u_noiseBase");
    L.noiseDetail    = glGetUniformLocation(p, "u_noiseDetail");
    L.enableTAA      = glGetUniformLocation(p, "u_enableTAA");
    L.frameIndex     = glGetUniformLocation(p, "u_frameIndex");
    L.taaBlend       = glGetUniformLocation(p, "u_taaBlend");
    L.prevCameraPos  = glGetUniformLocation(p, "u_prevCameraPos");
    L.prevView       = glGetUniformLocation(p, "u_prevView");
    L.prevProj       = glGetUniformLocation(p, "u_prevProj");
    L.historyTex     = glGetUniformLocation(p, "u_historyTex");
    L.useNVDF         = glGetUniformLocation(p, "u_useNVDF");
    L.nvdfTex         = glGetUniformLocation(p, "u_nvdfTex");
    L.hwModStrength   = glGetUniformLocation(p, "u_hwModStrength");
    L.hwModScale      = glGetUniformLocation(p, "u_hwModScale");
    L.curlStrength    = glGetUniformLocation(p, "u_curlStrength");
    L.nvdfTileScale   = glGetUniformLocation(p, "u_nvdfTileScale");
    L.nvdfWorldOffset = glGetUniformLocation(p, "u_nvdfWorldOffset");
    L.nvdfRotMat      = glGetUniformLocation(p, "u_nvdfRotMat");
    L.nvdfYOffset     = glGetUniformLocation(p, "u_nvdfYOffset");
    L.adaptiveFactor  = glGetUniformLocation(p, "u_adaptiveFactor");
    L.jitterSwitchDist= glGetUniformLocation(p, "u_jitterSwitchDist");
    L.passMode        = glGetUniformLocation(p, "u_passMode");
    L.nearFarSplit    = glGetUniformLocation(p, "u_nearFarSplit");
    L.useCircleField  = glGetUniformLocation(p, "u_useCircleField");
    L.circleRadius    = glGetUniformLocation(p, "u_circleRadius");
    L.useSphereField  = glGetUniformLocation(p, "u_useSphereField");
    L.planetRadius    = glGetUniformLocation(p, "u_planetRadius");
    L.cloudBaseHeight = glGetUniformLocation(p, "u_cloudBaseHeight");
    L.cloudThickness  = glGetUniformLocation(p, "u_cloudThickness");
    L.domeExtent      = glGetUniformLocation(p, "u_domeExtent");
    L.blueNoise       = glGetUniformLocation(p, "u_blueNoise");
    L.useWeatherMap        = glGetUniformLocation(p, "u_useWeatherMap");
    L.weatherMap           = glGetUniformLocation(p, "u_weatherMap");
    L.weatherMapScale      = glGetUniformLocation(p, "u_weatherMapScale");
    L.autoWeatherMap       = glGetUniformLocation(p, "u_autoWeatherMap");
    L.weatherMapAnchor     = glGetUniformLocation(p, "u_weatherMapAnchor");
    L.weatherMapGridExtent = glGetUniformLocation(p, "u_weatherMapGridExtent");
    L.useBaseNoise         = glGetUniformLocation(p, "u_useBaseNoise");
    L.useWorleyErosion     = glGetUniformLocation(p, "u_useWorleyErosion");
    L.useDetailNoise       = glGetUniformLocation(p, "u_useDetailNoise");
    L.fogColor             = glGetUniformLocation(p, "u_fogColor");
    L.fogDensity      = glGetUniformLocation(p, "u_fogDensity");
    L.fogStart        = glGetUniformLocation(p, "u_fogStart");

    vcCloud_CreateOutputTex(c);
    vcCloud_GenerateSpheres(node);
    vcCloud_UploadSpheres(c);

    // Auto-load NVDF if a path was persisted from a previous session.
    if (c->nvdfPath[0] != '\0' && !c->nvdfTex)
        vcCloud_LoadNVDF(node, c->nvdfPath);

    // Auto-load weather map similarly.
    if (c->weatherMapPath[0] != '\0' && !c->weatherMapTex)
        vcCloud_LoadWeatherMap(node, c->weatherMapPath);

    // Build the auto weather map if enabled and not yet generated.
    if (c->autoWeatherMap && !c->weatherMapTex)
        vcCloud_RebuildAutoWeatherMap(node);
}

void sg_RenderVolumetricCloudNode(SceneNode* node, mat4 view, mat4 proj)
{
    if (!node || !s_ComputeProg || !s_QuadProg) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;
    if (!c->outputTex || !c->historyTex) return;
    if (!c->noiseBaseTex || !c->noiseDetailTex) return;

    vcCloud_UploadSpheres(c);

    // ---- Compute pass ----
    glUseProgram(s_ComputeProg);

    // SSBO binding 1 — spheres are now 2D XZ coverage footprints, not 3D SDF
    if (c->sphereSSBO)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, c->sphereSSBO);

    // Noise textures (texture units 0 and 1 — separate from image units 0/1)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, c->noiseBaseTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, c->noiseDetailTex);
    glActiveTexture(GL_TEXTURE0);

    const auto& L = c->computeLoc;

    vec3 camPos = GetActiveCameraPosition();
    static vec3 s_prevCamPos = camPos;
    static mat4 s_prevView   = view;
    static mat4 s_prevProj   = proj;
    glUniform3fv(L.cameraPos,     1, (float*)&camPos);
    glUniform3fv(L.prevCameraPos, 1, (float*)&s_prevCamPos);
    glUniformMatrix4fv(L.prevView, 1, GL_FALSE, s_prevView);
    glUniformMatrix4fv(L.prevProj, 1, GL_FALSE, s_prevProj);
    glUniform1i(L.historyTex, 2);  // texture unit 2
    s_prevCamPos = camPos;
    s_prevView   = view;
    s_prevProj   = proj;
    glUniformMatrix4fv(L.view, 1, GL_FALSE, view);

    // ---- Halton sub-pixel jitter for true TAA ----------------------------------
    // Shift the projection matrix by a sub-pixel offset each frame using the
    // Halton(2,3) low-discrepancy sequence. Without this, TAA only acts as a
    // denoiser for blue noise — it does NOT reconstruct sub-pixel edge detail.
    // With jitter, the camera samples a new sub-pixel location each frame and
    // TAA truly accumulates fine geometric detail.
    mat4 jitteredProj = proj;
    if (c->enableTAA) {
        int fidx = c->frameIndex % 16;
        // Halton base-2 and base-3 in [0,1], recentred to [-0.5, 0.5]
        auto halton = [](int idx, int base) -> float {
            float r = 0.0f, f = 1.0f;
            while (idx > 0) { f /= (float)base; r += f * (float)(idx % base); idx /= base; }
            return r;
        };
        float jx = (halton(fidx, 2) - 0.5f) * 2.0f / (float)c->outputW;
        float jy = (halton(fidx, 3) - 0.5f) * 2.0f / (float)c->outputH;
        // OpenGL projection matrix is column-major; jitter lives in column 2 (NDC/W)
        jitteredProj[2][0] += jx;
        jitteredProj[2][1] += jy;
    }
    glUniformMatrix4fv(L.proj, 1, GL_FALSE, jitteredProj);
    glUniformMatrix4fv(L.nonJitteredProj, 1, GL_FALSE, proj);
    
    glUniformMatrix4fv(L.worldMatrix, 1, GL_FALSE, node->world_matrix);
    glUniform1f(L.time, platformGetTime());

    glUniform1i(L.sphereCount,  c->sphereCount);
    glUniform1f(L.densityScale, c->densityScale);
    glUniform1i(L.maxSteps,     c->maxSteps);
    glUniform1f(L.stepSize,     c->stepSize);
    glUniform1f(L.turbulence,      c->turbulence);
    glUniform1f(L.windSpeed,       c->windSpeed);
    glUniform1f(L.localNoiseSpeed, c->localNoiseSpeed);

    vec3 worldPos = vec3(node->world_matrix[3][0],
                         node->world_matrix[3][1],
                         node->world_matrix[3][2]);
    float sx = sqrtf(node->world_matrix[0][0]*node->world_matrix[0][0] +
                     node->world_matrix[0][1]*node->world_matrix[0][1] +
                     node->world_matrix[0][2]*node->world_matrix[0][2]);
    float sy = sqrtf(node->world_matrix[1][0]*node->world_matrix[1][0] +
                     node->world_matrix[1][1]*node->world_matrix[1][1] +
                     node->world_matrix[1][2]*node->world_matrix[1][2]);
    float sz = sqrtf(node->world_matrix[2][0]*node->world_matrix[2][0] +
                     node->world_matrix[2][1]*node->world_matrix[2][1] +
                     node->world_matrix[2][2]*node->world_matrix[2][2]);
    vec3 halfBox = vec3(c->boxSize[0]*sx, c->boxSize[1]*sy, c->boxSize[2]*sz) * 0.5f;
    vec3 boxMin  = worldPos - halfBox;
    vec3 boxMax  = worldPos + halfBox;
    glUniform3fv(L.boxMin, 1, (float*)&boxMin);
    glUniform3fv(L.boxMax, 1, (float*)&boxMax);

    vec3 sunDir = c->sunDirection;
    float sunLen = sqrtf(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    if (sunLen > 1e-5f) { sunDir[0] /= sunLen; sunDir[1] /= sunLen; sunDir[2] /= sunLen; }
    else                { sunDir = vec3(0.0f, 1.0f, 0.0f); }

    glUniform3fv(L.sunDir,           1, (float*)&sunDir);
    glUniform3fv(L.sunColor,         1, (float*)&c->sunColor);
    glUniform1f(L.sunIntensity,      c->sunIntensity);
    glUniform1f(L.ambientStrength,   c->ambientStrength);
    glUniform1f(L.scatterG,          c->scatterG);
    glUniform3fv(L.cloudColorTop,    1, (float*)&c->cloudColorTop);
    glUniform3fv(L.cloudColorBottom, 1, (float*)&c->cloudColorBottom);
    glUniform1f(L.absorption,        c->absorption);
    glUniform1f(L.coverage,          c->coverage);
    glUniform1f(L.erosion,           c->erosion);
    glUniform1f(L.silverLining,      c->silverLining);
    glUniform1i(L.flatBottom,        c->flatBottom ? 1 : 0);
    glUniform1f(L.cloudType,         c->cloudType);
    glUniform1f(L.noiseScale,        c->noiseScale);
    glUniform1f(L.detailScale,       c->detailScale);
    glUniform1i(L.noiseBase,   0);   // texture unit 0
    glUniform1i(L.noiseDetail, 1);   // texture unit 1

    // NVDF binding (texture unit 3). useNVDF only takes effect when a texture
    // is actually loaded — otherwise we'd sample an unbound sampler3D (UB).
    bool nvdfReady = c->useNVDF && c->nvdfTex != 0;
    if (nvdfReady) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, c->nvdfTex);
        glActiveTexture(GL_TEXTURE0);
    }
    glUniform1i(L.useNVDF,       nvdfReady ? 1 : 0);
    glUniform1i(L.nvdfTex,       3);
    glUniform1f(L.hwModStrength, c->hwModStrength);
    glUniform1f(L.hwModScale,    c->hwModScale);
    glUniform1f(L.curlStrength,  c->curlStrength);
    glUniform1f(L.nvdfTileScale,    c->nvdfTileScale);
    glUniform3fv(L.nvdfWorldOffset, 1, (float*)&c->nvdfWorldOffset);
    {
        // Build rotation matrix (Ry * Rx * Rz, Euler YXZ order) for texture UVW.
        // Uploaded column-major (GL_FALSE = no transpose).
        float cx = cosf(c->nvdfRotX),  sx = sinf(c->nvdfRotX);
        float cy = cosf(c->nvdfRotAngle), sy = sinf(c->nvdfRotAngle);
        float cz = cosf(c->nvdfRotZ),  sz = sinf(c->nvdfRotZ);
        float m[9] = {
            // col 0
             cy*cz + sy*sx*sz,
             cx*sz,
            -sy*cz + cy*sx*sz,
            // col 1
            -cy*sz + sy*sx*cz,
             cx*cz,
             sy*sz + cy*sx*cz,
            // col 2
             sy*cx,
            -sx,
             cy*cx
        };
        glUniformMatrix3fv(L.nvdfRotMat, 1, GL_FALSE, m);
    }
    glUniform1f(L.nvdfYOffset,      c->nvdfYOffset);
    glUniform1f(L.adaptiveFactor,   c->adaptiveFactor);
    glUniform1f(L.jitterSwitchDist, c->jitterSwitchDist);
    glUniform1f(L.nearFarSplit,     c->nearFarSplit);
    glUniform1i(L.useCircleField,   c->useCircleField ? 1 : 0);
    glUniform1f(L.circleRadius,     c->circleRadius);
    glUniform1i(L.useSphereField,   c->useSphereField ? 1 : 0);
    glUniform1f(L.planetRadius,     c->planetRadius);
    glUniform1f(L.cloudBaseHeight,  c->cloudBaseHeight);
    glUniform1f(L.cloudThickness,   c->cloudThickness);
    glUniform1f(L.domeExtent,       c->domeExtent);

    // Blue noise (texture unit 5 — shared across all cloud nodes)
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, s_BlueNoiseTex);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(L.blueNoise, 5);

    // Weather map (texture unit 4 — per-node)
    bool wmReady = c->useWeatherMap && c->weatherMapTex != 0;
    if (wmReady) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, c->weatherMapTex);
        glActiveTexture(GL_TEXTURE0);
    }
    glUniform1i(L.useWeatherMap,   wmReady ? 1 : 0);
    glUniform1i(L.weatherMap,      4);
    glUniform1f(L.weatherMapScale, c->weatherMapScale);
    glUniform1i(L.autoWeatherMap,  (wmReady && c->autoWeatherMap) ? 1 : 0);

    // Weather map anchor: camera-follow or fixed world XZ position.
    float anchorX = (c->autoWeatherMap && !c->weatherGen.followCamera)
                    ? c->weatherGen.worldAnchorX : camPos[0];
    float anchorZ = (c->autoWeatherMap && !c->weatherGen.followCamera)
                    ? c->weatherGen.worldAnchorZ : camPos[2];
    glUniform2f(L.weatherMapAnchor, anchorX, anchorZ);
    glUniform1f(L.weatherMapGridExtent, c->weatherMapGridExtent);

    glUniform1i(L.useBaseNoise,      c->useBaseNoise      ? 1 : 0);
    glUniform1i(L.useWorleyErosion,  c->useWorleyErosion  ? 1 : 0);
    glUniform1i(L.useDetailNoise,    c->useDetailNoise    ? 1 : 0);

    // Atmospheric fog
    glUniform3fv(L.fogColor, 1, (float*)&c->fogColor);
    glUniform1f(L.fogDensity, c->fogDensity);
    glUniform1f(L.fogStart,   c->fogStart);

    glUniform1i(L.useSceneDepth, 0);

    glUniform1i(L.enableTAA,  c->enableTAA ? 1 : 0);
    glUniform1f(L.taaBlend,   c->taaBlend);

    // ---- Auto-resize textures if viewport changed ----
    extern int viewportWidth, viewportHeight;
    int targetW = (int)(viewportWidth  * c->renderScale);
    int targetH = (int)(viewportHeight * c->renderScale);
    if (targetW != c->outputW || targetH != c->outputH) {
        c->outputW = targetW;
        c->outputH = targetH;
        vcCloud_CreateOutputTex(c);
    }

    // Helper that binds image0 + history2, dispatches, then TAA-pings the pair.
    auto doDispatch = [&](GLuint& outTex, GLuint& histTex, int w, int h,
                          int& fIdx, int passMode_)
    {
        glBindImageTexture(0, outTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, histTex);
        glActiveTexture(GL_TEXTURE0);

        glUniform1i(L.frameIndex, fIdx);
        glUniform1i(L.passMode,   passMode_);

        GLuint gx = ((GLuint)w + 7) / 8;
        GLuint gy = ((GLuint)h + 7) / 8;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        fIdx++;
        if (c->enableTAA) { GLuint tmp = outTex; outTex = histTex; histTex = tmp; }
    };

    // ---- Compute pass(es) ----
    if (c->useDualPass && !c->farOutputTex) {
        vcCloud_CreateOutputTex(c); // Ensure all dual-pass textures exist
    }

    if (!c->useDualPass) {
        doDispatch(c->outputTex, c->historyTex, 
                   c->outputW, c->outputH, c->frameIndex, 0);
    } else {
        if (c->farOutputTex) {
            doDispatch(c->farOutputTex, c->farHistoryTex,
                       c->farOutputW, c->farOutputH, c->farFrameIndex, 2);
        }
        doDispatch(c->outputTex, c->historyTex,
                   c->outputW, c->outputH, c->frameIndex, 1);
    }

    // ---- Quad composite pass(es) ----
    //
    // The quad sits at NDC z = 0.99 (depth ≈ 0.995).
    // GL_LESS passes only where existing depth > 0.995 — i.e. sky/far-plane
    // pixels — so opaque geometry always wins regardless of camera angle.
    // GL_FALSE depth mask keeps the cloud from writing to the depth buffer.
    glUseProgram(s_QuadProg->id);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(s_EmptyVAO);

    auto doQuad = [&](GLuint tex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(s_QuadCloudTexLoc, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    };

    if (c->useDualPass && c->farOutputTex) {
        doQuad(c->enableTAA ? c->farHistoryTex : c->farOutputTex);
    }
    doQuad(c->enableTAA ? c->historyTex : c->outputTex);

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void sg_FreeVolumetricCloudNode(SceneNode* node)
{
    if (!node) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;

    if (c->outputTex)     { glDeleteTextures(1, &c->outputTex);     c->outputTex     = 0; }
    if (c->historyTex)    { glDeleteTextures(1, &c->historyTex);    c->historyTex    = 0; }
    if (c->farOutputTex)  { glDeleteTextures(1, &c->farOutputTex);  c->farOutputTex  = 0; }
    if (c->farHistoryTex) { glDeleteTextures(1, &c->farHistoryTex); c->farHistoryTex = 0; }
    if (c->sphereSSBO)    { glDeleteBuffers(1, &c->sphereSSBO);     c->sphereSSBO    = 0; }
    if (c->nvdfTex)       { glDeleteTextures(1, &c->nvdfTex);       c->nvdfTex       = 0; }
    if (c->weatherMapTex) { glDeleteTextures(1, &c->weatherMapTex); c->weatherMapTex = 0; }
    if (c->noiseBaseTex)   { glDeleteTextures(1, &c->noiseBaseTex);   c->noiseBaseTex   = 0; }
    if (c->noiseDetailTex) { glDeleteTextures(1, &c->noiseDetailTex); c->noiseDetailTex = 0; }
    if (c->sphereData) { free(c->sphereData); c->sphereData = nullptr; }
    c->sphereCount = 0;
}

// ============================================================================
// Public: user-facing helper API
// ============================================================================

VolumetricCloudNodeData* vcCloud_GetData(SceneNode* node)
{
    return (node && node->type == ENTITY_VOLUMETRIC_CLOUD)
           ? &node->data.volumetricCloud : nullptr;
}

// Set the world-space extents of the cloud volume.
void vcCloud_SetBox(SceneNode* node, vec3 size)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    c->boxSize = size;
}

// Set sphere-grid generation parameters.
void vcCloud_SetGrid(SceneNode* node, int gx, int gz, float spacing, float scale)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    c->gridX        = gx;
    c->gridZ        = gz;
    c->gridSpacing  = spacing;
    c->gridScale    = scale;
}

// Adjust raymarching quality.
void vcCloud_SetRaymarching(SceneNode* node, float densityScale, int maxSteps, float stepSize)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    c->densityScale = densityScale;
    c->maxSteps     = maxSteps;
    c->stepSize     = stepSize;
}

// Adjust sun lighting.
void vcCloud_SetLighting(SceneNode* node, vec3 sunColor, float sunIntensity, float ambientStrength, float scatterG)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    c->sunColor        = sunColor;
    c->sunIntensity    = sunIntensity;
    c->ambientStrength = ambientStrength;
    c->scatterG        = scatterG;
}

// Set render resolution scale (0.25–1.0). Rebuilds the output texture.
void vcCloud_SetRenderScale(SceneNode* node, float scale)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    scale = scale < 0.1f ? 0.1f : (scale > 1.0f ? 1.0f : scale);
    extern int viewportWidth, viewportHeight;
    c->renderScale = scale;
    c->outputW = (int)(viewportWidth  * scale);
    c->outputH = (int)(viewportHeight * scale);
    vcCloud_CreateOutputTex(c);
}

// Shared-noise accessors — used by the NVDF generator so it doesn't have to
// regenerate the (slow, ~100ms) 64^3 + 32^3 noise textures. Lazily inits the
// shader+texture state on first call.
GLuint vcCloud_GetBaseNoiseTex()   { vcCloud_InitShaders(); return s_NoiseTexBase;   }
GLuint vcCloud_GetDetailNoiseTex() { vcCloud_InitShaders(); return s_NoiseTexDetail; }

// ---- NVDF (.nvdf file) load -----------------------------------------------
//
// File layout written by nvdf_generator.cpp:
//   char     magic[4] = "NVDF"
//   uint32_t version, width, height, depth, format (0=R8)
//   uint8_t  voxels[w*h*d]
bool vcCloud_LoadNVDF(SceneNode* node, const char* path)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c || !path || !path[0]) return false;

    FILE* f = fopen(path, "rb");
    if (!f) { LOG_E("vcCloud_LoadNVDF: cannot open %s", path); return false; }

    char     magic[4];
    uint32_t hdr[5];
    if (fread(magic, 1, 4, f) != 4 || fread(hdr, sizeof(uint32_t), 5, f) != 5
        || magic[0] != 'N' || magic[1] != 'V' || magic[2] != 'D' || magic[3] != 'F') {
        LOG_E("vcCloud_LoadNVDF: bad header %s", path);
        fclose(f); return false;
    }
    uint32_t w = hdr[1], h = hdr[2], d = hdr[3], fmt = hdr[4];
    if (fmt != 0u || w == 0 || h == 0 || d == 0) {
        LOG_E("vcCloud_LoadNVDF: unsupported format/size %s", path);
        fclose(f); return false;
    }

    size_t voxels = (size_t)w * h * d;
    unsigned char* buf = (unsigned char*)malloc(voxels);
    if (!buf) { LOG_E("vcCloud_LoadNVDF: OOM"); fclose(f); return false; }
    if (fread(buf, 1, voxels, f) != voxels) {
        LOG_E("vcCloud_LoadNVDF: short read %s", path);
        free(buf); fclose(f); return false;
    }
    fclose(f);

    if (c->nvdfTex) glDeleteTextures(1, &c->nvdfTex);
    glGenTextures(1, &c->nvdfTex);
    glBindTexture(GL_TEXTURE_3D, c->nvdfTex);
    // R8 textures are 1 byte/voxel. Without alignment=1 GL pads each X-row
    // to 4 bytes, garbling every row for widths not divisible by 4.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, w, h, d, 0, GL_RED, GL_UNSIGNED_BYTE, buf);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // S(X) and R(Z) are horizontal tile axes → REPEAT.
    // T(Y) is the cloud height axis, sampled via effH which is always clamped → CLAMP.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glBindTexture(GL_TEXTURE_3D, 0);
    free(buf);

    strncpy(c->nvdfPath, path, sizeof(c->nvdfPath) - 1);
    c->nvdfPath[sizeof(c->nvdfPath) - 1] = '\0';
    LOG_I("vcCloud_LoadNVDF: %s  %ux%ux%u  id=%u", path, w, h, d, c->nvdfTex);
    return true;
}

// ---- 2D Weather map load (uses stb_image, already included via engine.h) ---
bool vcCloud_LoadWeatherMap(SceneNode* node, const char* path)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c || !path || !path[0]) return false;

    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 3);  // force RGB
    if (!data) {
        LOG_E("vcCloud_LoadWeatherMap: cannot decode %s (%s)", path, stbi_failure_reason());
        return false;
    }

    if (c->weatherMapTex) glDeleteTextures(1, &c->weatherMapTex);
    glGenTextures(1, &c->weatherMapTex);
    glBindTexture(GL_TEXTURE_2D, c->weatherMapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    strncpy(c->weatherMapPath, path, sizeof(c->weatherMapPath) - 1);
    c->weatherMapPath[sizeof(c->weatherMapPath) - 1] = '\0';
    LOG_I("vcCloud_LoadWeatherMap: %s  %dx%d  id=%u", path, w, h, c->weatherMapTex);
    return true;
}

// Regenerate cloud spheres (call after changing grid params).
void vcCloud_RegenerateSpheres(SceneNode* node)
{
    if (!node || node->type != ENTITY_VOLUMETRIC_CLOUD) return;
    vcCloud_GenerateSpheres(node);
    vcCloud_UploadSpheres(&node->data.volumetricCloud);
}

// Save the current weather map texture to a PNG file.
void vcCloud_SaveWeatherMap(SceneNode* node, const char* path) {
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c || !c->weatherMapTex) { LOG_W("vcCloud: no weather map to save"); return; }

    const int W = 256, H = 256;
    unsigned char* pixels = (unsigned char*)malloc((size_t)W * H * 4);
    if (!pixels) { LOG_E("vcCloud: OOM saving weather map"); return; }

    glBindTexture(GL_TEXTURE_2D, c->weatherMapTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (stbi_write_png(path, W, H, 4, pixels, W * 4))
        LOG_I("vcCloud: weather map saved to %s", path);
    else
        LOG_E("vcCloud: failed to write %s", path);
    free(pixels);
}

// Rebuild the procedural weather map for a node (call after toggling autoWeatherMap
// or after changing box/sphere size significantly).
void vcCloud_RegenerateWeatherMap(SceneNode* node)
{
    if (!node || node->type != ENTITY_VOLUMETRIC_CLOUD) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;
    if (!c->autoWeatherMap) return;
    vcCloud_RebuildAutoWeatherMap(node);
}

// Toggle TAA. blend is 0.05 (smooth/ghosty) – 0.5 (sharp/noisy).
void vcCloud_SetTAA(SceneNode* node, bool enable, float blend)
{
    VolumetricCloudNodeData* c = vcCloud_GetData(node);
    if (!c) return;
    bool wasEnabled = c->enableTAA;
    c->enableTAA  = enable;
    c->taaBlend   = blend < 0.01f ? 0.01f : (blend > 1.0f ? 1.0f : blend);
    // Clear history when enabling from scratch so stale data doesn't bleed in
    if (enable && !wasEnabled && c->historyTex) {
        static const float zero[4] = {};
        glClearTexImage(c->historyTex, 0, GL_RGBA, GL_FLOAT, zero);
        c->frameIndex = 0;
    }
}
