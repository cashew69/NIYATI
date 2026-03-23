#include "engine/engine.h"
#include "model_controller.h"
#include <vector>
#include <string>

#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"
#endif

// Shader for our highlight/boundary
static ShaderProgram* outlineShader = NULL;
static vec3 outlineColor(1.0f, 0.5f, 0.0f); // Orange boundary

// Global list of models we can edit
struct RegisteredModel {
    std::string name;
    Mesh* meshes;
    int meshCount;
    vec3* pos;
    vec3* rot;
    float* scale;
};

static std::vector<RegisteredModel> g_Models;
static int g_SelectedIndex = -1;
static int g_SelectedSubMeshIndex = -1; // -1 means whole model

void ModelController_Init() {
    if (outlineShader) return;
    
    // Initialize the line shader program for the boundary
    const char* shaderFiles[5] = {
        "engine/shaders/lineVert.glsl",
        NULL, NULL, NULL,
        "engine/shaders/lineFrag.glsl"
    };

    if (!buildShaderProgramFromFiles(shaderFiles, 5, &outlineShader, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build outline shader in ModelController_Init\n");
    } else {
        outlineShader->name = "OutlineShader";
    }
}

void ModelController_Register(const char* name, Mesh* meshes, int count, vec3* pos, vec3* rot, float* sc) {
    RegisteredModel model;
    model.name = name;
    model.meshes = meshes;
    model.meshCount = count;
    model.pos = pos;
    model.rot = rot;
    model.scale = sc;
    g_Models.push_back(model);
}

void ModelController_UpdateUI() {
#ifdef HAS_IMGUI
    ImGui::Begin("Model Controller");
    
    if (g_Models.empty()) {
        ImGui::Text("No selectable models registered.");
    } else {
        // Dropdown to select model
        const char* currentLabel = (g_SelectedIndex >= 0) ? g_Models[g_SelectedIndex].name.c_str() : "Select Model...";
        if (ImGui::BeginCombo("Object", currentLabel)) {
            for (int i = 0; i < (int)g_Models.size(); i++) {
                bool isSelected = (i == g_SelectedIndex);
                if (ImGui::Selectable(g_Models[i].name.c_str(), isSelected)) {
                    g_SelectedIndex = i;
                    g_SelectedSubMeshIndex = -1; // Reset sub-mesh when changing model
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        
        ImGui::Separator();
        
        if (g_SelectedIndex >= 0 && g_SelectedIndex < (int)g_Models.size()) {
            RegisteredModel& target = g_Models[g_SelectedIndex];
            
            // Sub-mesh Selection Tree
            if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("Whole Model", g_SelectedSubMeshIndex == -1)) g_SelectedSubMeshIndex = -1;
                for (int m = 0; m < target.meshCount; ++m) {
                    char label[128];
                    sprintf(label, "[%d] %s", m, strlen(target.meshes[m].name) > 0 ? target.meshes[m].name : "Unnamed Mesh");
                    if (ImGui::Selectable(label, g_SelectedSubMeshIndex == m)) {
                        g_SelectedSubMeshIndex = m;
                    }
                }
            }

            ImGui::Separator();
            
            if (g_SelectedSubMeshIndex == -1) {
                // Whole Model Transform
                ImGui::Text("Global Transform (%s)", target.name.c_str());
                if (target.pos) ImGui::DragFloat3("Pos (m)", &(*target.pos)[0], 0.1f);
                if (target.rot) ImGui::DragFloat3("Rot (m)", &(*target.rot)[0], 1.0f);
                if (target.scale) ImGui::SliderFloat("Scale (m)", target.scale, 0.01f, 10.0f);
                
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Material Overrides", ImGuiTreeNodeFlags_DefaultOpen) && target.meshCount > 0) {
                    ImGui::Text("Overrides all submeshes");
                    static Material tempMat;
                    static bool firstRun = true;
                    if (firstRun) {
                        tempMat = target.meshes[0].material;
                        firstRun = false;
                    }
                    ImGui::ColorEdit3("Diffuse", tempMat.diffuseColor);
                    ImGui::SliderFloat("Shininess", &tempMat.shininess, 0.0f, 100.0f);
                    if (ImGui::Button("Apply to All Meshes")) {
                        for (int m = 0; m < target.meshCount; ++m) {
                            target.meshes[m].material.diffuseColor[0] = tempMat.diffuseColor[0];
                            target.meshes[m].material.diffuseColor[1] = tempMat.diffuseColor[1];
                            target.meshes[m].material.diffuseColor[2] = tempMat.diffuseColor[2];
                            target.meshes[m].material.shininess = tempMat.shininess;
                        }
                    }
                }
            } else {
                // Sub-mesh Transform
                Mesh* submesh = &target.meshes[g_SelectedSubMeshIndex];
                ImGui::Text("Sub-mesh Transform (%s)", submesh->name);
                if (submesh->transform) {
                     // Since Transform struct might be opaque, we'll expose its fields directly if we know them
                     // (they are vec3 position, vec3 rotation, vec3 scale in structs.h forward decal and platform code)
                     ImGui::DragFloat3("Pos (s)", &submesh->transform->position[0], 0.1f);
                     ImGui::DragFloat3("Rot (s)", &submesh->transform->rotation[0], 1.0f);
                     ImGui::DragFloat3("Scale (s)", &submesh->transform->scale[0], 0.1f);
                }
                
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Material Override", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::ColorEdit3("Diffuse", submesh->material.diffuseColor);
                    ImGui::SliderFloat("Shininess", &submesh->material.shininess, 0.0f, 100.0f);
                }
            }
            
            if (ImGui::Button("Reset View Selection")) {
                 g_SelectedSubMeshIndex = -1;
            }
            
            ImGui::Separator();
            ImGui::ColorEdit3("Outline Color", &outlineColor[0]);
        }
    }
    
    ImGui::End();
#endif
}

mat4 ModelController_GetTransform(const RegisteredModel& model) {
    mat4 m = mat4::identity();
    if (model.pos) m = m * vmath::translate(*model.pos);
    if (model.rot) {
        m = m * vmath::rotate((*model.rot)[0], vec3(1.0f, 0.0f, 0.0f));
        m = m * vmath::rotate((*model.rot)[1], vec3(0.0f, 1.0f, 0.0f));
        m = m * vmath::rotate((*model.rot)[2], vec3(0.0f, 0.0f, 1.0f));
    }
    if (model.scale) m = m * vmath::scale(*model.scale);
    
    return m;
}

void ModelController_RenderHighlight() {
    if (g_SelectedIndex < 0 || g_SelectedIndex >= (int)g_Models.size() || !outlineShader) return;
    
    RegisteredModel& target = g_Models[g_SelectedIndex];
    if (!target.meshes) return;
    
    mat4 modelRootMat = ModelController_GetTransform(target);
    
    glUseProgram(outlineShader->id);
    glUniformMatrix4fv(glGetUniformLocation(outlineShader->id, "projection"), 1, GL_FALSE, perspectiveProjectionMatrix);
    glUniformMatrix4fv(glGetUniformLocation(outlineShader->id, "view"), 1, GL_FALSE, viewMatrix);
    glUniform3fv(glGetUniformLocation(outlineShader->id, "lineColor"), 1, outlineColor);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glLineWidth(2.5f);
    
    for (int i = 0; i < target.meshCount; i++) {
        // If a specific submesh is selected, only highlight THAT one
        if (g_SelectedSubMeshIndex != -1 && g_SelectedSubMeshIndex != i) continue;

        Mesh* mesh = &target.meshes[i];
        
        // Build final matrix: ModelRoot * SubmeshTransform
        mat4 subMeshMat = mat4::identity();
        if (mesh->transform) {
             subMeshMat = vmath::translate(mesh->transform->position) * 
                          vmath::rotate(mesh->transform->rotation[0], vec3(1.0f, 0.0f, 0.0f)) *
                          vmath::rotate(mesh->transform->rotation[1], vec3(0.0f, 1.0f, 0.0f)) *
                          vmath::rotate(mesh->transform->rotation[2], vec3(0.0f, 0.0f, 1.0f)) *
                          vmath::scale(mesh->transform->scale);
        }
        
        mat4 finalMat = modelRootMat * subMeshMat;

        glUniformMatrix4fv(glGetUniformLocation(outlineShader->id, "model"), 1, GL_FALSE, finalMat);
        
        glBindVertexArray(mesh->vao);
        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glLineWidth(1.0f);
    glUseProgram(0);
}
