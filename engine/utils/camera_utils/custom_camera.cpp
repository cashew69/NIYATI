#include "camera_base.h"
#include "engine/utils/editor_utils.h"

void InitCustomCameras() {
    if (!g_EditorCamera) {
        g_EditorCamera = createCamera(vec3(10.0f, 10.0f, 10.0f),
                                      vec3(0.0f, 0.0f, 0.0f),
                                      vec3(0.0f, 1.0f, 0.0f));
        g_EditorCamera->useQuaternion = true;
    }
}

// 13 lines per camera: look-at, up-vector, 3-crosshair, 8-frustum
#define LINES_PER_CAM 13
#define FLOATS_PER_LINE 12

static float s_CamBatch[MAX_SCENE_CAMERAS * LINES_PER_CAM * FLOATS_PER_LINE];
static int   s_CamLineCount = 0;

static void pushLine(vec3 s, vec3 e, vec3 c) {
    float* v = s_CamBatch + s_CamLineCount * FLOATS_PER_LINE;
    v[0]=s[0]; v[1]=s[1]; v[2]=s[2]; v[3]=c[0]; v[4]=c[1]; v[5]=c[2];
    v[6]=e[0]; v[7]=e[1]; v[8]=e[2]; v[9]=c[0]; v[10]=c[1]; v[11]=c[2];
    s_CamLineCount++;
}

static void drawCameraGizmo(SceneNode* node) {
    Camera* cam = &node->data.camera;

    vec3 dir = cam->target - cam->position;
    // Skip degenerate camera (position == target)
    if (dir[0] == 0.0f && dir[1] == 0.0f && dir[2] == 0.0f) return;

    vec3 up  = (cam->up[0] == 0.0f && cam->up[1] == 0.0f && cam->up[2] == 0.0f)
               ? vec3(0.0f, 1.0f, 0.0f) : cam->up;
    bool sel = (g_SelectedSceneNode == node);
    vec3 col = sel ? vec3(1.0f, 1.0f, 1.0f) : vec3(0.7f, 0.7f, 0.7f);

    pushLine(cam->position, cam->target,                          vec3(1.0f, 1.0f, 0.0f));
    pushLine(cam->position, cam->position + up * 2.0f,           vec3(0.0f, 1.0f, 1.0f));

    float ts = 0.3f;
    pushLine(cam->target + vec3(-ts,0,0), cam->target + vec3(ts,0,0), col);
    pushLine(cam->target + vec3(0,-ts,0), cam->target + vec3(0,ts,0), col);
    pushLine(cam->target + vec3(0,0,-ts), cam->target + vec3(0,0,ts), col);

    float bs    = 0.3f;
    vec3  fwd   = normalize(dir);
    vec3  right = normalize(cross(fwd, up));
    vec3  up2   = cross(right, fwd);
    vec3  p1 = cam->position + (right - up2)  * bs;
    vec3  p2 = cam->position + (right + up2)  * bs;
    vec3  p3 = cam->position + (-right + up2) * bs;
    vec3  p4 = cam->position + (-right - up2) * bs;
    vec3  tip = cam->position + fwd * (bs * 2.5f);
    pushLine(p1, p2, col); pushLine(p2, p3, col);
    pushLine(p3, p4, col); pushLine(p4, p1, col);
    pushLine(p1, tip, col); pushLine(p2, tip, col);
    pushLine(p3, tip, col); pushLine(p4, tip, col);
}

static void walkGizmos(SceneNode* node) {
    if (!node) return;
    if (node->type == ENTITY_CAMERA) {
        // Only skip when actively looking through this camera
        bool lookingThrough = (currentCameraMode == CAM_MODE_CUSTOM &&
                               node == g_ActiveCameraNode);
        if (!lookingThrough)
            drawCameraGizmo(node);
    }
    for (int i = 0; i < node->num_children; i++)
        walkGizmos(node->children[i]);
}

void RenderCustomCameraHelpers(mat4 view, mat4 proj) {
    if (!g_SceneRoot) return;
    s_CamLineCount = 0;
    walkGizmos(g_SceneRoot);
    if (s_CamLineCount > 0)
        drawDebugLinesBatch(s_CamBatch, s_CamLineCount, view, proj);
}
