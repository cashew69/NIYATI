#pragma once
#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"


// Structure to track objects we can edit in the GUI
struct EditableObject {
    const char* name;
    Mesh* meshes;
    int meshCount;
    
    // Pointers to the variables controlling transform in project.cpp
    // This allows the GUI to modify them directly.
    vec3* position;
    vec3* rotation; // Eulerian angles
    float* scale;
    
    // Internal state for the highlight
    bool isSelected;
};

// Initialize the controller (builds outline shaders, etc.)
void ModelController_Init();

// Register a model for editing
// If you pass pointers to your project's variables, the GUI will modify them.
void ModelController_Register(const char* name, Mesh* meshes, int count, vec3* pos, vec3* rot, float* sc);

// Update implementation for ImGui window
void ModelController_UpdateUI();

// Render highlight around selected object
// Call this at the end of your projectRender()
void ModelController_RenderHighlight();

// Get transform for a specific model (for highlight rendering)
// This is used internally to reconstruct the matrix from variables
mat4 ModelController_GetTransform(const EditableObject* obj);
