#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"

extern SceneNode* g_SceneRoot;

static const char* s_condLabels[] = {
    "Always",
    "Camera Y Above",
    "Camera Y Below",
    "Camera Near (dist)",
    "Camera Far (dist)",
};

static const char* s_nodeTypeTag(NodeType t) {
    switch (t) {
        case ENTITY_MODEL:            return "[M] ";
        case ENTITY_LIGHT:            return "[L] ";
        case ENTITY_CAMERA:           return "[C] ";
        case ENTITY_TERRAIN:          return "[T] ";
        case ENTITY_SKYBOX:           return "[S] ";
        case ENTITY_VOLUMETRIC_CLOUD: return "[VC]";
        case ENTITY_SKY_ATMOSPHERE:   return "[SA]";
        case ENTITY_INSTANCE:         return "[I] ";
        default:                      return "[ ] ";
    }
}

static void s_CollectAll(SceneNode* n, SceneNode** out, int* cnt, int cap) {
    if (!n || *cnt >= cap) return;
    out[(*cnt)++] = n;
    for (int i = 0; i < n->num_children; i++)
        s_CollectAll(n->children[i], out, cnt, cap);
}

static void s_CollectWithRules(SceneNode* n, SceneNode** out, int* cnt, int cap) {
    if (!n || *cnt >= cap) return;
    if (n->renderRule.enabled) out[(*cnt)++] = n;
    for (int i = 0; i < n->num_children; i++)
        s_CollectWithRules(n->children[i], out, cnt, cap);
}

void ShowDynamicRenderOrderPanel() {
    if (!ImGui::Begin("Dynamic Render Order")) { ImGui::End(); return; }

    ImGui::TextDisabled("Rules reorder siblings at runtime without changing scene structure.");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("+ Add Rule")) ImGui::OpenPopup("##RuleNodePicker");

    if (ImGui::BeginPopup("##RuleNodePicker")) {
        ImGui::Text("Choose entity to add rule to:");
        ImGui::Separator();
        static SceneNode* all[128];
        int allCnt = 0;
        if (g_SceneRoot) s_CollectAll(g_SceneRoot, all, &allCnt, 128);
        for (int i = 0; i < allCnt; i++) {
            SceneNode* nd = all[i];
            if (nd->renderRule.enabled) continue;
            char lbl[160];
            snprintf(lbl, sizeof(lbl), "%s %s", s_nodeTypeTag(nd->type), nd->name ? nd->name : "?");
            if (ImGui::Selectable(lbl)) {
                nd->renderRule.enabled     = true;
                nd->renderRule.condition   = RENDER_COND_ALWAYS;
                nd->renderRule.threshold   = 0.0f;
                nd->renderRule.targetIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();

    static SceneNode* ruleNodes[64];
    int ruleCnt = 0;
    if (g_SceneRoot) s_CollectWithRules(g_SceneRoot, ruleNodes, &ruleCnt, 64);

    if (ruleCnt == 0) {
        ImGui::TextDisabled("No rules yet.");
        ImGui::End();
        return;
    }

    for (int i = 0; i < ruleCnt; i++) {
        SceneNode*       nd = ruleNodes[i];
        RenderOrderRule& r  = nd->renderRule;

        ImGui::PushID(nd->ID);

        ImGui::TextColored(ImVec4(0.55f, 0.9f, 0.55f, 1.0f),
                           "%s %s", s_nodeTypeTag(nd->type), nd->name ? nd->name : "?");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 18.0f);
        if (ImGui::SmallButton("X")) {
            r = {};
            ImGui::PopID();
            continue;
        }

        // Condition
        ImGui::SetNextItemWidth(160.0f);
        int cond = (int)r.condition;
        if (ImGui::Combo("When##cond", &cond, s_condLabels, 5))
            r.condition = (RenderOrderCondition)cond;

        // Threshold (hidden for ALWAYS)
        if (r.condition != RENDER_COND_ALWAYS) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            bool isDist = (r.condition == RENDER_COND_CAMERA_NEAR ||
                           r.condition == RENDER_COND_CAMERA_FAR);
            ImGui::DragFloat(isDist ? "units##thr" : "Y##thr", &r.threshold, 1.0f);
        }

        // Target index
        ImGui::SetNextItemWidth(130.0f);
        ImGui::InputInt("Index (0=first, -1=last)##pos", &r.targetIndex);
        if (r.targetIndex < -1) r.targetIndex = -1;

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::End();
}
#endif
