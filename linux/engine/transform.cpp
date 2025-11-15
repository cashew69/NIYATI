#include "transform.h"
#include <stdlib.h>
#include <math.h>

// Helper function to mark transform as dirty
static void markDirty(Transform* transform) {
    if (transform) {
        transform->isDirty = true;
    }
}

// Create and initialize a new transform
Transform* createTransform(vec3 position, vec3 rotation, vec3 scale) {
    Transform* transform = (Transform*)malloc(sizeof(Transform));
    if (!transform) return NULL;
    
    transform->position = position;
    transform->rotation = rotation;
    transform->scale = scale;
    transform->localMatrix = mat4::identity();
    transform->isDirty = true;
    transform->parent = NULL;
    
    return transform;
}

// Initialize an existing transform structure
void initTransform(Transform* transform) {
    if (!transform) return;
    
    transform->position = vec3(0.0f, 0.0f, 0.0f);
    transform->rotation = vec3(0.0f, 0.0f, 0.0f);
    transform->scale = vec3(1.0f, 1.0f, 1.0f);
    transform->localMatrix = mat4::identity();
    transform->isDirty = true;
    transform->parent = NULL;
}

// Free a transform
void freeTransform(Transform* transform) {
    if (transform) {
        free(transform);
    }
}

// Set absolute position
void setPosition(Transform* transform, vec3 position) {
    if (!transform) return;
    transform->position = position;
    markDirty(transform);
}

// Set absolute rotation (Euler angles in degrees)
void setRotation(Transform* transform, vec3 rotation) {
    if (!transform) return;
    transform->rotation = rotation;
    markDirty(transform);
}

// Set absolute scale
void setScale(Transform* transform, vec3 scale) {
    if (!transform) return;
    transform->scale = scale;
    markDirty(transform);
}

// Translate by a delta
void translate(Transform* transform, vec3 translation) {
    if (!transform) return;
    transform->position = transform->position + translation;
    markDirty(transform);
}

// Rotate by delta angles (in degrees)
void rotate(Transform* transform, vec3 rotation) {
    if (!transform) return;
    transform->rotation = transform->rotation + rotation;
    markDirty(transform);
}

// Scale by factors
void scaleBy(Transform* transform, vec3 scaleFactors) {
    if (!transform) return;
    transform->scale[0] *= scaleFactors[0];
    transform->scale[1] *= scaleFactors[1];
    transform->scale[2] *= scaleFactors[2];
    markDirty(transform);
}

// Rotate around X axis
void rotateX(Transform* transform, float degrees) {
    if (!transform) return;
    transform->rotation[0] += degrees;
    markDirty(transform);
}

// Rotate around Y axis
void rotateY(Transform* transform, float degrees) {
    if (!transform) return;
    transform->rotation[1] += degrees;
    markDirty(transform);
}

// Rotate around Z axis
void rotateZ(Transform* transform, float degrees) {
    if (!transform) return;
    transform->rotation[2] += degrees;
    markDirty(transform);
}

// Get position
vec3 getPosition(const Transform* transform) {
    return transform ? transform->position : vec3(0.0f, 0.0f, 0.0f);
}

// Get rotation
vec3 getRotation(const Transform* transform) {
    return transform ? transform->rotation : vec3(0.0f, 0.0f, 0.0f);
}

// Get scale
vec3 getScale(const Transform* transform) {
    return transform ? transform->scale : vec3(1.0f, 1.0f, 1.0f);
}

// Calculate and cache the local transformation matrix
mat4 getLocalMatrix(Transform* transform) {
    if (!transform) return mat4::identity();
    
    // Recalculate only if dirty
    if (transform->isDirty) {
        // Build transformation matrix: T * R * S
        mat4 translationMatrix = vmath::translate(
            transform->position[0],
            transform->position[1],
            transform->position[2]
        );
        
        mat4 rotationMatrix = mat4::identity();
        
        // Apply rotations in order: Z * Y * X (standard rotation order)
        if (transform->rotation[2] != 0.0f) {
            rotationMatrix = vmath::rotate(transform->rotation[2], vec3(0.0f, 0.0f, 1.0f));
        }
        if (transform->rotation[1] != 0.0f) {
            rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[1], vec3(0.0f, 1.0f, 0.0f));
        }
        if (transform->rotation[0] != 0.0f) {
            rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[0], vec3(1.0f, 0.0f, 0.0f));
        }
        
        mat4 scaleMatrix = vmath::scale(
            transform->scale[0],
            transform->scale[1],
            transform->scale[2]
        );
        
        // Combine: Translation * Rotation * Scale
        transform->localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        transform->isDirty = false;
    }
    
    return transform->localMatrix;
}

// Get world matrix (includes parent transforms)
mat4 getWorldMatrix(Transform* transform) {
    if (!transform) return mat4::identity();
    
    mat4 localMatrix = getLocalMatrix(transform);
    
    // If there's a parent, multiply by parent's world matrix
    if (transform->parent) {
        return getWorldMatrix(transform->parent) * localMatrix;
    }
    
    return localMatrix;
}

// Get forward vector (based on rotation)
vec3 getForward(const Transform* transform) {
    if (!transform) return vec3(0.0f, 0.0f, -1.0f);
    
    // Calculate forward vector from rotation
    mat4 rotationMatrix = mat4::identity();
    rotationMatrix = vmath::rotate(transform->rotation[2], vec3(0.0f, 0.0f, 1.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[1], vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[0], vec3(1.0f, 0.0f, 0.0f));
    
    // Transform the default forward vector
    vec3 forward = vec3(
        rotationMatrix[0][2] * -1.0f,
        rotationMatrix[1][2] * -1.0f,
        rotationMatrix[2][2] * -1.0f
    );
    return forward;
}

// Get right vector (based on rotation)
vec3 getRight(const Transform* transform) {
    if (!transform) return vec3(1.0f, 0.0f, 0.0f);
    
    mat4 rotationMatrix = mat4::identity();
    rotationMatrix = vmath::rotate(transform->rotation[2], vec3(0.0f, 0.0f, 1.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[1], vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[0], vec3(1.0f, 0.0f, 0.0f));
    
    vec3 right = vec3(
        rotationMatrix[0][0],
        rotationMatrix[1][0],
        rotationMatrix[2][0]
    );
    return right;
}

// Get up vector (based on rotation)
vec3 getUp(const Transform* transform) {
    if (!transform) return vec3(0.0f, 1.0f, 0.0f);
    
    mat4 rotationMatrix = mat4::identity();
    rotationMatrix = vmath::rotate(transform->rotation[2], vec3(0.0f, 0.0f, 1.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[1], vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = rotationMatrix * vmath::rotate(transform->rotation[0], vec3(1.0f, 0.0f, 0.0f));
    
    vec3 up = vec3(
        rotationMatrix[0][1],
        rotationMatrix[1][1],
        rotationMatrix[2][1]
    );
    return up;
}

// Set parent transform
void setParent(Transform* transform, Transform* parent) {
    if (!transform) return;
    transform->parent = parent;
    markDirty(transform);
}

// Get parent transform
Transform* getParent(const Transform* transform) {
    return transform ? transform->parent : NULL;
}

// Make transform look at a target point
void lookAt(Transform* transform, vec3 target, vec3 up) {
    if (!transform) return;
    
    vec3 direction = target - transform->position;
    float length = sqrt(direction[0] * direction[0] + 
                       direction[1] * direction[1] + 
                       direction[2] * direction[2]);
    
    if (length < 0.0001f) return; // Avoid division by zero
    
    direction = direction / length; // Normalize
    
    // Calculate yaw (rotation around Y axis)
    float yaw = atan2(direction[0], direction[2]) * 180.0f / 3.14159265359f;
    
    // Calculate pitch (rotation around X axis)
    float pitch = asin(-direction[1]) * 180.0f / 3.14159265359f;
    
    transform->rotation = vec3(pitch, yaw, 0.0f);
    markDirty(transform);
}

// Reset transform to identity
void resetTransform(Transform* transform) {
    if (!transform) return;
    
    transform->position = vec3(0.0f, 0.0f, 0.0f);
    transform->rotation = vec3(0.0f, 0.0f, 0.0f);
    transform->scale = vec3(1.0f, 1.0f, 1.0f);
    markDirty(transform);
}
