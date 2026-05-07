#include "camera.h"
void updateCamera(Camera* camera) {
    if (!camera) return;
    
    if (camera->useQuaternion) {
        
        mat4 orient = camera->orientation.asMatrix();
        
        // View matrix rotation is the transpose of the orientation matrix
        mat4 viewRot = mat4::identity();
        for(int i=0; i<3; ++i)
            for(int j=0; j<3; ++j)
                viewRot[i][j] = orient[j][i];
                
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

    camera->position     = position;
    camera->target       = target;
    camera->up           = up;
    camera->roll         = 0.0f;
    camera->fov          = 45.0f;
    camera->near         = 0.1f;
    camera->far          = 10000.0f;
    camera->orientation  = quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    camera->useQuaternion = false;
    camera->path_name[0] = '\0';

    updateCamera(camera);
    return camera;
}

void freeCamera(Camera* camera) {
    if (camera) free(camera);
}
