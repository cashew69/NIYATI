#version 460 core

layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba8, binding = 0) uniform image3D uBaseNoise;
layout(rgba8, binding = 1) uniform image3D uDetailNoise;

uniform int uBaseRes;
uniform int uDetailRes;
uniform int uMode; // 0: Base, 1: Detail

// ---- Noise Helpers ---------------------------------------------------------

float hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    z = ((z % period) + period) % period;
    uint n = uint(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013);
    n ^= n >> 13;  n *= 0xb5297a4du;
    n ^= n >> 7;   n *= 0x68e31da4u;
    n ^= n >> 11;
    return float(n & 0x00ffffffu) / 16777216.0;
}

float valueNoise(vec3 p, int period) {
    ivec3 p0 = ivec3(floor(p));
    vec3 f = fract(p);
    f = f*f*(3.0 - 2.0*f);

    float h000 = hash(p0.x,     p0.y,     p0.z,     period, 0);
    float h100 = hash(p0.x + 1, p0.y,     p0.z,     period, 0);
    float h010 = hash(p0.x,     p0.y + 1, p0.z,     period, 0);
    float h110 = hash(p0.x + 1, p0.y + 1, p0.z,     period, 0);
    float h001 = hash(p0.x,     p0.y,     p0.z + 1, period, 0);
    float h101 = hash(p0.x + 1, p0.y,     p0.z + 1, period, 0);
    float h011 = hash(p0.x,     p0.y + 1, p0.z + 1, period, 0);
    float h111 = hash(p0.x + 1, p0.y + 1, p0.z + 1, period, 0);

    return mix(mix(mix(h000, h100, f.x), mix(h010, h110, f.x), f.y),
               mix(mix(h001, h101, f.x), mix(h011, h111, f.x), f.y), f.z);
}

float fbm(vec3 p, int baseFreq, int oct) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < oct; i++) {
        int f = baseFreq << i;
        v += a * valueNoise(p * float(f), f);
        a *= 0.5;
    }
    return v;
}

float worley(vec3 p, int freq) {
    vec3 fp = p * float(freq);
    ivec3 cp = ivec3(floor(fp));
    float md = 9.0;
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        ivec3 np = cp + ivec3(dx, dy, dz);
        vec3 cellPoint = vec3(np) + vec3(hash(np.x, np.y, np.z, freq, 0),
                                         hash(np.x, np.y, np.z, freq, 1),
                                         hash(np.x, np.y, np.z, freq, 2));
        float d = dot(fp - cellPoint, fp - cellPoint);
        if (d < md) md = d;
    }
    return 1.0 - sqrt(md) / 1.73205;
}

float perlinWorley(vec3 p, int freq) {
    float perlin = fbm(p, freq, 3) * 1.5;
    perlin = clamp(perlin, 0.0, 1.0);
    float invW = worley(p, freq);
    return invW + perlin * (1.0 - invW);
}

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID);
    
    if (uMode == 0) { // Base Noise
        if (id.x >= uBaseRes || id.y >= uBaseRes || id.z >= uBaseRes) return;
        vec3 uvw = (vec3(id) + 0.5) / float(uBaseRes);
        
        float pw  = perlinWorley(uvw, 4);
        float w4  = worley(uvw, 4);
        float w8  = worley(uvw, 8);
        float w16 = worley(uvw, 16);
        
        imageStore(uBaseNoise, id, vec4(pw, w4, w8, w16));
    } else { // Detail Noise
        if (id.x >= uDetailRes || id.y >= uDetailRes || id.z >= uDetailRes) return;
        vec3 uvw = (vec3(id) + 0.5) / float(uDetailRes);
        
        float w4  = worley(uvw, 4);
        float w8  = worley(uvw, 8);
        float w16 = worley(uvw, 16);
        
        imageStore(uDetailNoise, id, vec4(w4, w8, w16, 1.0));
    }
}
