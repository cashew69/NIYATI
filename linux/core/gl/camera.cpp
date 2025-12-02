#include "camera.h"
void updateCamera(Camera* camera) {
    if (!camera) return;
    
    if (camera->useQuaternion) {
        
        mat4 rot = camera->orientation.asMatrix();
        // Transpose rotation for view
        mat4 viewRot = mat4::identity();
        for(int i=0; i<3; ++i)
            for(int j=0; j<3; ++j)
                viewRot[i][j] = rot[j][i];
                
        mat4 trans = vmath::translate(-camera->position[0], -camera->position[1], -camera->position[2]);
        
        camera->viewMatrix = viewRot * trans;
        
    } else {
        camera->viewMatrix = vmath::lookat(
            camera->position,
            camera->target,
            camera->up
        );
    }
}



Camera* createCamera(vec3 position, vec3 target, vec3 up) {
    Camera* camera = (Camera*)malloc(sizeof(Camera));
    if (!camera) return NULL;
    
    camera->position = position;
    camera->target = target;
    camera->up = up;
    camera->orientation = quaternion(0.0f, 0.0f, 0.0f, 1.0f); // Identity
    camera->useQuaternion = false;
    
    updateCamera(camera);
    return camera;
}

void freeCamera(Camera* camera) {
    if (camera) free(camera);
}
