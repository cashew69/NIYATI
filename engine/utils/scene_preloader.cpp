#include "scene_preloader.h"
#include "scenegraph.h"
#include <string.h>
#include <stdio.h>

static void* preloader_ThreadFunc(void* arg) {
    ScenePreloader* p = (ScenePreloader*)arg;
    p->node = sg_ParseScene(p->path);
    __sync_synchronize();         // full memory barrier before publishing status
    p->status = PRELOAD_PARSED;
    return NULL;
}

void preloader_Init(ScenePreloader* p) {
    p->node          = NULL;
    p->status        = PRELOAD_IDLE;
    p->thread_active = 0;
    p->path[0]       = '\0';
}

void preloader_Start(ScenePreloader* p, const char* path) {
    preloader_Destroy(p);

    strncpy(p->path, path, sizeof(p->path) - 1);
    p->path[sizeof(p->path) - 1] = '\0';
    p->node   = NULL;
    p->status = PRELOAD_LOADING;

    if (pthread_create(&p->thread, NULL, preloader_ThreadFunc, p) == 0) {
        p->thread_active = 1;
    } else {
        fprintf(stderr, "[preloader] pthread_create failed for: %s\n", path);
        p->status = PRELOAD_IDLE;
    }
}

void preloader_Tick(ScenePreloader* p) {
    if (p->status != PRELOAD_PARSED) return;

    if (p->node) {
        sg_InitNode(p->node);   // GL uploads — main thread only
    }

    if (p->thread_active) {
        pthread_join(p->thread, NULL);
        p->thread_active = 0;
    }

    p->status = p->node ? PRELOAD_READY : PRELOAD_IDLE;
}

int preloader_IsReady(const ScenePreloader* p) {
    return p->status == PRELOAD_READY && p->node != NULL;
}

struct SceneNode* preloader_Take(ScenePreloader* p) {
    if (!preloader_IsReady(p)) return NULL;
    struct SceneNode* node = p->node;
    p->node          = NULL;
    p->status        = PRELOAD_IDLE;
    p->path[0]       = '\0';
    return node;
}

void preloader_Destroy(ScenePreloader* p) {
    if (p->thread_active) {
        pthread_join(p->thread, NULL);
        p->thread_active = 0;
    }
    if (p->node) {
        sg_FreeNode(p->node);
        p->node = NULL;
    }
    p->status  = PRELOAD_IDLE;
    p->path[0] = '\0';
}
