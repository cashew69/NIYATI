#include "vcloud_noise.h"
#include <math.h>

// Internal hash function
static float vcv_hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    z = ((z % period) + period) % period;
    unsigned n = (unsigned)(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013);
    n ^= n >> 13;  n *= 0xb5297a4du;
    n ^= n >> 7;   n *= 0x68e31da4u;
    n ^= n >> 11;
    return (float)(n & 0x00ffffffu) / (float)0x01000000u;
}

// Internal trilinear value noise
static float vcv_valueNoise(float x, float y, float z, int period, int salt) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float u = x-x0, v = y-y0, w = z-z0;
    u = u*u*(3.0f-2.0f*u); v = v*v*(3.0f-2.0f*v); w = w*w*(3.0f-2.0f*w);
    auto h = [&](int a, int b, int c){ return vcv_hash(a,b,c,period,salt); };
    auto lp = [](float a, float b, float t){ return a+(b-a)*t; };
    return lp(lp(lp(h(x0,y0,z0),  h(x0+1,y0,z0),  u),
                  lp(h(x0,y0+1,z0),h(x0+1,y0+1,z0),u), v),
               lp(lp(h(x0,y0,z0+1),h(x0+1,y0,z0+1),u),
                  lp(h(x0,y0+1,z0+1),h(x0+1,y0+1,z0+1),u), v), w);
}

static float vcv_fbm(float x, float y, float z, int baseFreq, int oct, int salt) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < oct; i++) {
        int f = baseFreq << i;
        v += a * vcv_valueNoise(x*f, y*f, z*f, f, salt);
        a *= 0.5f;
    }
    return v;
}

float vcVCloud_Worley(float x, float y, float z, int freq) {
    float fx = x*freq, fy = y*freq, fz = z*freq;
    int cx = (int)floorf(fx), cy = (int)floorf(fy), cz = (int)floorf(fz);
    float md = 9.0f;
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        int nx = cx+dx, ny = cy+dy, nz = cz+dz;
        float px = (float)nx + vcv_hash(nx,ny,nz,freq,0);
        float py = (float)ny + vcv_hash(nx,ny,nz,freq,1);
        float pz = (float)nz + vcv_hash(nx,ny,nz,freq,2);
        float d  = (fx-px)*(fx-px)+(fy-py)*(fy-py)+(fz-pz)*(fz-pz);
        if (d < md) md = d;
    }
    return 1.0f - sqrtf(md) / 1.73205f;
}

float vcVCloud_Alligator(float x, float y, float z, int freq) {
    float fx = x*freq, fy = y*freq, fz = z*freq;
    int cx = (int)floorf(fx), cy = (int)floorf(fy), cz = (int)floorf(fz);
    float md1 = 9.0f;
    float md2 = 9.0f;

    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        int nx = cx+dx, ny = cy+dy, nz = cz+dz;
        float px = (float)nx + vcv_hash(nx,ny,nz,freq,0);
        float py = (float)ny + vcv_hash(nx,ny,nz,freq,1);
        float pz = (float)nz + vcv_hash(nx,ny,nz,freq,2);
        float d  = (fx-px)*(fx-px)+(fy-py)*(fy-py)+(fz-pz)*(fz-pz);
        
        if (d < md1) {
            md2 = md1;
            md1 = d;
        } else if (d < md2) {
            md2 = d;
        }
    }
    
    // Ridged Worley: d2 - d1
    float val = sqrtf(md2) - sqrtf(md1);
    // Normalize and saturate
    val = val / 1.73205f;
    return val > 1.0f ? 1.0f : (val < 0.0f ? 0.0f : val);
}

static vec3 vcv_potential(float x, float y, float z, float freq, int oct) {
    return vec3(
        vcv_fbm(x, y, z, (int)freq, oct, 10),
        vcv_fbm(x, y, z, (int)freq, oct, 20),
        vcv_fbm(x, y, z, (int)freq, oct, 30)
    );
}

vec3 vcVCloud_Curl(float x, float y, float z, float frequency, int octaves) {
    float eps = 0.01f;
    
    vec3 p = vec3(x, y, z);
    
    vec3 dx_plus  = vcv_potential(x + eps, y, z, frequency, octaves);
    vec3 dx_minus = vcv_potential(x - eps, y, z, frequency, octaves);
    vec3 dy_plus  = vcv_potential(x, y + eps, z, frequency, octaves);
    vec3 dy_minus = vcv_potential(x, y - eps, z, frequency, octaves);
    vec3 dz_plus  = vcv_potential(x, y, z + eps, frequency, octaves);
    vec3 dz_minus = vcv_potential(x, y, z - eps, frequency, octaves);
    
    vec3 dx = (dx_plus - dx_minus) / (2.0f * eps);
    vec3 dy = (dy_plus - dy_minus) / (2.0f * eps);
    vec3 dz = (dz_plus - dz_minus) / (2.0f * eps);
    
    return vec3(dy[2] - dz[1], dz[0] - dx[2], dx[1] - dy[0]);
}

float vcVCloud_CurlyAlligator(float x, float y, float z, int freq, float curlAmount) {
    if (curlAmount > 0.0f) {
        vec3 c = vcVCloud_Curl(x, y, z, (float)freq * 2.0f, 2);
        x += c[0] * curlAmount;
        y += c[1] * curlAmount;
        z += c[2] * curlAmount;
    }
    return vcVCloud_Alligator(x, y, z, freq);
}
