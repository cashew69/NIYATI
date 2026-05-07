#pragma once
#include "engine/core/gl/structs.h"

// ============================================================================
// Attribute descriptor system
// One AttrDesc maps a struct field to its serialization key, UI behaviour,
// and byte offset — so the same table drives UI + save + load.
// ============================================================================

enum AttrType {
    ATTR_FLOAT,
    ATTR_VEC3,
    ATTR_COLOR3,
    ATTR_BOOL,    // native bool  (1 byte)
    ATTR_BOOL32,  // Bool = int   (4 bytes), shown as checkbox
    ATTR_INT,
    ATTR_STRING,  // null-terminated char buffer of size strSize
};

struct AttrDesc {
    const char* key;     // serialization key and UI label
    AttrType    type;
    int         offset;  // offsetof() into the data block passed as base
    float       speed;   // drag widget speed (numeric types)
    float       lo, hi;  // clamp range; both 0 = unclamped
    int         strSize; // ATTR_STRING only: buffer capacity (e.g. 256)
};

// Transform/Material descriptors — used by all node types and the model UI.
extern const AttrDesc g_TransformAttrs[];
extern const int      g_TransformAttrCount;
extern const AttrDesc g_MaterialAttrs[];
extern const int      g_MaterialAttrCount;

// ============================================================================
// Entity registry
// One EntityDesc per node type — drives type-name mapping AND data save/load.
// All entries live in entity_defs.cpp.
// ============================================================================

struct EntityDesc {
    NodeType        type;        // ENTITY_LIGHT etc.
    const char*     typeName;    // "LIGHT" — used in save header
    const char*     sectionKey;  // "Light"  — used as section block name; null = no data section
    const AttrDesc* attrs;       // null = no data section (e.g. EMPTY, MODEL)
    int             attrCount;
    size_t          dataOffset;  // offsetof(SceneNode, data.xxx) — base for attrs
};

extern const EntityDesc g_EntityTable[];
extern const int        g_EntityTableCount;

// Lookup helper — null if the type has no descriptor entry.
const EntityDesc* findEntityDesc(NodeType t);
