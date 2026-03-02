#ifndef _TRANSFORM_H_
#define _TRANSFORM_H_


using namespace vmath;

typedef struct Transform {
    vec3 position;
    vec3 rotation;
    vec3 scale;
    
    // Quaternion orientation (optional, used if useQuaternion is true)
    quaternion orientation;
    bool useQuaternion;
    
    // Cached matrices for performance
    mat4 localMatrix;
    bool isDirty;     // Flag to track if matrix needs recalculation
    
    // optional parent for hierarchical transforms
    struct Transform* parent;
} Transform;

Transform* createTransform(vec3 position = vec3(0.0f, 0.0f, 0.0f),
                          vec3 rotation = vec3(0.0f, 0.0f, 0.0f),
                          vec3 scale = vec3(1.0f, 1.0f, 1.0f));

// Quaternion setters/getters
void setRotation(Transform* transform, quaternion rotation);
quaternion getRotationQ(const Transform* transform);

void initTransform(Transform* transform);
void freeTransform(Transform* transform);

// Transform operations
void setPosition(Transform* transform, vec3 position);
void setRotation(Transform* transform, vec3 rotation);
void setScale(Transform* transform, vec3 scale);

void translate(Transform* transform, vec3 translation);
void rotate(Transform* transform, vec3 rotation);
void scaleBy(Transform* transform, vec3 scaleFactors);

// Rotate around
void rotateX(Transform* transform, float degrees);
void rotateY(Transform* transform, float degrees);
void rotateZ(Transform* transform, float degrees);

// transform getters
vec3 getPosition(const Transform* transform);
vec3 getRotation(const Transform* transform);
vec3 getScale(const Transform* transform);

// Matrix generation
mat4 getLocalMatrix(Transform* transform);
mat4 getWorldMatrix(Transform* transform);

// based on current rotation
vec3 getForward(const Transform* transform);
vec3 getRight(const Transform* transform);
vec3 getUp(const Transform* transform);

// Hierarchy operations
void setParent(Transform* transform, Transform* parent);
Transform* getParent(const Transform* transform);

// Utility functions
void lookAt(Transform* transform, vec3 target, vec3 up = vec3(0.0f, 1.0f, 0.0f));
void resetTransform(Transform* transform);

#endif
