#include "camera.h"
void updateCamera(Camera* camera) {
    if (!camera) return;
    
    camera->viewMatrix = vmath::lookat(
        camera->position,
        camera->target,
        camera->up
    );
}



Camera* createCamera(vec3 position, vec3 target, vec3 up) {
    Camera* camera = (Camera*)malloc(sizeof(Camera));
    if (!camera) return NULL;
    
    camera->position = position;
    camera->target = target;
    camera->up = up;
    
    updateCamera(camera);
    return camera;
}

void freeCamera(Camera* camera) {
    if (camera) free(camera);
}
