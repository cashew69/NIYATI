#include "engine/utils/attrdesc.h"
#include "engine/dependancies/imgui/imgui.h"
#include "engine/utils/catmulromspline.h"

void ShowCatmullRomAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_CATMULLROMSPLINE) return;
    CatmullRomNodeData* data = &node->data.catmullrom;

    if (ImGui::CollapsingHeader("Catmull-Rom Spline Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        
        if (ImGui::DragFloat("Tension", &data->tension, 0.01f, 0.0f, 10.0f)) changed = true;
        if (ImGui::DragInt("Segments Per Curve", &data->segmentsPerCurve, 1.0f, 1, 100)) changed = true;
        if (ImGui::Checkbox("Looping", &data->isLooping)) changed = true;
        ImGui::Checkbox("Show Control Points", &data->showControlPoints);
        ImGui::ColorEdit3("Curve Color", (float*)&data->color);

        ImGui::Separator();
        ImGui::Text("Control Points (%d)", data->pointCount);
        
        if (ImGui::Button("Add Point")) {
            if (data->pointCount >= data->pointCapacity) {
                data->pointCapacity = data->pointCapacity == 0 ? 4 : data->pointCapacity * 2;
                data->controlPoints = (vec3*)realloc(data->controlPoints, data->pointCapacity * sizeof(vec3));
            }
            if (data->pointCount > 0) {
                vec3 last = data->controlPoints[data->pointCount - 1];
                data->controlPoints[data->pointCount] = last + vec3(1.0f, 0.0f, 0.0f);
            } else {
                data->controlPoints[data->pointCount] = vec3(0.0f, 0.0f, 0.0f);
            }
            data->pointCount++;
            changed = true;
        }

        for (int i = 0; i < data->pointCount; i++) {
            char label[32];
            sprintf(label, "Point %d", i);
            ImGui::PushID(i);
            if (ImGui::DragFloat3(label, (float*)&data->controlPoints[i], 0.1f)) {
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                for (int j = i; j < data->pointCount - 1; j++) {
                    data->controlPoints[j] = data->controlPoints[j+1];
                }
                data->pointCount--;
                i--;
                changed = true;
            }
            ImGui::PopID();
        }

        if (changed) {
            sg_UpdateCatmullRomCurve(node);
        }
    }
}
