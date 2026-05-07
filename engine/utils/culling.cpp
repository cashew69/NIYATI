#include "culling.h"
#include <math.h>

// Globals 
bool g_FrustumCullingEnabled = false;
bool g_BVHEnabled            = true;

BVH         g_SceneBVH       = {};
cullfrustum g_RenderFrustum  = {};

int g_CulledModelCount  = 0;
int g_VisibleModelCount = 0;
int g_VisibleLightCount = 0;

void cull_SetFrustumCulling(bool enabled) { g_FrustumCullingEnabled = enabled; }
bool cull_IsFrustumCullingEnabled(void)   { return g_FrustumCullingEnabled;    }

//Frustum extraction

static void normalizePlane(Plane* p) {
    float l = sqrtf(p->normal[0]*p->normal[0] +
                    p->normal[1]*p->normal[1] +
                    p->normal[2]*p->normal[2]);
    if (l > 0.0f) {
        p->normal[0] /= l;
        p->normal[1] /= l;
        p->normal[2] /= l;
        p->distance  /= l;
    }
}

void cull_ExtractFrustum(cullfrustum* out, const mat4& vp) {
    if (!out) return;

    // Row vectors of VP (vmath is column-major: vp[col][row]).
    float r0x = vp[0][0], r0y = vp[1][0], r0z = vp[2][0], r0w = vp[3][0];
    float r1x = vp[0][1], r1y = vp[1][1], r1z = vp[2][1], r1w = vp[3][1];
    float r2x = vp[0][2], r2y = vp[1][2], r2z = vp[2][2], r2w = vp[3][2];
    float r3x = vp[0][3], r3y = vp[1][3], r3z = vp[2][3], r3w = vp[3][3];

    // LEFT  = row3 + row0
    out->planes[0].normal   = vec3(r3x + r0x, r3y + r0y, r3z + r0z);
    out->planes[0].distance = r3w + r0w;
    // RIGHT = row3 - row0
    out->planes[1].normal   = vec3(r3x - r0x, r3y - r0y, r3z - r0z);
    out->planes[1].distance = r3w - r0w;
    // BOTTOM= row3 + row1
    out->planes[2].normal   = vec3(r3x + r1x, r3y + r1y, r3z + r1z);
    out->planes[2].distance = r3w + r1w;
    // TOP   = row3 - row1
    out->planes[3].normal   = vec3(r3x - r1x, r3y - r1y, r3z - r1z);
    out->planes[3].distance = r3w - r1w;
    // NEAR  = row3 + row2
    out->planes[4].normal   = vec3(r3x + r2x, r3y + r2y, r3z + r2z);
    out->planes[4].distance = r3w + r2w;
    // FAR   = row3 - row2
    out->planes[5].normal   = vec3(r3x - r2x, r3y - r2y, r3z - r2z);
    out->planes[5].distance = r3w - r2w;

    for (int i = 0; i < 6; i++) normalizePlane(&out->planes[i]);
}

// Frustum vs AABB 

bool cull_TestAABB(const cullfrustum* f, AABB box) {
    if (!f) return true;
    if (bbox_IsEmpty(box)) return true;
    for (int i = 0; i < 6; i++) {
        const Plane& p = f->planes[i];
        vec3 pv;
        pv[0] = p.normal[0] >= 0.0f ? box.max[0] : box.min[0];
        pv[1] = p.normal[1] >= 0.0f ? box.max[1] : box.min[1];
        pv[2] = p.normal[2] >= 0.0f ? box.max[2] : box.min[2];
        float d = p.normal[0]*pv[0] + p.normal[1]*pv[1] + p.normal[2]*pv[2] + p.distance;
        if (d < 0.0f) return false;   // fully outside this plane
    }
    return true;
}

void cull_TransformFrustum(cullfrustum* out, const cullfrustum* in, const mat4& localToWorld) {
    if (!out || !in) return;
    
    // To transform a plane [n, d] from world to local space:
    // LocalPlane = WorldPlane * LocalToWorld
    
    for (int i = 0; i < 6; i++) {
        float nx = in->planes[i].normal[0];
        float ny = in->planes[i].normal[1];
        float nz = in->planes[i].normal[2];
        float d  = in->planes[i].distance;
        
        // n_local = row_nx * col_0 + row_ny * col_1 + row_nz * col_2 + row_d * col_3
        // Actually, Plane_L = [nx, ny, nz, d] * localToWorld
        out->planes[i].normal[0] = nx * localToWorld[0][0] + ny * localToWorld[0][1] + nz * localToWorld[0][2] + d * localToWorld[0][3];
        out->planes[i].normal[1] = nx * localToWorld[1][0] + ny * localToWorld[1][1] + nz * localToWorld[1][2] + d * localToWorld[1][3];
        out->planes[i].normal[2] = nx * localToWorld[2][0] + ny * localToWorld[2][1] + nz * localToWorld[2][2] + d * localToWorld[2][3];
        out->planes[i].distance  = nx * localToWorld[3][0] + ny * localToWorld[3][1] + nz * localToWorld[3][2] + d * localToWorld[3][3];

        // Re-normalize plane to keep distance meaningful
        normalizePlane(&out->planes[i]);
    }
}
