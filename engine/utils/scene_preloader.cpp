#include "scene_preloader.h"
#include "scenegraph.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void* preloader_ThreadFunc(void* arg) {
    ScenePreloader* p = (ScenePreloader*)arg;
    p->node = sg_ParseScene(p->path);
    __sync_synchronize();
    p->status = PRELOAD_PARSED;
    return NULL;
}

// Flatten the scene tree into a contiguous array for per-frame init.
static void collectNodes(SceneNode* node, SceneNode*** arr, int* count, int* cap) {
    if (!node) return;
    if (*count >= *cap) {
        *cap = (*cap == 0) ? 16 : (*cap * 2);
        *arr = (SceneNode**)realloc(*arr, *cap * sizeof(SceneNode*));
    }
    (*arr)[(*count)++] = node;
    for (int i = 0; i < node->num_children; i++)
        collectNodes(node->children[i], arr, count, cap);
}

void preloader_Init(ScenePreloader* p) {
    p->node            = NULL;
    p->status          = PRELOAD_IDLE;
    p->thread_active   = 0;
    p->path[0]         = '\0';
    p->initQueue       = NULL;
    p->initQueueCount  = 0;
    p->initQueueHead   = 0;
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
    // Parsing just finished — join thread and build the flat init queue.
    if (p->status == PRELOAD_PARSED) {
        if (p->thread_active) {
            pthread_join(p->thread, NULL);
            p->thread_active = 0;
        }
        if (!p->node) { p->status = PRELOAD_IDLE; return; }

        int cap = 0;
        collectNodes(p->node, &p->initQueue, &p->initQueueCount, &cap);
        p->initQueueHead = 0;
        p->status = PRELOAD_INITING;
    }

    // Drain one node per tick — each gets its GL resources in a separate frame.
    if (p->status == PRELOAD_INITING) {
        if (p->initQueueHead < p->initQueueCount) {
            sg_InitNodeSingle(p->initQueue[p->initQueueHead++]);
        }
        if (p->initQueueHead >= p->initQueueCount) {
            free(p->initQueue);
            p->initQueue      = NULL;
            p->initQueueCount = 0;
            p->initQueueHead  = 0;
            p->status         = PRELOAD_READY;
        }
    }
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
    if (p->initQueue) {
        free(p->initQueue);
        p->initQueue      = NULL;
        p->initQueueCount = 0;
        p->initQueueHead  = 0;
    }
    if (p->node) {
        sg_FreeNode(p->node);
        p->node = NULL;
    }
    p->status  = PRELOAD_IDLE;
    p->path[0] = '\0';
}
