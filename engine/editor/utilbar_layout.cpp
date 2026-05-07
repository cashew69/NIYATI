void ShowEditorToolbar() {
    ImGui::Begin("UtilBar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Camera Selection Dropdown — built by walking the scene graph each frame
    const char* current_label = (currentCameraMode == CAM_MODE_MOUSE_BOARD)
                                ? "Editor: Mouse"
                                : (g_ActiveCameraNode && g_ActiveCameraNode->name
                                   ? g_ActiveCameraNode->name : "Scene Camera");

    // Collect camera nodes from the scene graph
    SceneNode* camNodes[MAX_SCENE_CAMERAS];
    int        camCount = 0;
    struct { static void run(SceneNode* n, SceneNode** out, int* cnt) {
        if (!n) return;
        if (n->type == ENTITY_CAMERA && *cnt < MAX_SCENE_CAMERAS) out[(*cnt)++] = n;
        for (int i = 0; i < n->num_children; i++) run(n->children[i], out, cnt);
    }} collectCams;
    collectCams.run(g_SceneRoot, camNodes, &camCount);

    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::BeginCombo("View Mode", current_label)) {
        if (ImGui::Selectable("Editor: Mouse", currentCameraMode == CAM_MODE_MOUSE_BOARD))
            currentCameraMode = CAM_MODE_MOUSE_BOARD;

        ImGui::Separator();
        for (int i = 0; i < camCount; i++) {
            bool isActive = (currentCameraMode == CAM_MODE_CUSTOM && g_ActiveCameraNode == camNodes[i]);
            const char* lbl = camNodes[i]->name ? camNodes[i]->name : "Camera";
            if (ImGui::Selectable(lbl, isActive))
                sg_SetActiveCamera(camNodes[i]);
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


    // Projection Toggle
    ImGui::SameLine();
    extern bool bPerspective;
    extern float globalFOV;
    extern int viewportWidth, viewportHeight;
    if (ImGui::Button(bPerspective ? "Persp" : "Ortho", ImVec2(50, 24))) {
        bPerspective = !bPerspective;
        extern void resize(int, int);
        resize(viewportWidth, viewportHeight);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    if (ImGui::DragFloat("FOV", &globalFOV, 0.5f, 5.0f, 160.0f, "%.0f")) {
        // Sync back to current camera
        Camera* active = g_EditorCamera; // FOV sync: editor camera tracks globalFOV
        if (active) active->fov = globalFOV;

        extern void resize(int, int);
        resize(viewportWidth, viewportHeight);
    }


    // Reset Flight Cam Button
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("R", ImVec2(24, 24))) {
        if (g_EditorCamera) {
            g_EditorCamera->position = vec3(5.0f, 10.0f, 75.0f);
            camera_yaw = 0.0f;
            camera_pitch = 0.0f;
            camera_roll = 0.0f;
            g_EditorCamera->orientation = quaternion(1.0f, vec3(0,0,0));
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    vec3 cp = GetActiveCameraPosition();
    ImGui::Text("Pos: %.1f, %.1f, %.1f", cp[0], cp[1], cp[2]);

    // --- BVH / Frustum Culling / Bounding Box visualizer toggles --------
    ImGui::SameLine();
    ImGui::TextDisabled("|");

    ImGui::SameLine();
    ImVec4 cullCol = g_FrustumCullingEnabled ? ImVec4(0.15f, 0.55f, 0.15f, 1.0f)
                                             : ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, cullCol);
    if (ImGui::Button(g_FrustumCullingEnabled ? "Cull: ON" : "Cull: OFF", ImVec2(80, 24))) {
        g_FrustumCullingEnabled = !g_FrustumCullingEnabled;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle frustum culling (uses scene BVH)");

    ImGui::SameLine();
    ImVec4 bbCol = g_DrawBoundingBoxes ? ImVec4(0.15f, 0.45f, 0.55f, 1.0f)
                                       : ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, bbCol);
    if (ImGui::Button(g_DrawBoundingBoxes ? "BBox: ON" : "BBox: OFF", ImVec2(80, 24))) {
        g_DrawBoundingBoxes = !g_DrawBoundingBoxes;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle bounding-box visualizer for models + lights");
    
    ImGui::SameLine();
    ImVec4 vsyncCol = g_VSyncEnabled ? ImVec4(0.45f, 0.15f, 0.55f, 1.0f)
                                     : ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, vsyncCol);
    if (ImGui::Button(g_VSyncEnabled ? "VSync: ON" : "VSync: OFF", ImVec2(85, 24))) {
        g_VSyncEnabled = !g_VSyncEnabled;
        platformSetSwapInterval(g_VSyncEnabled ? 1 : 0);
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle Vertical Sync (Cap to monitor refresh rate vs Uncapped)");

    if (g_FrustumCullingEnabled) {
        ImGui::SameLine();
        ImGui::TextDisabled("Vis %d / Cull %d  Lights %d",
                            g_VisibleModelCount, g_CulledModelCount, g_VisibleLightCount);
    }

    ImGui::End();
}