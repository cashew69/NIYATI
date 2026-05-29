#pragma once
#include <pthread.h>

struct SceneNode;

typedef enum {
    PRELOAD_IDLE    = 0,
    PRELOAD_LOADING = 1,  // background thread: running sg_ParseScene
    PRELOAD_PARSED  = 2,  // background thread: done — preloader_Tick will drain init queue
    PRELOAD_INITING = 3,  // main thread: draining init queue one node per tick
    PRELOAD_READY   = 4,  // all nodes initialised — call preloader_Take() to swap
} PreloadStatus;

typedef struct {
    struct SceneNode*  node;
    volatile int       status;
    pthread_t          thread;
    int                thread_active;
    char               path[512];

    // flat queue for per-frame init (populated once PRELOAD_PARSED is reached)
    struct SceneNode** initQueue;
    int                initQueueCount;
    int                initQueueHead;
} ScenePreloader;

// Call once at startup before any other preloader function.
void preloader_Init(ScenePreloader* p);

// Kick off background parse of path. Safe to call while a scene is rendering.
void preloader_Start(ScenePreloader* p, const char* path);

// Call every frame from the main thread.
// Drains the init queue one node per call so GL uploads are spread across frames.
void preloader_Tick(ScenePreloader* p);

// Returns 1 when the scene is fully GPU-ready and can be swapped in.
int preloader_IsReady(const ScenePreloader* p);

// Transfers ownership of the loaded node and resets the preloader to IDLE.
// Returns NULL if not ready. Call sg_FreeNode(old g_SceneRoot) after taking.
struct SceneNode* preloader_Take(ScenePreloader* p);

// Joins the background thread and frees any partially-loaded node. Call at shutdown.
void preloader_Destroy(ScenePreloader* p);
