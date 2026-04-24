// Define array and state
StrategicCamera g_StrategicCameras[MAX_STRATEGIC_CAMERAS];
int g_StrategicCameraCount = 0;
int g_SelectedStrategicCameraIndex = -1;
int g_ActiveStrategicCameraIndex = 0;

void InitStrategicCameras() {
    if (g_StrategicCameraCount > 0) return;

    // Add default camera
    StrategicCamera* cam = &g_StrategicCameras[g_StrategicCameraCount++];
    cam->pos    = vec3(0.0f, 10.0f, 10.0f);
    cam->target = vec3(0.0f, 0.0f, 0.0f);
    cam->up     = vec3(0.0f, 1.0f, 0.0f);
    cam->roll   = 0.0f;
    strcpy(cam->name, "Main Scene Camera");
}

void AddStrategicCamera(const char* name) {
    if (g_StrategicCameraCount >= MAX_STRATEGIC_CAMERAS) return;
    
    StrategicCamera* cam = &g_StrategicCameras[g_StrategicCameraCount++];
    cam->pos    = vec3(10.0f, 10.0f, 10.0f);
    cam->target = vec3(0.0f, 0.0f, 0.0f);
    cam->up     = vec3(0.0f, 1.0f, 0.0f);
    cam->roll   = 0.0f;
    snprintf(cam->name, 32, "%s_%d", name, g_StrategicCameraCount);
}

void ProcessStrategicCamera(void* window_handle, float dt) {
    // Purely GUI controlled
}

void RenderStrategicCameraHelpers(mat4 view, mat4 proj) {
    // Initialize if empty
    InitStrategicCameras();

    for (int i = 0; i < g_StrategicCameraCount; i++) {
        // Skip rendering the "viewfinder" gizmo for the camera we are currently looking through
        if (currentCameraMode == CAM_MODE_STRATEGIC && i == g_ActiveStrategicCameraIndex) {
            continue;
        }

        StrategicCamera* cam = &g_StrategicCameras[i];
        vec3 col = (i == g_SelectedStrategicCameraIndex) ? vec3(1, 1, 1) : vec3(0.7f, 0.7f, 0.7f);

        // 1. Line to Target (Yellow)
        drawDebugLine(cam->pos, cam->target, vec3(1.0f, 1.0f, 0.0f), view, proj);
        
        // 2. Up Vector (Cyan)
        drawDebugLine(cam->pos, cam->pos + cam->up * 2.0f, vec3(0.0f, 1.0f, 1.0f), view, proj);

        // 3. Target Point (Red Crosshair)
        float ts = 0.3f;
        drawDebugLine(cam->target + vec3(-ts, 0, 0), cam->target + vec3(ts, 0, 0), vec3(1,0,0), view, proj);
        drawDebugLine(cam->target + vec3(0, -ts, 0), cam->target + vec3(0, ts, 0), vec3(1,0,0), view, proj);
        drawDebugLine(cam->target + vec3(0, 0, -ts), cam->target + vec3(0, 0, ts), vec3(1,0,0), view, proj);

        // 4. Camera Wireframe Prism
        vec3 cp = cam->pos;
        float bs = 0.3f;
        vec3 forward = normalize(cam->target - cam->pos);
        vec3 right = normalize(cross(forward, cam->up));
        vec3 up = cross(right, forward);
        
        vec3 p1 = cp + (right - up) * bs;
        vec3 p2 = cp + (right + up) * bs;
        vec3 p3 = cp + (-right + up) * bs;
        vec3 p4 = cp + (-right - up) * bs;
        vec3 tip = cp + forward * (bs * 2.5f);

        drawDebugLine(p1, p2, col, view, proj);
        drawDebugLine(p2, p3, col, view, proj);
        drawDebugLine(p3, p4, col, view, proj);
        drawDebugLine(p4, p1, col, view, proj);
        drawDebugLine(p1, tip, col, view, proj);
        drawDebugLine(p2, tip, col, view, proj);
        drawDebugLine(p3, tip, col, view, proj);
        drawDebugLine(p4, tip, col, view, proj);
    }
}
