#ifdef HAS_IMGUI
//#include "engine/dependancies/imgui/imgui.h"
//#include "engine/utils/camera_utils/camera_base.h"
#include <string>

// Forward decls
void AddStrategicCamera(const char* name);

void ShowEditorToolbar() {
    ImGui::Begin("UtilBar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Camera Selection Dropdown
    const char* base_modes[] = { "Editor: WASD", "Editor: Mouse" };
    const char* current_label = "Unknown";
    if (currentCameraMode == CAM_MODE_WASD_EULER) current_label = base_modes[0];
    else if (currentCameraMode == CAM_MODE_MOUSE_BOARD) current_label = base_modes[1];
    else if (currentCameraMode == CAM_MODE_STRATEGIC) {
         if (g_ActiveStrategicCameraIndex < g_StrategicCameraCount)
            current_label = g_StrategicCameras[g_ActiveStrategicCameraIndex].name;
         else
            current_label = "Strategic View";
    }

    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::BeginCombo("View Mode", current_label)) {
        if (ImGui::Selectable(base_modes[0], currentCameraMode == CAM_MODE_WASD_EULER)) currentCameraMode = CAM_MODE_WASD_EULER;
        if (ImGui::Selectable(base_modes[1], currentCameraMode == CAM_MODE_MOUSE_BOARD)) currentCameraMode = CAM_MODE_MOUSE_BOARD;
        
        ImGui::Separator();
        for (int i = 0; i < g_StrategicCameraCount; i++) {
            if (ImGui::Selectable(g_StrategicCameras[i].name, (currentCameraMode == CAM_MODE_STRATEGIC && g_ActiveStrategicCameraIndex == i))) {
                currentCameraMode = CAM_MODE_STRATEGIC;
                g_ActiveStrategicCameraIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    static bool show_settings = false;
    if (ImGui::SmallButton(show_settings ? "<< Close" : "Cam Settings >>")) {
        show_settings = !show_settings;
    }

    if (show_settings) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Speed", &camera_speed, 10.0f, 100.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Sens", &camera_sensitivity, 0.05f, 2.0f, "%.2f");
    }

    // Reset Flight Cam Button
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("R", ImVec2(24, 24))) {
        camera_pos = vec3(10.0f, 10.0f, 10.0f);
        camera_yaw = 0.0f;
        camera_pitch = 0.0f;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    vec3 cp = (currentCameraMode == CAM_MODE_STRATEGIC) ? g_StrategicCameras[g_ActiveStrategicCameraIndex].pos : camera_pos;
    ImGui::Text("Pos: %.1f, %.1f, %.1f", cp[0], cp[1], cp[2]);

    ImGui::End();
}

// Selection tracking
enum SelectedType {
    SEL_NONE,
    SEL_STRAT_CAM,
    SEL_FLIGHT_CAM
};
static SelectedType selectedType = SEL_NONE;
static int selectedIndex = -1;

void ShowEditorLayout() {
    ShowEditorToolbar();

    // 1. Scene Manager
    ImGui::Begin("Scene Manager");
    if (ImGui::Button("Add Strategic Camera")) {
        AddStrategicCamera("Camera");
    }
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Selectable("Flight Camera (Editor)", selectedType == SEL_FLIGHT_CAM)) {
            selectedType = SEL_FLIGHT_CAM;
        }
        for (int i = 0; i < g_StrategicCameraCount; i++) {
            if (ImGui::Selectable(g_StrategicCameras[i].name, (selectedType == SEL_STRAT_CAM && selectedIndex == i))) {
                selectedType = SEL_STRAT_CAM;
                selectedIndex = i;
                g_SelectedStrategicCameraIndex = i;
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();

    // 2. Attribute Manager
    ImGui::Begin("Attribute Manager");
    if (selectedType == SEL_STRAT_CAM && selectedIndex >= 0 && selectedIndex < g_StrategicCameraCount) {
        StrategicCamera* cam = &g_StrategicCameras[selectedIndex];
        ImGui::Text("Camera Settings: %s", cam->name);
        ImGui::InputText("Name", cam->name, 32);
        ImGui::Separator();

        if (ImGui::CollapsingHeader("LookAt Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position");
            ImGui::InputFloat3("##PosM", (float*)&cam->pos, "%.2f");
            ImGui::DragFloat3("##PosD", (float*)&cam->pos, 0.1f);
            
            ImGui::Spacing();
            ImGui::Text("Target");
            ImGui::InputFloat3("##TarM", (float*)&cam->target, "%.2f");
            ImGui::DragFloat3("##TarD", (float*)&cam->target, 0.1f);

            ImGui::Spacing();
            ImGui::Text("Up Vector");
            ImGui::InputFloat3("##UpM", (float*)&cam->up, "%.2f");
            ImGui::DragFloat3("##UpD", (float*)&cam->up, 0.05f);

            ImGui::Spacing();
            ImGui::Text("Roll Angle");
            ImGui::SliderFloat("##RollS", &cam->roll, -180.0f, 180.0f);
        }
    } else if (selectedType == SEL_FLIGHT_CAM) {
        ImGui::Text("Flight Camera Stats");
        ImGui::DragFloat3("Pos", (float*)&camera_pos, 0.1f);
        ImGui::DragFloat("Yaw", &camera_yaw, 0.5f);
        ImGui::DragFloat("Pitch", &camera_pitch, 0.5f);
    } else {
        ImGui::TextDisabled("Select an object to edit.");
    }
    ImGui::End();

    // 3. Filesystem (Stub)
    ImGui::Begin("Filesystem");
    ImGui::Text("Assets/");
    ImGui::End();
}
#endif
