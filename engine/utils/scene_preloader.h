#pragma once
#include <pthread.h>

struct SceneNode;

typedef enum {
    PRELOAD_IDLE    = 0,
    PRELOAD_LOADING = 1,  // background thread: running sg_ParseScene
    PRELOAD_PARSED  = 2,  // background thread: done — call preloader_Tick() to run sg_InitNode
    PRELOAD_READY   = 3,  // sg_InitNode done — call preloader_Take() to swap
} PreloadStatus;

typedef struct {
    struct SceneNode* node;
    volatile int      status;
    pthread_t         thread;
    int               thread_active;
    char              path[512];
} ScenePreloader;

// Call once at startup before any other preloader function.
void preloader_Init(ScenePreloader* p);

// Kick off background parse of path. Safe to call while a scene is rendering.
void preloader_Start(ScenePreloader* p, const char* path);

// Call every frame from the main thread.
// When background parse finishes, runs sg_InitNode (GL uploads) and advances to PRELOAD_READY.
void preloader_Tick(ScenePreloader* p);

// Returns 1 when the scene is fully GPU-ready and can be swapped in.
int preloader_IsReady(const ScenePreloader* p);

// Transfers ownership of the loaded node and resets the preloader to IDLE.
// Returns NULL if not ready. Call sg_FreeNode(old g_SceneRoot) after taking.
struct SceneNode* preloader_Take(ScenePreloader* p);

// Joins the background thread and frees any partially-loaded node. Call at shutdown.
void preloader_Destroy(ScenePreloader* p);
