
// ============================================================================
// Example 04: Volumetric Cloud Rendering
// ============================================================================
#define PROJECT_04
#define PROJECT_03 // Skip platform stubs that redefine updateCameraFromMouse

#include <cstdlib>
#include <ctime>

#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"
#endif

// Includes
#include "engine/utils/primitives.cpp"
#include "engine/utils/pbr.cpp"
#include "engine/utils/skybox.cpp"
#include "engine/effects/particleEmission.cpp"
#include "engine/effects/cloud.cpp"
#include "engine/effects/terrain/terrain.h"

// Globals needed by Terrain
vec3 lightPos(0.0f, 50.0f, 0.0f);
vec3 lightColor(1.0f, 1.0f, 1.0f);
float lightIntensity = 1.0f;
bool useIBL = true;
float iblIntensity = 1.0f;

GLuint HeightMap = 0;

vec3 helmetPos(0.0f, 0.0f, -5.0f);
vec3 helmetRot(0.0f, 0.0f, 0.0f);
float helmetScale = 1.0f;


// Forward declaration for platform GUI function
void NewFrameGUI();


// ---- Camera and mouse vars (project 04 owns these) ----
Camera *mainCamera          = NULL;
vec3    camera_target(0.0f, 0.0f, 0.0f);
float   camera_yaw          = 180.0f;
float   camera_pitch        = 0.0f;
float   mouse_sensitivity   = 0.1f;
float   cameraDist          = 80.0f;

// Cloud parameters are now in engine/effects/cloud.cpp
extern float cloudDensityScale;
extern int   cloudMaxSteps;
extern float cloudStepSize;
extern vec3  cloudBoxPos;
extern vec3  cloudBoxScale;
extern int   gridX, gridZ;
extern float envSpacing, envScale;
extern int   spheresPerEnvMin, spheresPerEnvMax, cloudTexRes;
extern GLuint cloudTexture, cloudVAO, cloudVBO;
void generateCloudSpheres();
void generateCloudTexture();
void initClouds();
void renderClouds();
void cleanupClouds();



#include "engine/effects/terrain/terrain.cpp"

static ParticleEmitter* smokeEmitter = NULL;
static float lastUpdateTime = 0.0f;

// F22 Model
Mesh *f22Meshes = NULL;
int f22MeshCount = 0;
ShaderProgram *Models_Shader = NULL;
vec3 f22Pos(0.0f, 0.0f, 0.0f);
vec3 f22Rot(0.0f, 90.0f, 0.0f); // Default: rotated 90 deg around Y
float f22Scale = 0.5f;

// Particle Lighting controls
static vec3 particleLightPos(10.0f, 10.0f, 10.0f);
static vec3 particleLightColor(1.0f, 0.9f, 0.8f);
static float particleAmbientStrength = 0.2f;
static GLuint smokeNormalMap = 0;

void updateCameraFromMouse(int delta_x, int delta_y) {
    camera_yaw   += delta_x * mouse_sensitivity;
    camera_pitch -= delta_y * mouse_sensitivity;
    if (camera_pitch >  89.0f) camera_pitch =  89.0f;
    if (camera_pitch < -89.0f) camera_pitch = -89.0f;
}

// (Cloud sphere and texture generation functions moved to engine/effects/cloud.cpp)


void projectInit()
{
    // Setup defaults for clouds
    g_powerCurve = 3.0f;
    g_turbulence = 0.5f;

    // Initialize Skybox and IBL
    initSkybox(NULL);

    // Setup Camera
    camera_pos = vec3(0.0f, 5.0f, cameraDist);
    camera_target = vec3(0.0f, 4.0f, 0.0f);
    mainCamera = createCamera(camera_pos, camera_target, vec3(0.0f, 1.0f, 0.0f));

    // ===== Volume Shader Program =====
    const char *ShaderFiles[5] = {
        "engine/shaders/vert_test.glsl", // Index 0: Vertex
        NULL,                               // Index 1: Tess Control
        NULL,                               // Index 2: Tess Evaluation
        NULL,                               // Index 3: Geometry
        "engine/shaders/frag_test.glsl"   // Index 4: Fragment
    };

    if (!buildShaderProgramFromFiles(ShaderFiles, 5,
                                     &VolumeRenderingProgram, attribNames,
                                     attribIndices, 4)) {
        fprintf(gpFile, "Failed to build volumerendering shader program\n");
    } else {
        VolumeRenderingProgram->name = "VolumeRendering";
    }

    initClouds();

    // Initialize Smoke Particle Emitter
    smokeEmitter = createParticleEmitter(1000);
    smokeNormalMap = loadGLTexture("engine/effects/sphere_normal_map.png");
    smokeEmitter->normalMapTexture = smokeNormalMap;
    smokeEmitter->origin = vec3(0.0f, -5.0f, 0.0f);
    smokeEmitter->emissionRate = 80.0f; // Particles per second
    smokeEmitter->velocityBase = vec3(0.0f, 3.0f, 0.0f);
    smokeEmitter->velocitySpread = 2.0f;
    smokeEmitter->lifeMin = 3.0f;
    smokeEmitter->lifeMax = 5.0f;
    smokeEmitter->sizeStart = 18.0f;
    smokeEmitter->sizeEnd = 30.0f;
    smokeEmitter->colorStart = vec4(0.4f, 0.4f, 0.4f, 0.8f);
    smokeEmitter->colorEnd = vec4(0.1f, 0.1f, 0.1f, 0.0f);
    
    // Load F22 Model
    const char* f22Path = "examples/04_clouds/f22blend.glb";
    if (!loadModel(f22Path, &f22Meshes, &f22MeshCount, 1.0f)) {
        fprintf(gpFile, "Failed to load F22 model: %s\n", f22Path);
    } else {
        fprintf(gpFile, "Successfully loaded F22 model with %d meshes\n", f22MeshCount);
    }

    // Build F22 Shader (PBR)
    const char *Models_ShaderFiles[5] = {
        "engine/shaders/vertex_shader.glsl",
        NULL, NULL, NULL,
        "engine/shaders/PBR/pbrFrag.glsl"
    };
    if (!buildShaderProgramFromFiles(Models_ShaderFiles, 5, &Models_Shader, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build F22 PBR shader\n");
    }

    // Initialize Model Controller & Register F22
    ModelController_Init();
    ModelController_Register("F-22 Raptor", f22Meshes, f22MeshCount, &f22Pos, &f22Rot, &f22Scale);

    // ===== Tessellation Shader Program =====
    const char *tessShaderFiles[5] = {
        "engine/shaders/vertex_shader.glsl", "engine/effects/terrain/main_tcs.glsl",
        "engine/effects/terrain/main_tes.glsl", NULL,
        "engine/shaders/PBR/pbrFrag.glsl"
    };
    if (!buildShaderProgramFromFiles(tessShaderFiles, 5, &tessellationShaderProgram, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build tessellation shader program\n");
    } else {
        tessellationShaderProgram->name = "Tessellation";
    }

    terrainMesh = createTerrainMesh();
    HeightMap = createHeightMapTexture(512, 512, 0.01f, 1.0f);

    // Load DamagedHelmet model
    const char* helmetPath = "user/models/DamagedHelmet.glb";
    if (loadModel(helmetPath, &helmetMeshes, &helmetMeshCount, 1.0f)) {
        ModelController_Register("Damaged Helmet", helmetMeshes, helmetMeshCount, &helmetPos, &helmetRot, &helmetScale);
    }

    glEnableVertexAttribArray(ATTRIB_TEXCOORD);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Hide the outside walls, draw the inside walls
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    fprintf(gpFile, "Project 04 (Volumetric Clouds) initialized\n");
}

void projectRender()
{
    if (!VolumeRenderingProgram) return;


    static bool firstRender = true;
    if (firstRender) {
        fprintf(gpFile, "First projectRender() call. Program ID: %u\n", VolumeRenderingProgram->id);
        firstRender = false;
    }

    // Orbit Camera: rotate camera_pos around camera_target
    float yr = camera_yaw   * 3.14159265f / 180.0f;
    float pr = camera_pitch * 3.14159265f / 180.0f;
    
    vec3 offset(
        cameraDist * cos(pr) * sin(yr),
        cameraDist * sin(pr),
        cameraDist * cos(pr) * cos(yr)
    );

    mainCamera->position = camera_target + offset;
    mainCamera->target = camera_target;
    updateCamera(mainCamera);
    viewMatrix = mainCamera->viewMatrix;

    // Render Skybox (renders at max depth)
    renderSkybox(viewMatrix, perspectiveProjectionMatrix);

    // Render Clouds
    renderClouds();

    glUseProgram(0);

    // Render Terrain
    if (terrainMesh && tessellationShaderProgram && HeightMap) {
        renderTerrain(HeightMap);
    }

    // Render Smoke Particles
    renderParticleEmitter(smokeEmitter, viewMatrix, perspectiveProjectionMatrix, particleLightPos, particleLightColor, particleAmbientStrength);

    // Render F22 Model
    if (f22Meshes && Models_Shader) {
        glUseProgram(Models_Shader->id);
        
        // Global uniforms
        glUniformMatrix4fv(glGetUniformLocation(Models_Shader->id, "uProjection"), 1, GL_FALSE, perspectiveProjectionMatrix);
        glUniformMatrix4fv(glGetUniformLocation(Models_Shader->id, "uView"), 1, GL_FALSE, viewMatrix);
        glUniform3fv(glGetUniformLocation(Models_Shader->id, "uViewPos"), 1, mainCamera->position);
        glUniform3fv(glGetUniformLocation(Models_Shader->id, "uLightPos"), 1, particleLightPos);
        glUniform3fv(glGetUniformLocation(Models_Shader->id, "uLightColor"), 1, particleLightColor);
        glUniform1i(glGetUniformLocation(Models_Shader->id, "uHasIBL"), 1);
        glUniform1f(glGetUniformLocation(Models_Shader->id, "uIBLIntensity"), 1.0f);
        
        bindIBL(Models_Shader);
        
        glDisable(GL_CULL_FACE);

        auto drawMeshes = [](Mesh* meshes, int count, vec3 pos, vec3 rot, float scale) {
            for (int i = 0; i < count; i++) {
                Mesh* mesh = &meshes[i];
                mat4 rootMat = mat4::identity();
                rootMat = rootMat * vmath::translate(pos);
                rootMat = rootMat * vmath::rotate(rot[0], vec3(1.0, 0.0, 0.0));
                rootMat = rootMat * vmath::rotate(rot[1], vec3(0.0, 1.0, 0.0));
                rootMat = rootMat * vmath::rotate(rot[2], vec3(0.0, 0.0, 1.0));
                rootMat = rootMat * vmath::scale(scale);

                mat4 subMat = mat4::identity();
                if (mesh->transform) {
                    subMat = vmath::translate(mesh->transform->position) * 
                             vmath::rotate(mesh->transform->rotation[0], vec3(1.0f, 0.0f, 0.0f)) *
                             vmath::rotate(mesh->transform->rotation[1], vec3(0.0f, 1.0f, 0.0f)) *
                             vmath::rotate(mesh->transform->rotation[2], vec3(0.0f, 0.0f, 1.0f)) *
                             vmath::scale(mesh->transform->scale);
                }

                mat4 finalModelMat = rootMat * subMat;
                glUniformMatrix4fv(glGetUniformLocation(Models_Shader->id, "uModel"), 1, GL_FALSE, finalModelMat);
                setMaterialUniforms(Models_Shader, &mesh->material);
                glBindVertexArray(mesh->vao);
                glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
        };

        drawMeshes(f22Meshes, f22MeshCount, f22Pos, f22Rot, f22Scale);
        drawMeshes(helmetMeshes, helmetMeshCount, helmetPos, helmetRot, helmetScale);
        

        glEnable(GL_CULL_FACE);
        
        glUseProgram(0);

        // Render Boundary Highlight
        ModelController_RenderHighlight();
    }
}

void projectUpdate()
{
    float currentTime = platformGetTime();
    if (lastUpdateTime == 0.0f) lastUpdateTime = currentTime;
    float dt = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;

    if (smokeEmitter) {
        // Move emitter to create a trailing effect
        smokeEmitter->origin = vec3(15.0f * cos(currentTime * 0.5f), -5.0f, 15.0f * sin(currentTime * 0.5f));
        updateParticleEmitter(smokeEmitter, dt);
    }

    // Optionally rotate or move the cloud box here
}

void projectCleanup()
{
    cleanupClouds();

    if (terrainMesh) {
        terrainMesh = NULL;
    }
    if (HeightMap) {
        glDeleteTextures(1, &HeightMap);
        HeightMap = 0;
    }
    if (helmetMeshes) {
        freeModel(helmetMeshes, helmetMeshCount);
        helmetMeshes = NULL;
    }
    if (mainCamera) {
        freeCamera(mainCamera);
        mainCamera = NULL;
    }
    if (smokeEmitter) {
        destroyParticleEmitter(smokeEmitter);
        smokeEmitter = NULL;
    }
    if (f22Meshes) {
        freeModel(f22Meshes, f22MeshCount);
        f22Meshes = NULL;
    }
    fprintf(gpFile, "Project 04 (Volumetric Clouds) cleaned up\n");
}

void toggleWireframe() {}

void UpdateGUI() {
#ifdef HAS_IMGUI
    NewFrameGUI();
    ImGui::Begin("Cloud Controls");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Skybox Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int selectedPreset = currentSkyboxPreset;
        if (selectedPreset != currentSkyboxPreset) selectedPreset = currentSkyboxPreset;
        if (ImGui::BeginCombo("Mood", skyboxPresets[selectedPreset].name)) {
            for (int i = 0; i < SKYBOX_PRESET_COUNT; i++) {
                bool isSelected = (selectedPreset == i);
                if (ImGui::Selectable(skyboxPresets[i].name, isSelected)) {
                    selectedPreset = i;
                    if (selectedPreset != currentSkyboxPreset) reloadSkyboxPreset(selectedPreset);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::CollapsingHeader("Worley Noise Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Texture Resolution", &cloudTexRes, 8, 256)) generateCloudTexture();
        if (ImGui::SliderFloat("Power Curve", &g_powerCurve, 1.0f, 5.0f)) generateCloudTexture();
        if (ImGui::SliderFloat("Turbulence", &g_turbulence, 0.0f, 2.0f)) generateCloudTexture();
        if (ImGui::Button("Regenerate Texture")) generateCloudTexture();
    }

    if (ImGui::CollapsingHeader("Raymarching Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Density Scale", &cloudDensityScale, 0.1f, 10.0f);
        ImGui::SliderInt("Max Steps", &cloudMaxSteps, 16, 256);
        ImGui::SliderFloat("Step Size", &cloudStepSize, 0.001f, 0.05f);
    }

    if (ImGui::CollapsingHeader("Cloud Grid Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Grid X", &gridX, 1, 10);
        ImGui::SliderInt("Grid Z", &gridZ, 1, 10);
        ImGui::SliderFloat("Spacing", &envSpacing, 10.0f, 100.0f);
        ImGui::SliderFloat("Envelope Scale", &envScale, 0.1f, 3.0f);
        ImGui::SliderInt("Min Spheres/Env", &spheresPerEnvMin, 1, 8);
        ImGui::SliderInt("Max Spheres/Env", &spheresPerEnvMax, 1, 16);
        if (ImGui::Button("Regenerate Grid")) generateCloudSpheres();
    }

    if (ImGui::CollapsingHeader("Camera Orbit Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Distance", &cameraDist, 10.0f, 500.0f);
        ImGui::DragFloat3("Target", &camera_target[0], 0.5f);
        ImGui::SliderFloat("Orbit Yaw", &camera_yaw, -360.0f, 360.0f);
        ImGui::SliderFloat("Orbit Pitch", &camera_pitch, -89.0f, 89.0f);
    }
    if (ImGui::CollapsingHeader("Smoke Particle System", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (smokeEmitter) {
            auto customSlider = [](const char* label, float* v, float v_min, float v_max) {
                ImGui::PushID(label);
                ImGui::Text("%s", label);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
                ImGui::SliderFloat("##s", v, v_min, v_max);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputFloat("##i", v, 0.0f, 0.0f, "%.2f");
                ImGui::PopID();
            };

            customSlider("Emission Rate", &smokeEmitter->emissionRate, 1.0f, 500.0f);
            
            ImGui::PushID("Velocity Base");
            ImGui::Text("Velocity Base");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
            ImGui::SliderFloat3("##vb_s", &smokeEmitter->velocityBase[0], -10.0f, 10.0f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputFloat3("##vb_i", &smokeEmitter->velocityBase[0], "%.2f");
            ImGui::PopID();

            customSlider("Velocity Spread", &smokeEmitter->velocitySpread, 0.0f, 15.0f);
            customSlider("Life Min", &smokeEmitter->lifeMin, 0.1f, 10.0f);
            customSlider("Life Max", &smokeEmitter->lifeMax, 0.1f, 10.0f);
            customSlider("Size Start", &smokeEmitter->sizeStart, 1.0f, 100.0f);
            customSlider("Size End", &smokeEmitter->sizeEnd, 1.0f, 100.0f);

            ImGui::ColorEdit4("Color Start", &smokeEmitter->colorStart[0]);
            ImGui::ColorEdit4("Color End", &smokeEmitter->colorEnd[0]);

            ImGui::Separator();
            ImGui::Text("Volume & 3D Config");
            ImGui::Checkbox("Use 3D Mesh Particle", &smokeEmitter->use3D);
            
            if (smokeEmitter->use3D) {
                static int currentMeshIdx = 0;
                static int currentNormalIdx = 0;
                const char* meshItems[] = { "engine/effects/icosphere_particle.obj",
                "engine/effects/uvsphere_particle.obj" };
                const char* normalItems[] = { 
                    "engine/effects/sphere_normal_map.png", 
                    "user/models/khadbad/gray_rocks_nor_gl_1k.png", 
                    "user/models/moon_assets/aerial_beach_01_nor_gl_1k.png" 
                };
                
                if (ImGui::Combo("3D Mesh", &currentMeshIdx, meshItems, 1)) {
                    strcpy(smokeEmitter->currentMeshPath, meshItems[currentMeshIdx]);
                }
                if (ImGui::Combo("Normal Map", &currentNormalIdx, normalItems, 3)) {
                    strcpy(smokeEmitter->currentNormalPath, normalItems[currentNormalIdx]);
                }
                
                ImGui::Separator();
                ImGui::Text("3D Material");
                ImGui::SliderFloat("Emission Mix", &smokeEmitter->emissionMix, 0.0f, 1.0f);
                ImGui::ColorEdit3("Emission Color", &smokeEmitter->emissionColor[0]);
                ImGui::SliderFloat("Emission Strength", &smokeEmitter->emissionStrength, 0.0f, 10.0f);
                ImGui::SliderFloat("Transparency", &smokeEmitter->particleTransparency, 0.0f, 1.0f);
            }

            ImGui::Checkbox("Enable 3D Noise/Distortion", &smokeEmitter->enableDistortion);
            ImGui::SliderFloat("Noise Scale", &smokeEmitter->noiseScale, 0.01f, 5.0f);
            ImGui::SliderFloat("Distortion Amount", &smokeEmitter->distortionAmount, 0.0f, 2.0f);
            ImGui::SliderFloat("Normal Merge Factor", &smokeEmitter->normalMergeFactor, 0.0f, 2.0f);

            ImGui::Separator();
            ImGui::Text("Particle Lighting");
            ImGui::DragFloat3("Light Pos", &particleLightPos[0], 0.1f);
            ImGui::ColorEdit3("Light Color", &particleLightColor[0]);
            ImGui::SliderFloat("Ambient Strength", &particleAmbientStrength, 0.0f, 1.0f);
        }
    }
    if (ImGui::CollapsingHeader("F22 Model Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("F22 Position", &f22Pos[0], 0.1f);
        ImGui::DragFloat3("F22 Rotation", &f22Rot[0], 0.5f);
        ImGui::SliderFloat("F22 Scale", &f22Scale, 0.01f, 10.0f);
    }

    // New Global Model Controller
    ModelController_UpdateUI();

    ImGui::End();
#endif
}
