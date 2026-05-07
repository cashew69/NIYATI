#include "attrdesc.h"
#include "engine/core/gl/structs.h"

// ============================================================================
// Transform — applies to ALL node types, base = SceneNode*
// ============================================================================
const AttrDesc g_TransformAttrs[] = {
    { "position", ATTR_VEC3, (int)offsetof(SceneNode, position),       0.1f,  0,      0,    0 },
    { "rotation", ATTR_VEC3, (int)offsetof(SceneNode, rotation_euler), 0.5f,  0,      0,    0 },
    { "scale",    ATTR_VEC3, (int)offsetof(SceneNode, scale),          0.1f,  0.001f, 1000, 0 },
};
const int g_TransformAttrCount = sizeof(g_TransformAttrs) / sizeof(g_TransformAttrs[0]);

// ============================================================================
// Material — sub-section of MODEL, base = Material*
// ============================================================================
const AttrDesc g_MaterialAttrs[] = {
    { "useDiffuseTexture",  ATTR_BOOL32, (int)offsetof(Material, useDiffuseTexture),             0,     0, 0, 0 },
    { "useNormalTexture",   ATTR_BOOL32, (int)offsetof(Material, useNormalTexture),              0,     0, 0, 0 },
    { "useORMTexture",      ATTR_BOOL32, (int)offsetof(Material, useMetallicRoughnessTexture),   0,     0, 0, 0 },
    { "useAOTexture",       ATTR_BOOL32, (int)offsetof(Material, useAOTexture),                  0,     0, 0, 0 },
    { "useEmissiveTexture", ATTR_BOOL32, (int)offsetof(Material, useEmissiveTexture),            0,     0, 0, 0 },
    { "roughness",          ATTR_FLOAT,  (int)offsetof(Material, roughness),                     0.01f, 0, 1, 0 },
    { "metalness",          ATTR_FLOAT,  (int)offsetof(Material, metalness),                     0.01f, 0, 1, 0 },
};
const int g_MaterialAttrCount = sizeof(g_MaterialAttrs) / sizeof(g_MaterialAttrs[0]);
