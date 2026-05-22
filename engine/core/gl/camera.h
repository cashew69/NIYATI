#ifndef CORE_CAMERA_H
#define CORE_CAMERA_H

// vec3, mat4, quaternion come from the including translation unit (via engine.h / vmath.h)

typedef struct Camera {
    // View — used by both editor and scene cameras
    vec3  position;
    vec3  target;
    vec3  up;
    float roll;
    // Lens
    float fov;
    float near;
    float far;
    // Sorting
    bool  useDistanceSorting;
    // Free-look (editor camera only; scene cameras leave these at defaults)
    quaternion orientation;
    bool       useQuaternion;
    // Computed each frame
    mat4 viewMatrix;
    // Optional name for animation paths
    char path_name[64];
} Camera;

Camera* createCamera(vec3 pos, vec3 target, vec3 up);
void    updateCamera(Camera* cam);

#endif
