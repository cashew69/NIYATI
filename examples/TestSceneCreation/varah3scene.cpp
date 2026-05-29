#include "varah3scene.h"

static SceneNode* v3_eCam    = nullptr;
static SceneNode* v3_sCam    = nullptr;
static SceneNode* v3_brahma  = nullptr;
static SceneNode* v3_pruthvi = nullptr;

// Phase 0: eCam   — Pruthvi rises Y: -2 -> 0  (1 unit/sec)
// Phase 1: brahma — hold 2 seconds
// Phase 2: sCam   — Pruthvi rises Y: 0 -> 200 (5 units/sec)
static int   v3_phase     = 0;
static float v3_phaseTime = 0.0f;

static const float RISE1_SPEED =   0.2f;
static const float BRAHMA_HOLD =   2.0f;
static const float RISE2_SPEED =   1.5f;
static const float MID_Y       =   0.0f;
static const float END_Y       = 200.0f;

void varah3SceneInit(void)
{
    // g_SceneRoot already swapped in by preloader
    v3_eCam    = sg_FindByName("eCam");
    v3_sCam    = sg_FindByName("sCam");
    v3_brahma  = sg_FindByName("brahma");
    v3_pruthvi = sg_FindByName("Pruthvi");

    if (!v3_eCam)    { printf("ERROR: eCam not found\n");    return; }
    if (!v3_sCam)    { printf("ERROR: sCam not found\n");    return; }
    if (!v3_brahma)  { printf("ERROR: brahma not found\n");  return; }
    if (!v3_pruthvi) { printf("ERROR: Pruthvi not found\n"); return; }

    v3_pruthvi->position[1] = -2.0f;
    v3_phase     = 0;
    v3_phaseTime = 0.0f;

    sg_SetActiveCamera(v3_eCam);
}

void varah3SceneDisplay(void)
{
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

void varah3SceneUpdate(void)
{
    if (!v3_pruthvi) return;

    v3_phaseTime += g_DeltaTime;

    if (v3_phase == 0) {
        v3_pruthvi->position[1] += RISE1_SPEED * g_DeltaTime;
        if (v3_pruthvi->position[1] >= MID_Y) {
            v3_pruthvi->position[1] = MID_Y;
            v3_phase     = 1;
            v3_phaseTime = 0.0f;
            sg_SetActiveCamera(v3_brahma);
        }
    } else if (v3_phase == 1) {
        if (v3_phaseTime >= BRAHMA_HOLD) {
            v3_phase     = 2;
            v3_phaseTime = 0.0f;
            sg_SetActiveCamera(v3_sCam);
        }
    } else if (v3_phase == 2) {
        if (v3_pruthvi->position[1] < END_Y) {
            v3_pruthvi->position[1] += RISE2_SPEED * g_DeltaTime;
            if (v3_pruthvi->position[1] > END_Y)
                v3_pruthvi->position[1] = END_Y;
        }
    }
}

void varah3SceneCleanup(void)
{
}
