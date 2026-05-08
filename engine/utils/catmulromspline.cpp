#include "catmulromspline.h"
#include "engine/dependancies/vmath.h"
#include <stdlib.h>
#include <string.h>

void sg_InitCatmullRomNode(SceneNode* node) {
    if (!node || node->type != ENTITY_CATMULLROMSPLINE) return;

    CatmullRomNodeData* data = &node->data.catmullrom;
    if (data->controlPoints == nullptr) {
        data->pointCapacity = 4;
        data->pointCount = 4;
        data->controlPoints = (vec3*)calloc(data->pointCapacity, sizeof(vec3));
        
        // Initial control points
        data->controlPoints[0] = vec3(-5.0f, 0.0f, 0.0f);
        data->controlPoints[1] = vec3(-2.0f, 2.0f, 0.0f);
        data->controlPoints[2] = vec3( 2.0f, -2.0f, 0.0f);
        data->controlPoints[3] = vec3( 5.0f, 0.0f, 0.0f);

        data->tension = 0.5f;
        data->segmentsPerCurve = 20;
        data->isLooping = false;
        data->showControlPoints = true;
        data->color = vec3(1.0f, 1.0f, 0.0f); // Default to yellow
        
        data->curvePoints = nullptr;
        data->curvePointCount = 0;
        data->curvePointCapacity = 0;
    }

    sg_UpdateCatmullRomCurve(node);
}

void sg_FreeCatmullRomNode(SceneNode* node) {
    if (!node || node->type != ENTITY_CATMULLROMSPLINE) return;
    CatmullRomNodeData* data = &node->data.catmullrom;
    if (data->controlPoints) {
        free(data->controlPoints);
        data->controlPoints = nullptr;
    }
    if (data->curvePoints) {
        free(data->curvePoints);
        data->curvePoints = nullptr;
    }
    data->pointCount = 0;
    data->curvePointCount = 0;
}

static float GetCentripetalT(float t, const vec3& p0, const vec3& p1) {
    float d = vmath::length(p1 - p0);
    return t + powf(d, 0.5f); // alpha = 0.5 for centripetal
}

vec3 sg_EvaluateCatmullRom(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t, float tension) {
    // Centripetal Catmull-Rom using Barry-Goldman algorithm
    // (tension parameter is typically ignored as the distance natively manages curvature)
    
    float t0 = 0.0f;
    float t1 = GetCentripetalT(t0, p0, p1);
    float t2 = GetCentripetalT(t1, p1, p2);
    float t3 = GetCentripetalT(t2, p2, p3);

    // Prevent division by zero for overlapping control points
    if (t1 == t0) t1 = t0 + 0.0001f;
    if (t2 == t1) t2 = t1 + 0.0001f;
    if (t3 == t2) t3 = t2 + 0.0001f;

    float t_eval = t1 + t * (t2 - t1);

    vec3 a1 = p0 * ((t1 - t_eval) / (t1 - t0)) + p1 * ((t_eval - t0) / (t1 - t0));
    vec3 a2 = p1 * ((t2 - t_eval) / (t2 - t1)) + p2 * ((t_eval - t1) / (t2 - t1));
    vec3 a3 = p2 * ((t3 - t_eval) / (t3 - t2)) + p3 * ((t_eval - t2) / (t3 - t2));

    vec3 b1 = a1 * ((t2 - t_eval) / (t2 - t0)) + a2 * ((t_eval - t0) / (t2 - t0));
    vec3 b2 = a2 * ((t3 - t_eval) / (t3 - t1)) + a3 * ((t_eval - t1) / (t3 - t1));

    vec3 c = b1 * ((t2 - t_eval) / (t2 - t1)) + b2 * ((t_eval - t1) / (t2 - t1));

    return c;
}

void sg_UpdateCatmullRomCurve(SceneNode* node) {
    if (!node || node->type != ENTITY_CATMULLROMSPLINE) return;

    CatmullRomNodeData* data = &node->data.catmullrom;
    if (data->pointCount < 4) return;

    int numSegments = data->isLooping ? data->pointCount : (data->pointCount - 3);
    if (numSegments < 1) return;

    // 1. Calculate average segment length to distribute points dynamically
    float totalDist = 0.0f;
    float* segDistances = (float*)malloc(numSegments * sizeof(float));
    
    for (int i = 0; i < numSegments; i++) {
        int index1 = (i + 1) % data->pointCount;
        int index2 = (i + 2) % data->pointCount;
        float d = vmath::length(data->controlPoints[index2] - data->controlPoints[index1]);
        segDistances[i] = d;
        totalDist += d;
    }
    
    float avgDist = totalDist / (float)numSegments;
    if (avgDist < 0.0001f) avgDist = 1.0f;

    // 2. Determine steps for each segment proportionally
    int* segSteps = (int*)malloc(numSegments * sizeof(int));
    int totalSteps = 0;
    for (int i = 0; i < numSegments; i++) {
        int steps = (int)roundf(data->segmentsPerCurve * (segDistances[i] / avgDist));
        if (steps < 2) steps = 2; // Need at least 2 points
        segSteps[i] = steps;
        totalSteps += steps;
    }

    int requiredPoints = totalSteps + (data->isLooping ? 0 : 1);
    
    if (requiredPoints > data->curvePointCapacity) {
        data->curvePointCapacity = requiredPoints;
        data->curvePoints = (vec3*)realloc(data->curvePoints, data->curvePointCapacity * sizeof(vec3));
    }

    data->curvePointCount = 0;

    for (int i = 0; i < numSegments; i++) {
        int index0 = i;
        int index1 = (i + 1) % data->pointCount;
        int index2 = (i + 2) % data->pointCount;
        int index3 = (i + 3) % data->pointCount;

        vec3 p0 = data->controlPoints[index0];
        vec3 p1 = data->controlPoints[index1];
        vec3 p2 = data->controlPoints[index2];
        vec3 p3 = data->controlPoints[index3];

        int steps = segSteps[i];
        for (int j = 0; j < steps; j++) {
            float t = (float)j / (float)steps;
            data->curvePoints[data->curvePointCount++] = sg_EvaluateCatmullRom(p0, p1, p2, p3, t, data->tension);
        }
    }

    if (!data->isLooping) {
        int lastIndex = data->pointCount - 2;
        data->curvePoints[data->curvePointCount++] = data->controlPoints[lastIndex];
    }

    free(segDistances);
    free(segSteps);
}

extern void drawDebugLinesBatch(float* verts, int lineCount, mat4 view, mat4 proj);

void sg_RenderCatmullRomNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_CATMULLROMSPLINE) return;
    CatmullRomNodeData* data = &node->data.catmullrom;

    if (data->curvePointCount < 2) return;

    int lineCount = data->isLooping ? data->curvePointCount : (data->curvePointCount - 1);
    int cpLines = data->showControlPoints ? (data->pointCount * 3) : 0;
    
    int totalLines = lineCount + cpLines;
    if (totalLines == 0) return;

    float* batch = (float*)malloc(totalLines * 12 * sizeof(float));
    int batchIdx = 0;

    vec3 c = data->color;
    
    auto addLine = [&](vec3 p1, vec3 p2, vec3 col) {
        vec4 w1 = vec4(p1, 1.0f) * node->world_matrix;
        vec4 w2 = vec4(p2, 1.0f) * node->world_matrix;
        
        batch[batchIdx++] = w1[0]; batch[batchIdx++] = w1[1]; batch[batchIdx++] = w1[2];
        batch[batchIdx++] = col[0]; batch[batchIdx++] = col[1]; batch[batchIdx++] = col[2];
        
        batch[batchIdx++] = w2[0]; batch[batchIdx++] = w2[1]; batch[batchIdx++] = w2[2];
        batch[batchIdx++] = col[0]; batch[batchIdx++] = col[1]; batch[batchIdx++] = col[2];
    };

    // Draw curve
    for (int i = 0; i < data->curvePointCount - 1; i++) {
        addLine(data->curvePoints[i], data->curvePoints[i+1], c);
    }
    if (data->isLooping) {
        addLine(data->curvePoints[data->curvePointCount - 1], data->curvePoints[0], c);
    }

    // Draw control points
    if (data->showControlPoints) {
        vec3 cpColor = vec3(1.0f, 0.0f, 0.0f); // Red
        float s = 0.5f; // size of cross
        for (int i = 0; i < data->pointCount; i++) {
            vec3 p = data->controlPoints[i];
            addLine(p + vec3(-s, 0, 0), p + vec3(s, 0, 0), cpColor);
            addLine(p + vec3(0, -s, 0), p + vec3(0, s, 0), cpColor);
            addLine(p + vec3(0, 0, -s), p + vec3(0, 0, s), cpColor);
        }
    }

    drawDebugLinesBatch(batch, totalLines, view, proj);
    free(batch);
}

vec3 sg_GetSplinePoint(SceneNode* splineNode, float progress) {
    if (!splineNode || splineNode->type != ENTITY_CATMULLROMSPLINE) return vec3(0,0,0);
    CatmullRomNodeData* spline = &splineNode->data.catmullrom;
    if (spline->curvePointCount < 2) return vec3(0,0,0);

    float t = progress;
    if (t > 1.0f) t = spline->isLooping ? (t - floorf(t)) : 1.0f;
    else if (t < 0.0f) t = spline->isLooping ? (t - floorf(t)) : 0.0f;
    
    float fIndex = t * (spline->curvePointCount - 1);
    int idx0 = (int)fIndex;
    int idx1 = idx0 + 1;
    if (idx1 >= spline->curvePointCount) idx1 = spline->curvePointCount - 1;
    
    float localT = fIndex - (float)idx0;
    vec3 localPos = spline->curvePoints[idx0] * (1.0f - localT) + spline->curvePoints[idx1] * localT;
    
    vec4 wPos4 = vec4(localPos, 1.0f) * splineNode->world_matrix;
    return vec3(wPos4[0], wPos4[1], wPos4[2]);
}

vec3 sg_AdvanceSpline(SceneNode* splineNode, float* progressInOut, float speedDelta) {
    if (!splineNode || !progressInOut || splineNode->type != ENTITY_CATMULLROMSPLINE) return vec3(0,0,0);
    CatmullRomNodeData* spline = &splineNode->data.catmullrom;
    
    *progressInOut += speedDelta;
    if (*progressInOut > 1.0f) {
        if (spline->isLooping) *progressInOut -= 1.0f;
        else *progressInOut = 1.0f;
    } else if (*progressInOut < 0.0f) {
        if (spline->isLooping) *progressInOut += 1.0f;
        else *progressInOut = 0.0f;
    }
    
    return sg_GetSplinePoint(splineNode, *progressInOut);
}

void sg_AnimateNodeAlongSpline(SceneNode* splineNode, SceneNode* targetNode, float speedDelta, float lookAhead) {
    if (!splineNode || !targetNode || splineNode->type != ENTITY_CATMULLROMSPLINE) return;
    
    CatmullRomNodeData* spline = &splineNode->data.catmullrom;
    if (spline->curvePointCount < 2) return;

    // Advance progress
    targetNode->pathProgress += speedDelta;
    if (targetNode->pathProgress > 1.0f) {
        if (spline->isLooping) targetNode->pathProgress -= 1.0f;
        else targetNode->pathProgress = 1.0f; // Stop at end
    } else if (targetNode->pathProgress < 0.0f) {
        if (spline->isLooping) targetNode->pathProgress += 1.0f;
        else targetNode->pathProgress = 0.0f;
    }

    vec3 pos = sg_GetSplinePoint(splineNode, targetNode->pathProgress);

    if (targetNode->type == ENTITY_CAMERA) {
        targetNode->data.camera.position = pos;
        if (lookAhead > 0.0f) {
            targetNode->data.camera.target = sg_GetSplinePoint(splineNode, targetNode->pathProgress + lookAhead);
        }
    } else {
        targetNode->position = pos;
        if (lookAhead > 0.0f) {
            vec3 forward = sg_GetSplinePoint(splineNode, targetNode->pathProgress + lookAhead) - pos;
            if (vmath::length(forward) > 0.0001f) {
                forward = vmath::normalize(forward);
                float yaw = atan2f(-forward[0], -forward[2]); 
                float pitch = asinf(forward[1]);
                targetNode->rotation_euler[1] = yaw * (180.0f / 3.14159265f);
                targetNode->rotation_euler[0] = pitch * (180.0f / 3.14159265f);
            }
        }
        extern void sg_MarkSceneDirty();
        sg_MarkSceneDirty();
    }
}
