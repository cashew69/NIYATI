#include "scenegraph.h"
#include "attrdesc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// SAVE / LOAD  —  brace-delimited KV format, fully driven by the entity table
// ============================================================================

// ---- Save helpers ----------------------------------------------------------

static void sg_Indent(FILE* f, int depth) {
    for (int i = 0; i < depth; i++) fprintf(f, "  ");
}

static void sg_SaveAttr(FILE* f, const AttrDesc& a, const void* base, int depth) {
    const void* ptr = (const char*)base + a.offset;
    sg_Indent(f, depth);
    switch (a.type) {
        case ATTR_FLOAT:
            fprintf(f, "%s  %g\n", a.key, *(const float*)ptr);
            break;
        case ATTR_VEC3:
        case ATTR_COLOR3: {
            const float* v = (const float*)ptr;
            fprintf(f, "%s  %g %g %g\n", a.key, v[0], v[1], v[2]);
            break;
        }
        case ATTR_BOOL:
            fprintf(f, "%s  %s\n", a.key, *(const bool*)ptr ? "true" : "false");
            break;
        case ATTR_BOOL32:
            fprintf(f, "%s  %s\n", a.key, *(const int*)ptr ? "true" : "false");
            break;
        case ATTR_INT:
            fprintf(f, "%s  %d\n", a.key, *(const int*)ptr);
            break;
        case ATTR_STRING: {
            const char* s = (const char*)ptr;
            fprintf(f, "%s  %s\n", a.key, s[0] ? s : "_");  // placeholder for empty strings
            break;
        }
    }
}

static void sg_SaveSection(FILE* f, const char* name,
                           const AttrDesc* attrs, int count,
                           const void* base, int depth) {
    sg_Indent(f, depth);
    fprintf(f, "%s {\n", name);
    for (int i = 0; i < count; i++)
        sg_SaveAttr(f, attrs[i], base, depth + 1);
    sg_Indent(f, depth);
    fprintf(f, "}\n");
}

static void sg_SaveNodeNew(FILE* f, SceneNode* node, int depth) {
    if (!node) return;

    const EntityDesc* desc = findEntityDesc(node->type);
    const char* typeStr = desc ? desc->typeName : "EMPTY";

    sg_Indent(f, depth);
    fprintf(f, "Node \"%s\" %s {\n", node->name ? node->name : "Unnamed", typeStr);

    sg_Indent(f, depth + 1); fprintf(f, "id  %d\n", node->ID);
    sg_Indent(f, depth + 1); fprintf(f, "terrainYOffset  %s\n", node->terrainYOffset ? "true" : "false");
    sg_Indent(f, depth + 1); fprintf(f, "selectedTerrainID  %d\n", node->selectedTerrainID);
    if (node->renderRule.enabled) {
        sg_Indent(f, depth + 1); fprintf(f, "RenderRule {\n");
        sg_Indent(f, depth + 2); fprintf(f, "condition    %d\n", (int)node->renderRule.condition);
        sg_Indent(f, depth + 2); fprintf(f, "threshold    %g\n", node->renderRule.threshold);
        sg_Indent(f, depth + 2); fprintf(f, "targetIndex  %d\n", node->renderRule.targetIndex);
        sg_Indent(f, depth + 1); fprintf(f, "}\n");
    }

    // Cameras keep their position/target inside the Camera section — skip generic transform.
    if (node->type != ENTITY_CAMERA) {
        for (int i = 0; i < g_TransformAttrCount; i++)
            sg_SaveAttr(f, g_TransformAttrs[i], node, depth + 1);
    }

    // Model has SceneNode-level fields + a nested Material section — special-cased.
    if (node->type == ENTITY_MODEL) {
        sg_Indent(f, depth + 1); fprintf(f, "sourcePath  %s\n", node->sourcePath[0] ? node->sourcePath : "_");
        sg_Indent(f, depth + 1); fprintf(f, "meshIndex   %d\n", node->meshIndex);
        fprintf(f, "\n");
        sg_SaveSection(f, "Material", g_MaterialAttrs, g_MaterialAttrCount,
                       &node->data.mesh.material, depth + 1);
    }
    // Everything else: write the registered section (if any).
    else if (desc && desc->attrs && desc->sectionKey) {
        fprintf(f, "\n");
        const void* base = (const char*)node + desc->dataOffset;
        sg_SaveSection(f, desc->sectionKey, desc->attrs, desc->attrCount, base, depth + 1);
        
        if (node->type == ENTITY_CATMULLROMSPLINE) {
            CatmullRomNodeData* cat = &node->data.catmullrom;
            sg_Indent(f, depth + 1); fprintf(f, "ControlPoints {\n");
            sg_Indent(f, depth + 2); fprintf(f, "pointCount  %d\n", cat->pointCount);
            for (int i = 0; i < cat->pointCount; i++) {
                sg_Indent(f, depth + 2); fprintf(f, "p%d  %g %g %g\n", i, cat->controlPoints[i][0], cat->controlPoints[i][1], cat->controlPoints[i][2]);
            }
            sg_Indent(f, depth + 1); fprintf(f, "}\n");
        }
    }

    for (int i = 0; i < node->num_children; i++) {
        fprintf(f, "\n");
        sg_SaveNodeNew(f, node->children[i], depth + 1);
    }

    sg_Indent(f, depth); fprintf(f, "}\n");
}

bool sg_SaveScene(SceneNode* root, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return false;
    sg_SaveNodeNew(f, root, 0);
    fclose(f);
    return true;
}

// ---- Load helpers ----------------------------------------------------------

struct SceneParser {
    const char* data;
    int pos;
    int len;

    void skipWs() {
        while (pos < len && isspace((unsigned char)data[pos])) pos++;
    }

    // Reads a word token (stops at whitespace, '{', '}'). Quoted strings strip the quotes.
    bool readToken(char* buf, int size) {
        skipWs();
        if (pos >= len) { if (size > 0) buf[0] = '\0'; return false; }

        if (data[pos] == '"') {
            pos++;
            int i = 0;
            while (pos < len && data[pos] != '"' && i < size - 1) buf[i++] = data[pos++];
            buf[i] = '\0';
            if (pos < len && data[pos] == '"') pos++;
            return true;
        }

        int i = 0;
        while (pos < len && !isspace((unsigned char)data[pos]) &&
               data[pos] != '{' && data[pos] != '}' && i < size - 1)
            buf[i++] = data[pos++];
        buf[i] = '\0';
        return i > 0;
    }

    bool peekToken(const char* expected) {
        int saved = pos;
        char buf[64];
        bool got = readToken(buf, sizeof(buf));
        pos = saved;
        return got && strcmp(buf, expected) == 0;
    }

    bool consumeChar(char c) {
        skipWs();
        if (pos < len && data[pos] == c) { pos++; return true; }
        return false;
    }

    bool peekChar(char c) {
        skipWs();
        return pos < len && data[pos] == c;
    }

    float readFloat() {
        skipWs();
        char* end;
        float f = strtof(data + pos, &end);
        if (end != data + pos) pos = (int)(end - data);
        return f;
    }

    bool readBool() {
        char tok[8];
        readToken(tok, sizeof(tok));
        return strcmp(tok, "true") == 0;
    }

    void skipToNextLine() {
        while (pos < len && data[pos] != '\n') pos++;
        if (pos < len) pos++;
    }
};

static bool sg_LoadAttr(SceneParser& p, const char* key,
                        const AttrDesc* attrs, int count, void* base) {
    for (int i = 0; i < count; i++) {
        if (strcmp(key, attrs[i].key) != 0) continue;
        void* ptr = (char*)base + attrs[i].offset;
        switch (attrs[i].type) {
            case ATTR_FLOAT:
                *(float*)ptr = p.readFloat();
                break;
            case ATTR_VEC3:
            case ATTR_COLOR3: {
                float* v = (float*)ptr;
                v[0] = p.readFloat(); v[1] = p.readFloat(); v[2] = p.readFloat();
                break;
            }
            case ATTR_BOOL:    *(bool*)ptr = p.readBool();              break;
            case ATTR_BOOL32:  *(int*)ptr  = p.readBool() ? 1 : 0;      break;
            case ATTR_INT:     *(int*)ptr  = (int)p.readFloat();        break;
            case ATTR_STRING: {
                char tok[512];
                if (p.readToken(tok, sizeof(tok))) {
                    char* dst = (char*)ptr;
                    int   cap = attrs[i].strSize > 0 ? attrs[i].strSize : 1;
                    if (strcmp(tok, "_") == 0) dst[0] = '\0';
                    else { strncpy(dst, tok, cap - 1); dst[cap - 1] = '\0'; }
                }
                break;
            }
        }
        return true;
    }
    return false;
}

static void sg_LoadSection(SceneParser& p, const AttrDesc* attrs, int count, void* base) {
    p.consumeChar('{');
    while (p.pos < p.len && !p.peekChar('}')) {
        char key[64];
        if (!p.readToken(key, sizeof(key))) break;
        if (!sg_LoadAttr(p, key, attrs, count, base))
            p.skipToNextLine();
    }
    p.consumeChar('}');
}

extern int g_NextNodeId;

// Map a TYPENAME ("LIGHT", "CAMERA", etc.) back to its NodeType via the table.
static NodeType lookupNodeType(const char* typeName) {
    for (int i = 0; i < g_EntityTableCount; i++)
        if (strcmp(typeName, g_EntityTable[i].typeName) == 0) return g_EntityTable[i].type;
    return ENTITY_EMPTY;
}

// Look up an entity table entry by its sectionKey ("Light", "Camera", etc.)
static const EntityDesc* findBySectionKey(const char* key) {
    for (int i = 0; i < g_EntityTableCount; i++) {
        const EntityDesc& d = g_EntityTable[i];
        if (d.sectionKey && strcmp(key, d.sectionKey) == 0) return &d;
    }
    return nullptr;
}

static SceneNode* sg_LoadNodeNew(SceneParser& p) {
    char tok[256];

    if (!p.readToken(tok, sizeof(tok))) return nullptr;
    char name[256];
    strncpy(name, tok, sizeof(name));

    if (!p.readToken(tok, 64)) return nullptr;
    NodeType type = lookupNodeType(tok);

    SceneNode* node = sg_CreateNode(type, name);
    if (!p.consumeChar('{')) { sg_FreeNode(node); return nullptr; }

    if (type == ENTITY_INSTANCE) {
        extern void instance_Init(InstanceData* inst);
        instance_Init(&node->data.instance);
    }

    while (p.pos < p.len && !p.peekChar('}')) {
        // Recurse into child nodes.
        if (p.peekToken("Node")) {
            p.readToken(tok, 8);
            SceneNode* child = sg_LoadNodeNew(p);
            if (child) sg_AddChild(node, child);
            continue;
        }

        if (!p.readToken(tok, sizeof(tok))) break;

        // SceneNode-level fields.
        if (strcmp(tok, "id") == 0) {
            int saved = (int)p.readFloat();
            node->ID = saved;
            if (saved >= g_NextNodeId) g_NextNodeId = saved + 1;
            continue;
        }
        if (strcmp(tok, "terrainYOffset")    == 0) { node->terrainYOffset    = p.readBool();      continue; }
        if (strcmp(tok, "selectedTerrainID") == 0) { node->selectedTerrainID = (int)p.readFloat(); continue; }

        // Transform attrs (inline, applies to all nodes).
        if (sg_LoadAttr(p, tok, g_TransformAttrs, g_TransformAttrCount, node)) continue;

        // Model-only inline fields (sourcePath, meshIndex live on SceneNode itself).
        if (strcmp(tok, "sourcePath") == 0) {
            p.readToken(node->sourcePath, sizeof(node->sourcePath));
            if (strcmp(node->sourcePath, "_") == 0) node->sourcePath[0] = '\0';
            continue;
        }
        if (strcmp(tok, "meshIndex") == 0) { node->meshIndex = (int)p.readFloat(); continue; }

        // Instance-only inline fields (backward compatibility with old scene format).
        if (node->type == ENTITY_INSTANCE) {
            const EntityDesc* inst_desc = findEntityDesc(ENTITY_INSTANCE);
            if (inst_desc && sg_LoadAttr(p, tok, inst_desc->attrs, inst_desc->attrCount, &node->data.instance)) continue;
        }

        // Material section (sub-section of MODEL).
        if (strcmp(tok, "Material") == 0) {
            sg_LoadSection(p, g_MaterialAttrs, g_MaterialAttrCount, &node->data.mesh.material);
            continue;
        }

        if (strcmp(tok, "RenderRule") == 0) {
            node->renderRule.enabled = true;
            p.consumeChar('{');
            while (p.pos < p.len && !p.peekChar('}')) {
                char k[64];
                if (!p.readToken(k, sizeof(k))) break;
                if      (strcmp(k, "condition")   == 0) node->renderRule.condition   = (RenderOrderCondition)(int)p.readFloat();
                else if (strcmp(k, "threshold")   == 0) node->renderRule.threshold   = p.readFloat();
                else if (strcmp(k, "targetIndex") == 0) node->renderRule.targetIndex = (int)p.readFloat();
                else p.skipToNextLine();
            }
            p.consumeChar('}');
            continue;
        }

        // Any other section name maps to an entity in the registry.
        const EntityDesc* d = findBySectionKey(tok);
        if (d && d->attrs) {
            void* base = (char*)node + d->dataOffset;
            sg_LoadSection(p, d->attrs, d->attrCount, base);
            continue;
        }

        if (strcmp(tok, "ControlPoints") == 0) {
            if (node->type == ENTITY_CATMULLROMSPLINE) {
                CatmullRomNodeData* cat = &node->data.catmullrom;
                p.consumeChar('{');
                while (p.pos < p.len && !p.peekChar('}')) {
                    char k[64];
                    if (!p.readToken(k, sizeof(k))) break;
                    if (strcmp(k, "pointCount") == 0) {
                        cat->pointCount = (int)p.readFloat();
                        cat->pointCapacity = cat->pointCount < 4 ? 4 : cat->pointCount;
                        if (cat->controlPoints) free(cat->controlPoints);
                        cat->controlPoints = (vec3*)calloc(cat->pointCapacity, sizeof(vec3));
                    } else if (k[0] == 'p') {
                        int idx = atoi(k + 1);
                        if (idx >= 0 && idx < cat->pointCount && cat->controlPoints) {
                            cat->controlPoints[idx][0] = p.readFloat();
                            cat->controlPoints[idx][1] = p.readFloat();
                            cat->controlPoints[idx][2] = p.readFloat();
                        } else {
                            p.readFloat(); p.readFloat(); p.readFloat();
                        }
                    } else {
                        p.skipToNextLine();
                    }
                }
                p.consumeChar('}');
            } else {
                p.consumeChar('{');
                while (p.pos < p.len && !p.peekChar('}')) p.skipToNextLine();
                p.consumeChar('}');
            }
            continue;
        }

        // Unknown key — skip to next line.
        p.skipToNextLine();
    }

    p.consumeChar('}');
    return node;
}

// Parses the scene file into a node tree — no GL calls, safe to run on a background thread.
// Caller must call sg_InitNode() on the main thread before using the result for rendering.
SceneNode* sg_ParseScene(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = (char*)malloc(size + 1);
    if (!data) { fclose(f); return nullptr; }
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);

    SceneParser p;
    p.data = data;
    p.pos  = 0;
    p.len  = (int)size;

    char tok[8];
    if (!p.readToken(tok, sizeof(tok)) || strcmp(tok, "Node") != 0) {
        free(data);
        return nullptr;
    }

    SceneNode* root = sg_LoadNodeNew(p);
    free(data);
    return root;
}

// Convenience: parse + GPU init in one call (main thread only).
SceneNode* sg_LoadScene(const char* filename) {
    SceneNode* root = sg_ParseScene(filename);
    sg_InitNode(root);
    return root;
}
