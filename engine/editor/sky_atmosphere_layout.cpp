// sky_atmosphere_layout.cpp — included by attributemanager_layout.cpp

#include <stdio.h>
#include <string.h>
#include <math.h>

// ─── Profile storage ────────────────────────────────────────────────────────

struct SkyProfileData {
    char  name[64];
    float sunElevDeg;
    float sunAziDeg;
    float sunColorR, sunColorG, sunColorB;
    float sunIntensity;
    float sunAngularRadius;
    float exposure;
    float rayleighR, rayleighG, rayleighB;
    float rayleighExpScale;
    float mieScattering;
    float mieAbsorption;
    float mieAnisotropy;
    float mieExpScale;
    float ozoneR, ozoneG, ozoneB;
    float groundR, groundG, groundB;
    float bottomRadius;
    float topRadius;
    float worldScale;
};

static const int    MAX_SKY_PROFILES = 32;
static SkyProfileData s_SkyProfiles[MAX_SKY_PROFILES];
static int            s_SkyProfileCount = 0;
static char           s_NewProfileName[64] = "My Profile";
static int            s_SelectedProfile = -1;
static bool           s_ProfilesDirtyDisk = false;
static bool           s_ProfilesLoaded = false;
static const char*    SKY_PROFILES_PATH = "user/sky_profiles.ini";

// ─── Helpers ────────────────────────────────────────────────────────────────

static const float SA_PI = 3.14159265f;
static float sa_toRad(float d) { return d * (SA_PI / 180.0f); }
static float sa_toDeg(float r) { return r * (180.0f / SA_PI); }

static void sa_HelpTooltip(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ─── Extract elevation/azimuth from a normalised direction ───────────────────

static void sa_DirToAngles(const vec3& dir, float& elevDeg, float& aziDeg) {
    float y = dir[1];
    if (y >  1.0f) y =  1.0f;
    if (y < -1.0f) y = -1.0f;
    elevDeg = sa_toDeg(asinf(y));
    if (fabsf(elevDeg) < 0.05f) elevDeg = 0.0f;   // kill -0.0 display
    aziDeg  = sa_toDeg(atan2f(dir[0], dir[2]));
    if (aziDeg < 0.0f) aziDeg += 360.0f;
}

static void sa_AnglesToDir(float elevDeg, float aziDeg, vec3& dir) {
    float eR = sa_toRad(elevDeg);
    float aR = sa_toRad(aziDeg);
    dir = vec3(cosf(eR) * sinf(aR), sinf(eR), cosf(eR) * cosf(aR));
}

// ─── Apply hard-coded Earth defaults ────────────────────────────────────────

static void sa_ApplyDefaults(SkyAtmosphereNodeData* d) {
    d->bottomRadius            = 6360.0f;
    d->topRadius               = 6460.0f;
    d->groundAlbedo            = vec3(0.3f, 0.3f, 0.3f);
    d->rayleighScattering      = vec3(5.802e-3f, 13.558e-3f, 33.1e-3f);
    d->rayleighDensityExpScale = -0.125f;
    d->mieScattering           = 3.996e-3f;
    d->mieAbsorption           = 4.440e-4f;
    d->mieAnisotropy           = 0.8f;
    d->mieDensityExpScale      = -0.8333f;
    d->absorptionExtinction    = vec3(6.5e-4f, 1.881e-3f, 8.5e-5f);
    d->sunDirection            = vec3(0.0f, 0.342f, 0.940f);
    d->sunColor                = vec3(1.0f, 1.0f, 1.0f);
    d->sunIntensity            = 20.0f;
    d->sunAngularRadius        = 0.02f;
    d->worldScale              = 0.001f;
    d->exposure                = 20.0f;
    d->lutsDirty               = true;
    d->prevIBLSunDir           = vec3(0.0f, 0.0f, 0.0f);
}

// ─── Profile ↔ NodeData conversion ─────────────────────────────────────────

static void sa_CaptureProfile(SkyAtmosphereNodeData* d, SkyProfileData* p) {
    float elevDeg, aziDeg;
    sa_DirToAngles(d->sunDirection, elevDeg, aziDeg);
    p->sunElevDeg      = elevDeg;
    p->sunAziDeg       = aziDeg;
    p->sunColorR       = d->sunColor[0];
    p->sunColorG       = d->sunColor[1];
    p->sunColorB       = d->sunColor[2];
    p->sunIntensity    = d->sunIntensity;
    p->sunAngularRadius= d->sunAngularRadius;
    p->exposure        = d->exposure;
    p->rayleighR       = d->rayleighScattering[0];
    p->rayleighG       = d->rayleighScattering[1];
    p->rayleighB       = d->rayleighScattering[2];
    p->rayleighExpScale= d->rayleighDensityExpScale;
    p->mieScattering   = d->mieScattering;
    p->mieAbsorption   = d->mieAbsorption;
    p->mieAnisotropy   = d->mieAnisotropy;
    p->mieExpScale     = d->mieDensityExpScale;
    p->ozoneR          = d->absorptionExtinction[0];
    p->ozoneG          = d->absorptionExtinction[1];
    p->ozoneB          = d->absorptionExtinction[2];
    p->groundR         = d->groundAlbedo[0];
    p->groundG         = d->groundAlbedo[1];
    p->groundB         = d->groundAlbedo[2];
    p->bottomRadius    = d->bottomRadius;
    p->topRadius       = d->topRadius;
    p->worldScale      = d->worldScale;
}

static void sa_ApplyProfile(SkyProfileData* p, SkyAtmosphereNodeData* d) {
    sa_AnglesToDir(p->sunElevDeg, p->sunAziDeg, d->sunDirection);
    d->sunColor[0]              = p->sunColorR;
    d->sunColor[1]              = p->sunColorG;
    d->sunColor[2]              = p->sunColorB;
    d->sunIntensity             = p->sunIntensity;
    d->sunAngularRadius         = p->sunAngularRadius;
    d->exposure                 = p->exposure;
    d->rayleighScattering       = vec3(p->rayleighR, p->rayleighG, p->rayleighB);
    d->rayleighDensityExpScale  = p->rayleighExpScale;
    d->mieScattering            = p->mieScattering;
    d->mieAbsorption            = p->mieAbsorption;
    d->mieAnisotropy            = p->mieAnisotropy;
    d->mieDensityExpScale       = p->mieExpScale;
    d->absorptionExtinction     = vec3(p->ozoneR, p->ozoneG, p->ozoneB);
    d->groundAlbedo             = vec3(p->groundR, p->groundG, p->groundB);
    d->bottomRadius             = p->bottomRadius;
    d->topRadius                = p->topRadius;
    d->worldScale               = p->worldScale;
    d->lutsDirty                = true;
    d->prevIBLSunDir            = vec3(0.0f, 0.0f, 0.0f);
}

// ─── Disk I/O ────────────────────────────────────────────────────────────────

static void sa_SaveProfilesToDisk() {
    FILE* f = fopen(SKY_PROFILES_PATH, "w");
    if (!f) return;
    for (int i = 0; i < s_SkyProfileCount; i++) {
        SkyProfileData* p = &s_SkyProfiles[i];
        fprintf(f, "[profile:%s]\n", p->name);
        fprintf(f, "sunElevDeg=%f\n",       p->sunElevDeg);
        fprintf(f, "sunAziDeg=%f\n",        p->sunAziDeg);
        fprintf(f, "sunColorR=%f\n",        p->sunColorR);
        fprintf(f, "sunColorG=%f\n",        p->sunColorG);
        fprintf(f, "sunColorB=%f\n",        p->sunColorB);
        fprintf(f, "sunIntensity=%f\n",     p->sunIntensity);
        fprintf(f, "sunAngularRadius=%f\n", p->sunAngularRadius);
        fprintf(f, "exposure=%f\n",         p->exposure);
        fprintf(f, "rayleighR=%f\n",        p->rayleighR);
        fprintf(f, "rayleighG=%f\n",        p->rayleighG);
        fprintf(f, "rayleighB=%f\n",        p->rayleighB);
        fprintf(f, "rayleighExpScale=%f\n", p->rayleighExpScale);
        fprintf(f, "mieScattering=%f\n",    p->mieScattering);
        fprintf(f, "mieAbsorption=%f\n",    p->mieAbsorption);
        fprintf(f, "mieAnisotropy=%f\n",    p->mieAnisotropy);
        fprintf(f, "mieExpScale=%f\n",      p->mieExpScale);
        fprintf(f, "ozoneR=%f\n",           p->ozoneR);
        fprintf(f, "ozoneG=%f\n",           p->ozoneG);
        fprintf(f, "ozoneB=%f\n",           p->ozoneB);
        fprintf(f, "groundR=%f\n",          p->groundR);
        fprintf(f, "groundG=%f\n",          p->groundG);
        fprintf(f, "groundB=%f\n",          p->groundB);
        fprintf(f, "bottomRadius=%f\n",     p->bottomRadius);
        fprintf(f, "topRadius=%f\n",        p->topRadius);
        fprintf(f, "worldScale=%f\n",       p->worldScale);
        fprintf(f, "\n");
    }
    fclose(f);
    s_ProfilesDirtyDisk = false;
}

static void sa_LoadProfilesFromDisk() {
    s_ProfilesLoaded = true;
    FILE* f = fopen(SKY_PROFILES_PATH, "r");
    if (!f) return;

    s_SkyProfileCount = 0;
    char line[256];
    int cur = -1;

    while (fgets(line, sizeof(line), f)) {
        // strip trailing newline
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        if (strncmp(line, "[profile:", 9) == 0 && s_SkyProfileCount < MAX_SKY_PROFILES) {
            cur = s_SkyProfileCount++;
            char name[64] = {0};
            sscanf(line + 9, "%63[^]]", name);
            strncpy(s_SkyProfiles[cur].name, name, 63);
            s_SkyProfiles[cur].name[63] = '\0';
            continue;
        }
        if (cur < 0) continue;

        char key[64] = {0};
        float val = 0.0f;
        if (sscanf(line, "%63[^=]=%f", key, &val) != 2) continue;

        SkyProfileData* p = &s_SkyProfiles[cur];
        if      (!strcmp(key,"sunElevDeg"))        p->sunElevDeg        = val;
        else if (!strcmp(key,"sunAziDeg"))         p->sunAziDeg         = val;
        else if (!strcmp(key,"sunColorR"))         p->sunColorR         = val;
        else if (!strcmp(key,"sunColorG"))         p->sunColorG         = val;
        else if (!strcmp(key,"sunColorB"))         p->sunColorB         = val;
        else if (!strcmp(key,"sunIntensity"))      p->sunIntensity      = val;
        else if (!strcmp(key,"sunAngularRadius"))  p->sunAngularRadius  = val;
        else if (!strcmp(key,"exposure"))          p->exposure          = val;
        else if (!strcmp(key,"rayleighR"))         p->rayleighR         = val;
        else if (!strcmp(key,"rayleighG"))         p->rayleighG         = val;
        else if (!strcmp(key,"rayleighB"))         p->rayleighB         = val;
        else if (!strcmp(key,"rayleighExpScale"))  p->rayleighExpScale  = val;
        else if (!strcmp(key,"mieScattering"))     p->mieScattering     = val;
        else if (!strcmp(key,"mieAbsorption"))     p->mieAbsorption     = val;
        else if (!strcmp(key,"mieAnisotropy"))     p->mieAnisotropy     = val;
        else if (!strcmp(key,"mieExpScale"))       p->mieExpScale       = val;
        else if (!strcmp(key,"ozoneR"))            p->ozoneR            = val;
        else if (!strcmp(key,"ozoneG"))            p->ozoneG            = val;
        else if (!strcmp(key,"ozoneB"))            p->ozoneB            = val;
        else if (!strcmp(key,"groundR"))           p->groundR           = val;
        else if (!strcmp(key,"groundG"))           p->groundG           = val;
        else if (!strcmp(key,"groundB"))           p->groundB           = val;
        else if (!strcmp(key,"bottomRadius"))      p->bottomRadius      = val;
        else if (!strcmp(key,"topRadius"))         p->topRadius         = val;
        else if (!strcmp(key,"worldScale"))        p->worldScale        = val;
    }
    fclose(f);
}

// ─── Main UI ─────────────────────────────────────────────────────────────────

void ShowSkyAtmosphereAttributes(SceneNode* node) {
    if (!node) return;
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;

    if (!s_ProfilesLoaded) sa_LoadProfilesFromDisk();

    // ── Quick Controls ─────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Quick Controls", ImGuiTreeNodeFlags_DefaultOpen)) {

        float elevDeg, aziDeg;
        sa_DirToAngles(d->sunDirection, elevDeg, aziDeg);

        bool sunMoved = false;
        ImGui::SetNextItemWidth(-1.0f);
        sunMoved |= ImGui::SliderFloat("##elev", &elevDeg, -10.0f, 90.0f,
                                       "Elevation  %.1f deg");
        sa_HelpTooltip(
            "Angle of the sun above the horizon.\n"
            " 0 deg = horizon (sunrise / sunset)\n"
            "90 deg = directly overhead (midday)\n"
            "Note: high-noon sky naturally looks calmer\n"
            "than sunrise/sunset — increase Exposure if\n"
            "you want a brighter midday.");

        ImGui::SetNextItemWidth(-1.0f);
        sunMoved |= ImGui::SliderFloat("##azi",  &aziDeg,  0.0f, 360.0f,
                                       "Azimuth   %.1f deg");
        sa_HelpTooltip(
            "Compass direction the sun comes from.\n"
            "  0 / 360 = North (+Z)\n"
            " 90       = East  (+X)\n"
            "180       = South (-Z)\n"
            "270       = West  (-X)");

        if (sunMoved) {
            sa_AnglesToDir(elevDeg, aziDeg, d->sunDirection);
            d->prevIBLSunDir = vec3(0.0f, 0.0f, 0.0f);
        }

        // Time-of-day presets
        ImGui::Spacing();
        float sp  = ImGui::GetStyle().ItemSpacing.x;
        float bW  = (ImGui::GetContentRegionAvail().x - sp * 3) / 4.0f;
        auto preset = [&](float e, float a) {
            sa_AnglesToDir(e, a, d->sunDirection);
            d->prevIBLSunDir = vec3(0.0f, 0.0f, 0.0f);
        };
        if (ImGui::Button("Sunrise", ImVec2(bW, 0))) preset( 5.0f,  90.0f);
        ImGui::SameLine();
        if (ImGui::Button("Midday",  ImVec2(bW, 0))) preset(75.0f, 180.0f);
        ImGui::SameLine();
        if (ImGui::Button("Sunset",  ImVec2(bW, 0))) preset( 5.0f, 270.0f);
        ImGui::SameLine();
        if (ImGui::Button("Night",   ImVec2(bW, 0))) preset(-8.0f, 180.0f);

        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##exp", &d->exposure, 0.5f, 0.1f, 100.0f,
                         "Exposure  %.1f");
        sa_HelpTooltip(
            "Sky brightness multiplier.\n"
            "Works with 1-exp(-L*E) tonemapping so sky\n"
            "never clips to white.\n"
            "~10-20 = Earth-like   ~30+ = very bright alien sky.");
    }

    // ── Sun Appearance ─────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {

        ImGui::ColorEdit3("Color##sunsa", (float*)&d->sunColor);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##sunint", &d->sunIntensity,
                         1.0f, 0.0f, 1000.0f, "Intensity  %.0f");
        sa_HelpTooltip(
            "Brightness of the sun disk.\n"
            "Must be >> 1 to punch above the scattered sky.\n"
            "Default 20 works well with Exposure ~20.");

        float diskDeg = sa_toDeg(d->sunAngularRadius);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##diskr", &diskDeg, 0.01f, 0.1f, 10.0f,
                             "Disk Radius  %.2f deg")) {
            d->sunAngularRadius = sa_toRad(diskDeg);
        }
        sa_HelpTooltip(
            "Angular half-radius of the sun disk in degrees.\n"
            "Real sun ~= 0.27 deg.  Larger values are easier\n"
            "to see on screen.");
    }

    // ── Atmosphere ─────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Atmosphere")) {

        bool changed = false;
        ImGui::SeparatorText("Sky Colour  (Rayleigh)");

        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::DragFloat3("##rayscatter",
            (float*)&d->rayleighScattering,
            0.00005f, 0.0f, 0.1f, "R %.5f  G %.5f  B %.5f");
        sa_HelpTooltip(
            "Rayleigh scattering per km — controls sky tint.\n"
            "Higher B = bluer sky.\n"
            "Earth default: 0.00580 / 0.01356 / 0.03310");

        float skyH = -1.0f / d->rayleighDensityExpScale;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##skyh", &skyH, 0.1f, 1.0f, 30.0f,
                             "Sky Thickness  %.1f km")) {
            d->rayleighDensityExpScale = -1.0f / fmaxf(skyH, 0.01f);
            changed = true;
        }
        sa_HelpTooltip(
            "Effective Rayleigh layer height in km.\n"
            "Earth ~= 8 km.  Higher = sky colour extends\n"
            "further toward the horizon.");

        ImGui::SeparatorText("Haze  (Mie)");

        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::DragFloat("##mieamt", &d->mieScattering,
            0.0001f, 0.0f, 0.05f, "Haze Amount  %.5f /km");
        sa_HelpTooltip("Mie scattering per km — milky haze / fog.");

        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::DragFloat("##mieabs", &d->mieAbsorption,
            0.0001f, 0.0f, 0.05f, "Haze Absorption  %.5f /km");
        sa_HelpTooltip("Absorption fraction of haze.  Higher = darker haze.");

        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::DragFloat("##mieg", &d->mieAnisotropy,
            0.01f, 0.0f, 0.999f, "Haze Glow (g)  %.3f");
        sa_HelpTooltip(
            "Henyey-Greenstein anisotropy.\n"
            "0 = uniform glow   0.8 = tight sun corona\n"
            "Earth default: 0.8");

        float hazeH = -1.0f / d->mieDensityExpScale;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##hazeh", &hazeH, 0.05f, 0.1f, 10.0f,
                             "Haze Thickness  %.2f km")) {
            d->mieDensityExpScale = -1.0f / fmaxf(hazeH, 0.01f);
            changed = true;
        }
        sa_HelpTooltip("Effective Mie layer height.  Earth ~= 1.2 km.");

        ImGui::SeparatorText("Ozone");
        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::DragFloat3("##ozone",
            (float*)&d->absorptionExtinction,
            0.00001f, 0.0f, 0.01f, "R %.6f  G %.6f  B %.6f");
        sa_HelpTooltip(
            "Ozone absorption per km.\n"
            "Gives the limb a blue/purple tint at sunset.");

        if (changed) d->lutsDirty = true;
    }

    // ── Ground & Planet ────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Ground & Planet")) {
        bool changed = false;

        changed |= ImGui::ColorEdit3("Ground Color##sa", (float*)&d->groundAlbedo);
        sa_HelpTooltip("Surface albedo for ground-bounce light.");

        float atmoThick = d->topRadius - d->bottomRadius;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##atmoT", &atmoThick, 0.5f, 10.0f, 500.0f,
                             "Atmo Thickness  %.0f km")) {
            d->topRadius = d->bottomRadius + fmaxf(atmoThick, 1.0f);
            changed = true;
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##botr", &d->bottomRadius, 10.0f, 100.0f, 20000.0f,
                             "Planet Radius  %.0f km")) changed = true;
        sa_HelpTooltip("Earth ~= 6360 km.  Smaller = more curved horizon.");

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##ws", &d->worldScale, 0.0001f, 0.00001f, 1.0f,
                             "World Scale  %.5f km/unit")) changed = true;
        sa_HelpTooltip(
            "1 scene unit = this many km.\n"
            "Default 0.001 = 1 metre per unit.\n"
            "Affects how high the camera sits in the atmosphere.");

        if (changed) d->lutsDirty = true;
    }

    // ── Profiles ───────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Profiles")) {

        // Save current as new profile
        ImGui::SetNextItemWidth(-80.0f);
        ImGui::InputText("##profname", s_NewProfileName, sizeof(s_NewProfileName));
        ImGui::SameLine();
        if (ImGui::Button("Save##prof", ImVec2(-1.0f, 0))) {
            // Find existing slot with same name, or allocate new
            int slot = -1;
            for (int i = 0; i < s_SkyProfileCount; i++) {
                if (strcmp(s_SkyProfiles[i].name, s_NewProfileName) == 0) {
                    slot = i; break;
                }
            }
            if (slot < 0 && s_SkyProfileCount < MAX_SKY_PROFILES)
                slot = s_SkyProfileCount++;
            if (slot >= 0) {
                strncpy(s_SkyProfiles[slot].name, s_NewProfileName, 63);
                s_SkyProfiles[slot].name[63] = '\0';
                sa_CaptureProfile(d, &s_SkyProfiles[slot]);
                sa_SaveProfilesToDisk();
                s_SelectedProfile = slot;
            }
        }
        sa_HelpTooltip("Type a name and click Save to store the current sky settings.\nSaved profiles persist across sessions.");

        ImGui::Spacing();

        // List saved profiles
        if (s_SkyProfileCount == 0) {
            ImGui::TextDisabled("No profiles saved yet.");
        } else {
            ImGui::Text("Saved profiles (%d):", s_SkyProfileCount);
            ImGui::BeginChild("##proflist",
                ImVec2(-1.0f, fminf(180.0f, 28.0f * s_SkyProfileCount + 8.0f)),
                true);

            for (int i = 0; i < s_SkyProfileCount; i++) {
                bool selected = (s_SelectedProfile == i);
                if (ImGui::Selectable(s_SkyProfiles[i].name, selected,
                                      0, ImVec2(ImGui::GetContentRegionAvail().x - 80.0f, 0))) {
                    s_SelectedProfile = i;
                }
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 74.0f);

                ImGui::PushID(i);
                if (ImGui::SmallButton("Apply")) {
                    sa_ApplyProfile(&s_SkyProfiles[i], d);
                    s_SelectedProfile = i;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Del")) {
                    // Compact array
                    for (int j = i; j < s_SkyProfileCount - 1; j++)
                        s_SkyProfiles[j] = s_SkyProfiles[j+1];
                    s_SkyProfileCount--;
                    if (s_SelectedProfile >= s_SkyProfileCount)
                        s_SelectedProfile = s_SkyProfileCount - 1;
                    sa_SaveProfilesToDisk();
                    ImGui::PopID();
                    break; // list changed, stop iterating
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }

    // ── Status & Actions ───────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();

    if (d->lutsDirty)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                           "LUTs dirty — rebuilding next frame");
    else
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LUTs up to date");

    float btnW3 = (ImGui::GetContentRegionAvail().x
                   - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

    if (ImGui::Button("Rebuild LUTs##sa", ImVec2(btnW3, 0)))
        d->lutsDirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Force IBL Bake##sa", ImVec2(btnW3, 0)))
        d->prevIBLSunDir = vec3(0.0f, 0.0f, 0.0f);
    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults##sa", ImVec2(-1.0f, 0))) {
        sa_ApplyDefaults(d);
    }

    // GPU debug
    if (ImGui::TreeNode("GPU Resources##sa")) {
        ImGui::TextDisabled("Transmittance LUT  : %u", d->transmittanceLUT);
        ImGui::TextDisabled("Multi-Scatter LUT  : %u", d->multiScatterLUT);
        ImGui::TextDisabled("Sky-View LUT       : %u", d->skyViewLUT);
        ImGui::TextDisabled("Atmosphere Cubemap : %u", d->atmosphereCubemap);
        ImGui::TextDisabled("Empty VAO          : %u", d->emptyVAO);
        ImGui::TreePop();
    }

    if (ImGui::CollapsingHeader("Sun Shadows")) {
        if (ImGui::Checkbox("Cast Shadow##atmo", &d->castShadow)) {
            if (d->castShadow && !d->shadow) {
                if (d->shadowResolution <= 0) d->shadowResolution = 2048;
                d->shadow = shadow_Create(d->shadowResolution);
            } else if (!d->castShadow && d->shadow) {
                shadow_Destroy(d->shadow);
                d->shadow = nullptr;
            }
        }

        if (d->castShadow && d->shadow) {
            ShadowMap* sm = d->shadow;

            const char* resOpts[] = {"512", "1024", "2048", "4096"};
            const int   resVals[] = { 512,   1024,   2048,   4096};
            int resIdx = 2;
            for (int i = 0; i < 4; i++) if (resVals[i] == sm->resolution) { resIdx = i; break; }
            if (ImGui::Combo("Resolution##atmo", &resIdx, resOpts, 4)) {
                shadow_Destroy(d->shadow);
                d->shadowResolution = resVals[resIdx];
                d->shadow = shadow_Create(d->shadowResolution);
                sm = d->shadow;
                // Restore settings to the new shadow map
                sm->orthoSize = d->shadowOrthoSize;
                sm->nearPlane = d->shadowNear;
                sm->farPlane  = d->shadowFar;
                sm->bias      = d->shadowBias;
            }

            if (ImGui::DragFloat("Ortho Size##atmo", &d->shadowOrthoSize, 0.5f,  1.0f, 2000.0f))
                sm->orthoSize = d->shadowOrthoSize;
            if (ImGui::DragFloat("Near Plane##atmo", &d->shadowNear, 0.1f,  0.1f,   50.0f))
                sm->nearPlane = d->shadowNear;
            if (ImGui::DragFloat("Far Plane##atmo",  &d->shadowFar,  1.0f, 10.0f, 5000.0f))
                sm->farPlane = d->shadowFar;
            if (ImGui::DragFloat("Bias##atmo", &d->shadowBias, 0.0001f, 0.0f, 0.05f, "%.5f"))
                sm->bias = d->shadowBias;

            ImGui::SeparatorText("Polygon Offset (acne / peter-panning)");
            if (ImGui::DragFloat("Offset Factor##atmo", &d->shadowPolyFactor, 0.05f, 0.0f, 16.0f, "%.2f"))
                sm->polyOffsetFactor = d->shadowPolyFactor;
            if (ImGui::DragFloat("Offset Units##atmo",  &d->shadowPolyUnits,  0.1f,  0.0f, 64.0f, "%.2f"))
                sm->polyOffsetUnits = d->shadowPolyUnits;
            ImGui::TextDisabled("0/0 = disabled. Increase to fight self-shadow acne.");

            ImGui::SeparatorText("Debug / Light View");
            ImGui::Text("Eye    : %.1f %.1f %.1f", sm->debugEye[0],    sm->debugEye[1],    sm->debugEye[2]);
            ImGui::Text("Target : %.1f %.1f %.1f", sm->debugTarget[0], sm->debugTarget[1], sm->debugTarget[2]);
            ImGui::Text("FBO=%u  DepthTex=%u  %dx%d", sm->fboID, sm->depthTexID, sm->resolution, sm->resolution);

            ImGui::SeparatorText("Depth Texture");
            float panelW = ImGui::GetContentRegionAvail().x;
            float side   = panelW > 32.0f ? panelW : 256.0f;
            if (side > 512.0f) side = 512.0f;
            ImGui::Image((ImTextureID)(intptr_t)sm->depthTexID, ImVec2(side, side),
                         ImVec2(0,1), ImVec2(1,0));
            ImGui::TextDisabled("Black = near to light, White = far / cleared.");
        }
    }
}
