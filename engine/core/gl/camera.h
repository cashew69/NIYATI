typedef struct Camera {
    vec3 position;
    vec3 target;
    vec3 up;
    mat4 viewMatrix;
    
    // Quaternion orientation
    quaternion orientation;
    bool useQuaternion;
} Camera;

Camera* createCamera(vec3 pos, vec3 target, vec3 up);
void updateCamera(Camera* cam);
