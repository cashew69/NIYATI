typedef struct Camera {
    vec3 position;
    vec3 target;
    vec3 up;
    mat4 viewMatrix;
} Camera;

Camera* createCamera(vec3 pos, vec3 target);
void updateCamera(Camera* cam);
