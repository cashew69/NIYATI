
// Scene Manager

#include "engine/dependancies/imgui/imfilebrowser.h"

// ---- Deferred-op state (safe to mutate outside tree traversal) ----
static SceneNode* s_NodeToDelete    = nullptr;
static SceneNode* s_NodeToDuplicate = nullptr;
static SceneNode* s_NodeToRename    = nullptr;
static char       s_RenameBuffer[128] = {};

// ---- Helpers -------------------------------------------------------

static int sg_ChildIndex(SceneNode* parent, SceneNode* child) {
    for (int i = 0; i < parent->num_children; i++)
        if (parent->children[i] == child) return i;
    return -1;
}

static void sg_SwapChildren(SceneNode* parent, int a, int b) {
    if (a < 0 || b < 0 || a >= parent->num_children || b >= parent->num_children) return;
    SceneNode* tmp      = parent->children[a];
    parent->children[a] = parent->children[b];
    parent->children[b] = tmp;
}

static SceneNode* sg_DuplicateShallow(SceneNode* src) {
    if (!src) return nullptr;
    SceneNode* copy      = sg_CreateNode(src->type, src->name);
    copy->position       = src->position;
    copy->rotation_euler = src->rotation_euler;
    copy->scale          = src->scale;
    memcpy(&copy->data, &src->data, sizeof(src->data));
    strncpy(copy->sourcePath, src->sourcePath, sizeof(copy->sourcePath));
    copy->meshIndex      = src->meshIndex;

    // Special handling for instances: reset GPU resources
    if (copy->type == ENTITY_INSTANCE) {
        copy->data.instance.instanceVBO = 0;
        copy->data.instance.instanceMatrices = nullptr;
        copy->data.instance.matricesCapacity = 0;
    }

    return copy;
}

static void DeleteSceneNode(SceneNode* node) {
    if (!node) return;
    if (g_SelectedSceneNode == node) {
        g_SelectedSceneNode = nullptr;
        g_SceneSelectedType = SEL_NONE;
    }
    if (node->parent)
        sg_RemoveChild(node->parent, node);
    else if (node == g_SceneRoot)
        g_SceneRoot = nullptr;
    sg_FreeNode(node);
}

// ---- Visual helpers ------------------------------------------------

static const char* NodeTypeTag(NodeType t) {
    switch (t) {
        case ENTITY_MODEL:    return "[M]";
        case ENTITY_LIGHT:    return "[L]";
        case ENTITY_CAMERA:   return "[C]";
        case ENTITY_INSTANCE: return "[I]";
        case ENTITY_TERRAIN:  return "[T]";
        case ENTITY_SKYBOX:   return "[S]";
        default:              return "[ ]";
    }
}

static ImVec4 NodeTypeColor(NodeType t) {
    switch (t) {
        case ENTITY_MODEL:    return ImVec4(0.55f, 0.85f, 1.00f, 1.0f);
        case ENTITY_LIGHT:    return ImVec4(1.00f, 0.90f, 0.30f, 1.0f);
        case ENTITY_CAMERA:   return ImVec4(0.40f, 1.00f, 0.60f, 1.0f);
        case ENTITY_INSTANCE: return ImVec4(0.80f, 0.60f, 1.00f, 1.0f);
        case ENTITY_TERRAIN:  return ImVec4(0.40f, 0.80f, 0.30f, 1.0f);
        case ENTITY_SKYBOX:   return ImVec4(0.70f, 0.40f, 0.80f, 1.0f);
        default:              return ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    }
}

// ---- Context menu --------------------------------------------------

static void DrawNodeContextMenu(SceneNode* node) {
    if (!ImGui::BeginPopupContextItem()) return;

    if (ImGui::MenuItem("Rename")) {
        s_NodeToRename = node;
        strncpy(s_RenameBuffer, node->name ? node->name : "", sizeof(s_RenameBuffer));
    }

    ImGui::Separator();

    SceneNode* parent = node->parent;
    if (parent) {
        int idx = sg_ChildIndex(parent, node);
        if (ImGui::MenuItem("Move Up",   nullptr, false, idx > 0))
            sg_SwapChildren(parent, idx, idx - 1);
        if (ImGui::MenuItem("Move Down", nullptr, false, idx < parent->num_children - 1))
            sg_SwapChildren(parent, idx, idx + 1);
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Duplicate"))
        s_NodeToDuplicate = node;

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.40f, 0.40f, 1.0f));
    if (ImGui::MenuItem("Delete"))
        s_NodeToDelete = node;
    ImGui::PopStyleColor();

    ImGui::EndPopup();
}

// ---- Recursive tree ------------------------------------------------

void DrawSceneNodeTree(SceneNode* node) {
    if (!node) return;

    if (s_NodeToRename == node) {
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::PushID(node->ID);
        bool commit    = ImGui::InputText("##rename", s_RenameBuffer, sizeof(s_RenameBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue |
                                          ImGuiInputTextFlags_AutoSelectAll);
        bool lostFocus = !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        ImGui::PopID();
        if (commit || lostFocus) {
            if (node->name) free((void*)node->name);
            node->name     = strdup(s_RenameBuffer[0] ? s_RenameBuffer : "Node");
            s_NodeToRename = nullptr;
        }
        return;
    }

    bool isLeaf     = (node->num_children == 0);
    bool isSelected = (g_SceneSelectedType == SEL_SCENENODE && g_SelectedSceneNode == node);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanAvailWidth
                             | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (isLeaf)     flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    if (!isSelected)
        ImGui::PushStyleColor(ImGuiCol_Text, NodeTypeColor(node->type));

    char label[144];
    snprintf(label, sizeof(label), "%s  %s##node%d",
             NodeTypeTag(node->type), node->name ? node->name : "Node", node->ID);

    bool nodeOpen = ImGui::TreeNodeEx(label, flags);

    if (!isSelected)
        ImGui::PopStyleColor();

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        g_SceneSelectedType = SEL_SCENENODE;
        g_SelectedSceneNode = node;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        s_NodeToRename = node;
        strncpy(s_RenameBuffer, node->name ? node->name : "", sizeof(s_RenameBuffer));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        ImGui::Text("ID: %d", node->ID);
        ImGui::Text("Pos:   %.2f  %.2f  %.2f", node->position[0],       node->position[1],       node->position[2]);
        ImGui::Text("Rot:   %.1f  %.1f  %.1f", node->rotation_euler[0], node->rotation_euler[1], node->rotation_euler[2]);
        ImGui::Text("Scale: %.2f  %.2f  %.2f", node->scale[0],          node->scale[1],          node->scale[2]);
        ImGui::EndTooltip();
    }

    DrawNodeContextMenu(node);

    if (!isLeaf && nodeOpen) {
        for (int i = 0; i < node->num_children; i++)
            DrawSceneNodeTree(node->children[i]);
        ImGui::TreePop();
    }
}

// ---- Main panel ----------------------------------------------------

void showSceneEditorUI()
{
    // Hoisted path/status statics so file browsers can write to them
    static char modelPath[256] = "assets/models/cube.obj";
    static char savePath[512]  = "scene.scene";
    static char loadPath[512]  = "scene.scene";
    static int  saveStatus     = 0;
    static int  loadStatus     = 0;

    // File browsers — one for each use-case
    static ImGui::FileBrowser modelBrowser(
        ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter);
    static ImGui::FileBrowser saveBrowser(
        ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc |
        ImGuiFileBrowserFlags_ConfirmOnEnter   | ImGuiFileBrowserFlags_CreateNewDir);
    static ImGui::FileBrowser loadBrowser(
        ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter);
    static bool s_BrowsersInited = false;
    if (!s_BrowsersInited) {
        modelBrowser.SetTitle("Select Model File");
        modelBrowser.SetTypeFilters({ ".obj", ".fbx", ".gltf", ".glb", ".3ds", ".dae" });
        saveBrowser.SetTitle("Save Scene");
        saveBrowser.SetTypeFilters({ ".scene" });
        loadBrowser.SetTitle("Load Scene");
        loadBrowser.SetTypeFilters({ ".scene" });
        s_BrowsersInited = true;
    }

    ImGui::Begin("Scene Manager");

    float avail = ImGui::GetContentRegionAvail().x;
    float sp    = ImGui::GetStyle().ItemSpacing.x;

    // ---- Top toolbar ----
    float btnW = (avail - sp * 3) / 4.0f;
    if (ImGui::Button("New", ImVec2(btnW, 0))) {
        if (g_SceneRoot) {
            SceneNode* old = g_SceneRoot;
            g_SceneRoot = nullptr; // clear first so sg_FreeNode's camera fix-up is a no-op
            sg_FreeNode(old);
        }
        g_SelectedSceneNode = nullptr;
        g_SceneSelectedType = SEL_NONE;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add", ImVec2(btnW, 0)))
        ImGui::OpenPopup("##AddNode");
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(btnW, 0)))
        ImGui::OpenPopup("Save Scene##dialog");
    ImGui::SameLine();
    if (ImGui::Button("Load", ImVec2(btnW, 0)))
        ImGui::OpenPopup("Load Scene##dialog");

    bool hasSelection = (g_SceneSelectedType == SEL_SCENENODE && g_SelectedSceneNode != nullptr);
    ImGui::BeginDisabled(!hasSelection);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.90f, 0.25f, 0.25f, 1.0f));
    if (ImGui::Button("Delete Selected", ImVec2(-1, 0)))
        s_NodeToDelete = g_SelectedSceneNode;
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();

    // ---- Add Node Popup ----
    if (ImGui::BeginPopup("##AddNode")) {
        ImGui::SeparatorText("Add to Scene");

        if (ImGui::Selectable("[C]  Camera")) {
            if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
            SceneNode* n = sg_AddCameraNode("Camera");
            if (n) {
                SceneNode* to = (hasSelection && g_SelectedSceneNode) ? g_SelectedSceneNode : g_SceneRoot;
                sg_AddChild(to, n);
                g_SceneSelectedType = SEL_SCENENODE;
                g_SelectedSceneNode = n;
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("[L]  Light")) {
            AddSceneLight("Light");
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("[I]  Instance")) {
            AddSceneInstance("Instance");
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("[T]  Terrain")) {
            if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
            SceneNode* n = sg_CreateNode(ENTITY_TERRAIN, "Terrain");
            SceneNode* to = (hasSelection && g_SelectedSceneNode) ? g_SelectedSceneNode : g_SceneRoot;
            sg_AddChild(to, n);
            sg_InitNode(n);
            g_SceneSelectedType = SEL_SCENENODE;
            g_SelectedSceneNode = n;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("[S]  Skybox")) {
            if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
            SceneNode* n = sg_CreateNode(ENTITY_SKYBOX, "Skybox");
            SceneNode* to = (hasSelection && g_SelectedSceneNode) ? g_SelectedSceneNode : g_SceneRoot;
            sg_AddChild(to, n);
            sg_InitNode(n);
            g_SceneSelectedType = SEL_SCENENODE;
            g_SelectedSceneNode = n;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("[ ]  Empty Node")) {
            if (!g_SceneRoot)
                g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
            SceneNode* n  = sg_CreateNode(ENTITY_EMPTY, "Node");
            SceneNode* to = (hasSelection && g_SelectedSceneNode) ? g_SelectedSceneNode : g_SceneRoot;
            sg_AddChild(to, n);
            g_SceneSelectedType = SEL_SCENENODE;
            g_SelectedSceneNode = n;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::Text("Model file:");

        // Path input + browse button on the same row
        float browseW = 28.0f;
        ImGui::SetNextItemWidth(220.0f - browseW - sp);
        ImGui::InputText("##modelpath", modelPath, sizeof(modelPath));
        ImGui::SameLine(0, sp);
        if (ImGui::Button("...##mdlBrowse", ImVec2(browseW, 0))) {
            modelBrowser.Open();
            ImGui::CloseCurrentPopup(); // close add-popup so browser can show
        }

        if (ImGui::Button("[M]  Add Model", ImVec2(-1, 0))) {
            CreateSceneModel("Model", modelPath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Save Modal ----
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Save Scene##dialog", NULL, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("File path:");
        float browseW = 28.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - sp);
        ImGui::InputText("##savepath", savePath, sizeof(savePath));
        ImGui::SameLine(0, sp);
        if (ImGui::Button("...##saveBrowse", ImVec2(browseW, 0))) {
            saveBrowser.Open();
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        float hw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;
        if (ImGui::Button("Save", ImVec2(hw, 28))) {
            saveStatus = (g_SceneRoot && sg_SaveScene(g_SceneRoot, savePath)) ? 1 : -1;
            if (saveStatus == 1) { ImGui::CloseCurrentPopup(); saveStatus = 0; }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##save", ImVec2(hw, 28))) { saveStatus = 0; ImGui::CloseCurrentPopup(); }
        if (saveStatus == -1)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Save failed — check path or scene is empty.");
        ImGui::EndPopup();
    }

    // ---- Load Modal ----
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Load Scene##dialog", NULL, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Current scene will be replaced.");
        ImGui::Text("File path:");
        float browseW = 28.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - sp);
        ImGui::InputText("##loadpath", loadPath, sizeof(loadPath));
        ImGui::SameLine(0, sp);
        if (ImGui::Button("...##loadBrowse", ImVec2(browseW, 0))) {
            loadBrowser.Open();
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        float hw = (ImGui::GetContentRegionAvail().x - sp) * 0.5f;
        if (ImGui::Button("Load", ImVec2(hw, 28))) {
            // Free old scene FIRST so g_CustomCameras[] is empty before
            // sg_LoadScene rebuilds it. Loading after freeing ensures camera
            // indices start from 0 and never collide with stale entries.
            if (g_SceneRoot) {
                SceneNode* old = g_SceneRoot;
                g_SceneRoot = nullptr;
                sg_FreeNode(old);
            }
            g_SelectedSceneNode = nullptr;
            g_SceneSelectedType = SEL_NONE;

            SceneNode* loaded = sg_LoadScene(loadPath);
            if (loaded) {
                g_SceneRoot = loaded;
                loadStatus  = 0;
                ImGui::CloseCurrentPopup();
            } else {
                // Load failed — restore an empty root so the editor isn't broken
                g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
                loadStatus  = -1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##load", ImVec2(hw, 28))) { loadStatus = 0; ImGui::CloseCurrentPopup(); }
        if (loadStatus == -1)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Load failed — file not found or invalid format.");
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ---- Scene Graph ----
    if (ImGui::CollapsingHeader("Scene Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        if (g_SceneRoot)
            DrawSceneNodeTree(g_SceneRoot);
        else
            ImGui::TextDisabled("  Empty — use '+ Add' to populate the scene.");
        ImGui::Spacing();
    }

    // ---- Legacy Models ----
    if (sceneModelCount > 0 && ImGui::CollapsingHeader("Models (Legacy)")) {
        ImGui::Spacing();
        for (int i = 0; i < sceneModelCount; i++) {
            bool sel = (g_SceneSelectedType == SEL_MODEL && g_SceneSelectedIndex == i);
            ImGui::PushStyleColor(ImGuiCol_Text, NodeTypeColor(ENTITY_MODEL));
            ImGui::TextUnformatted("[M]");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 6);
            char mdlLabel[80];
            snprintf(mdlLabel, sizeof(mdlLabel), "%s##mdl%d", sceneModels[i].name, i);
            if (ImGui::Selectable(mdlLabel, sel)) {
                g_SceneSelectedType  = SEL_MODEL;
                g_SceneSelectedIndex = i;
            }
        }
        ImGui::Spacing();
    }

    // ---- Deferred operations ----
    if (s_NodeToDelete) {
        DeleteSceneNode(s_NodeToDelete);
        s_NodeToDelete = nullptr;
    }
    if (s_NodeToDuplicate) {
        SceneNode* src    = s_NodeToDuplicate;
        s_NodeToDuplicate = nullptr;
        SceneNode* copy   = sg_DuplicateShallow(src);
        if (copy) {
            if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
            sg_AddChild(src->parent ? src->parent : g_SceneRoot, copy);
            g_SceneSelectedType = SEL_SCENENODE;
            g_SelectedSceneNode = copy;
        }
    }

    ImGui::End(); // Scene Manager window ends here

    // ---- File browser Display() — must be called outside any popup ----
    modelBrowser.Display();
    if (modelBrowser.HasSelected()) {
        auto path = modelBrowser.GetSelected().string();
        strncpy(modelPath, path.c_str(), sizeof(modelPath) - 1);
        modelPath[sizeof(modelPath) - 1] = '\0';
        modelBrowser.ClearSelected();
    }

    saveBrowser.Display();
    if (saveBrowser.HasSelected()) {
        auto path = saveBrowser.GetSelected().string();
        strncpy(savePath, path.c_str(), sizeof(savePath) - 1);
        savePath[sizeof(savePath) - 1] = '\0';
        saveBrowser.ClearSelected();
        // Immediately try to re-open the save modal so user can confirm
        ImGui::OpenPopup("Save Scene##dialog");
    }

    loadBrowser.Display();
    if (loadBrowser.HasSelected()) {
        auto path = loadBrowser.GetSelected().string();
        strncpy(loadPath, path.c_str(), sizeof(loadPath) - 1);
        loadPath[sizeof(loadPath) - 1] = '\0';
        loadBrowser.ClearSelected();
        // Re-open the load modal so user can confirm
        ImGui::OpenPopup("Load Scene##dialog");
    }
}
