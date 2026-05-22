#ifndef NVDF_EDITOR_H
#define NVDF_EDITOR_H

#include <GL/glew.h>
#include <vector>
#include <string>

struct NVDFProfile {
    std::string name = "New Profile";
    float cloudType = 0.5f;
    float coverage = 0.55f;
    float noiseScale = 2.0f;
    float detailScale = 8.0f;
    float densityScale = 1.0f;
    float erosion = 0.45f;
    float heightBias = 0.0f;
    float anvilBias = 0.0f;
    float curlyness = 0.0f;
};

class NVDFEditorV2 {
public:
    static NVDFEditorV2& Get();

    void Init();
    void DrawGUI();
    void Shutdown();

    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; }

    // Load an externally-owned 3D texture for preview (called from the
    // cloud attribute panel when the user clicks "Preview").
    // Does NOT take ownership — caller must keep the texture alive.
    void SetPreviewTex(GLuint tex3d);

private:
    NVDFEditorV2() = default;

    void BakeCurrentProfile();
    void ExportBC6H();
    void EnsurePreviewResources();
    void RefreshPreview();

    bool m_open = false;
    std::vector<NVDFProfile> m_profiles;
    int m_selectedProfile = 0;

    int m_resX = 256, m_resY = 64, m_resZ = 256;
    char m_exportPath[256] = "user/vclouds/mega_cloud.nvdf";

    GLuint m_genProg     = 0;
    GLuint m_previewProg = 0;
    GLuint m_nvdfTex     = 0;  // baked by this editor
    GLuint m_previewTex  = 0;  // 2D RGBA8 slice for ImGui::Image
    GLuint m_sourceTex   = 0;  // 3D tex currently being previewed

    // Dimensions of m_sourceTex (queried from GL on SetPreviewTex)
    int m_previewSrcW = 256, m_previewSrcH = 64, m_previewSrcD = 256;

    int   m_previewAxis  = 1;    // 0=X  1=Y(height)  2=Z
    float m_previewSlice = 0.5f; // normalised 0..1

    bool m_previewDirty = false;  // set to true when axis/slice changes
};

#endif // NVDF_EDITOR_H
