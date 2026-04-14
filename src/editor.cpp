#include "editor.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "imgui.h"
#include "core/raycast.h"
#include <cstdio>
#include <cmath>

static const float BASE_FONT_SIZE = 36.0f;
static const float REFERENCE_HEIGHT = 1080.0f;

// custom glyph ranges: default + symbols used by window controls
static const ImWchar* get_glyph_ranges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x2010, 0x2027, // General Punctuation (em dash U+2014)
        0x2500, 0x25FF, // Box Drawing + Geometric Shapes (U+25A1 white square)
        0x2700, 0x27BF, // Dingbats (U+2715 multiplication X)
        0x2290, 0x22A5, // Math operators (U+22A1 squared dot for restore)
        0, // terminator
    };
    return ranges;
}

static void rebuild_font(EditorUI *ui, float fontSize) {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    fontCfg.GlyphRanges = get_glyph_ranges();
    io.Fonts->AddFontFromFileTTF(ui->fontPath, fontSize, &fontCfg);
    io.Fonts->Build();
    io.FontGlobalScale = 1.0f;
    ui->lastFontSize = fontSize;
}

void editor_init(Editor *ed, int screenW, int screenH) {
    // window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenW, screenH, "btw");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable ESC to quit

    // core systems (init ui early so fontPath is available)
    scene_init(&ed->scene);
    editor_camera_init(&ed->camera);
    timeline_init(&ed->timeline);
    ui_init(&ed->ui, screenW, screenH);
    shadowmap_init(&ed->shadowMap, 4096);

    // store font path — try ../font/ first (build/), then ../../font/ (build/Debug/)
    {
        const char *appDir = GetApplicationDirectory();
        const char *fontName = "ZedMonoNerdFontMono-Medium.ttf";
        snprintf(ed->ui.fontPath, sizeof(ed->ui.fontPath), "%s../font/%s", appDir, fontName);
        if (!FileExists(ed->ui.fontPath)) {
            snprintf(ed->ui.fontPath, sizeof(ed->ui.fontPath), "%s../../font/%s", appDir, fontName);
        }
    }

    // imgui + font
    rlImGuiBeginInitImGui();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // disable imgui.ini
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    fontCfg.GlyphRanges = get_glyph_ranges();
    // use actual screen height (may differ from requested due to DPI scaling)
    float actualH = (float)GetScreenHeight();
    float initSize = BASE_FONT_SIZE * (actualH / 1080.0f) * ed->ui.uiScale;
    if (initSize < 10.0f) initSize = 10.0f;
    io.Fonts->AddFontFromFileTTF(ed->ui.fontPath, initSize, &fontCfg);
    io.FontGlobalScale = 1.0f;
    ed->ui.lastFontSize = 0.0f; // force rebuild on first frame (window size may not be final yet)
    rlImGuiEndInitImGui();

    // editor theme — base background #181818
    {
        ImGuiStyle &st = ImGui::GetStyle();
        st.WindowRounding    = 2.0f;
        st.FrameRounding     = 2.0f;
        st.GrabRounding      = 2.0f;
        st.TabRounding       = 2.0f;
        st.ScrollbarRounding = 2.0f;
        st.WindowBorderSize  = 1.0f;
        st.FrameBorderSize   = 0.0f;
        st.PopupBorderSize   = 1.0f;
        st.WindowPadding     = ImVec2(8, 8);
        st.FramePadding      = ImVec2(6, 4);
        st.ItemSpacing       = ImVec2(8, 6);

        ImVec4 *c = st.Colors;

        // Monokai Pro (Filter Machine) palette on #181818 base
        // text:    #d4d4d4  -> 0.831, 0.831, 0.831
        // muted:   #727072  -> 0.447, 0.439, 0.447
        // border:  #2d2d2d  -> 0.176
        // raised:  #1d1d1d  -> 0.114
        // input:   #141414  -> 0.078
        // select:  #363636  -> 0.212
        // accent:  #fc9867  orange -> 0.988, 0.596, 0.404
        // green:   #a9dc76  -> 0.663, 0.863, 0.463
        // blue:    #78dce8  -> 0.471, 0.863, 0.910
        // yellow:  #ffd866  -> 1.0, 0.847, 0.400

        // backgrounds
        c[ImGuiCol_WindowBg]             = ImVec4(0.094f, 0.094f, 0.094f, 1.0f); // #181818
        c[ImGuiCol_ChildBg]              = ImVec4(0.094f, 0.094f, 0.094f, 1.0f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.114f, 0.114f, 0.114f, 0.98f); // #1d1d1d
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.094f, 0.094f, 0.094f, 1.0f);

        // borders — neutral gray
        c[ImGuiCol_Border]               = ImVec4(0.176f, 0.176f, 0.176f, 0.6f); // #2d2d2d
        c[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // text
        c[ImGuiCol_Text]                 = ImVec4(0.831f, 0.831f, 0.831f, 1.0f); // #d4d4d4
        c[ImGuiCol_TextDisabled]         = ImVec4(0.447f, 0.439f, 0.447f, 1.0f); // #727072

        // frames (inputs, sliders, combos)
        c[ImGuiCol_FrameBg]              = ImVec4(0.078f, 0.078f, 0.078f, 1.0f); // #141414
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.133f, 0.133f, 0.133f, 1.0f); // #222222
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.176f, 0.176f, 0.176f, 1.0f); // #2d2d2d

        // title bar
        c[ImGuiCol_TitleBg]              = ImVec4(0.075f, 0.075f, 0.075f, 1.0f); // #131313
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.094f, 0.094f, 0.094f, 1.0f); // #181818
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.059f, 0.059f, 0.059f, 1.0f); // #0f0f0f

        // tabs
        c[ImGuiCol_Tab]                  = ImVec4(0.102f, 0.102f, 0.102f, 1.0f); // #1a1a1a
        c[ImGuiCol_TabHovered]           = ImVec4(0.176f, 0.176f, 0.176f, 1.0f); // #2d2d2d
        c[ImGuiCol_TabSelected]          = ImVec4(0.133f, 0.133f, 0.133f, 1.0f); // #222222
        c[ImGuiCol_TabSelectedOverline]  = ImVec4(0.988f, 0.596f, 0.404f, 0.8f); // #fc9867 orange
        c[ImGuiCol_TabDimmed]            = ImVec4(0.075f, 0.075f, 0.075f, 1.0f);
        c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);
        c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.447f, 0.439f, 0.447f, 0.5f);

        // headers — neutral with orange active
        c[ImGuiCol_Header]              = ImVec4(0.176f, 0.176f, 0.176f, 0.6f);  // #2d2d2d
        c[ImGuiCol_HeaderHovered]       = ImVec4(0.220f, 0.220f, 0.220f, 0.8f);  // #383838
        c[ImGuiCol_HeaderActive]        = ImVec4(0.988f, 0.596f, 0.404f, 0.35f); // #fc9867

        // buttons
        c[ImGuiCol_Button]              = ImVec4(0.176f, 0.176f, 0.176f, 1.0f);  // #2d2d2d
        c[ImGuiCol_ButtonHovered]       = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);  // #383838
        c[ImGuiCol_ButtonActive]        = ImVec4(0.988f, 0.596f, 0.404f, 0.6f);  // #fc9867

        // checkmarks, sliders — orange accent
        c[ImGuiCol_CheckMark]           = ImVec4(0.988f, 0.596f, 0.404f, 1.0f);  // #fc9867
        c[ImGuiCol_SliderGrab]          = ImVec4(0.988f, 0.596f, 0.404f, 0.7f);
        c[ImGuiCol_SliderGrabActive]    = ImVec4(0.988f, 0.596f, 0.404f, 1.0f);

        // scrollbar
        c[ImGuiCol_ScrollbarBg]         = ImVec4(0.075f, 0.075f, 0.075f, 0.6f);
        c[ImGuiCol_ScrollbarGrab]       = ImVec4(0.212f, 0.212f, 0.212f, 1.0f);  // #363636
        c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);

        // separators
        c[ImGuiCol_Separator]           = ImVec4(0.176f, 0.176f, 0.176f, 0.6f);
        c[ImGuiCol_SeparatorHovered]    = ImVec4(0.988f, 0.596f, 0.404f, 0.6f);
        c[ImGuiCol_SeparatorActive]     = ImVec4(0.988f, 0.596f, 0.404f, 1.0f);

        // resize grip
        c[ImGuiCol_ResizeGrip]          = ImVec4(0.176f, 0.176f, 0.176f, 0.4f);
        c[ImGuiCol_ResizeGripHovered]   = ImVec4(0.988f, 0.596f, 0.404f, 0.5f);
        c[ImGuiCol_ResizeGripActive]    = ImVec4(0.988f, 0.596f, 0.404f, 0.9f);

        // docking
        c[ImGuiCol_DockingPreview]      = ImVec4(0.988f, 0.596f, 0.404f, 0.5f);
        c[ImGuiCol_DockingEmptyBg]      = ImVec4(0.075f, 0.075f, 0.075f, 1.0f);
    }

    lighting_init(&ed->lighting);

    ed->running = true;
}

bool editor_should_close(const Editor *ed) {
    return !ed->running || ed->ui.wantClose || WindowShouldClose();
}

void editor_update(Editor *ed) {
    // font scaling — rebuild atlas when target pixel size changes
    float screenH = (float)GetScreenHeight();
    if (screenH < 100.0f) screenH = REFERENCE_HEIGHT;
    float targetFontSize = BASE_FONT_SIZE * (screenH / REFERENCE_HEIGHT) * ed->ui.uiScale;
    if (targetFontSize < 10.0f) targetFontSize = 10.0f;
    // only rebuild if size changed by more than 1px (avoid constant rebuilds during resize)
    if (fabsf(targetFontSize - ed->ui.lastFontSize) > 1.0f) {
        rebuild_font(&ed->ui, targetFontSize);
    }

    // timeline
    timeline_update(&ed->timeline);

    // apply keyframe animation
    if (ed->timeline.state == PLAYBACK_PLAYING) {
        for (int i = 0; i < ed->scene.objectCount; i++) {
            SceneObject *obj = &ed->scene.objects[i];
            if (obj->keyframeCount > 0) {
                ObjectTransform t;
                keyframe_evaluate(obj, ed->timeline.currentFrame, &t);
                obj->transform = t;
            }
        }
    }

    // camera (disable orbit/pan when in placement mode so clicks go to placement)
    editor_camera_update(&ed->camera,
        ed->ui.viewportHovered && !ed->ui.placementMode && !ImGui::GetIO().WantCaptureKeyboard);

    // transform mode shortcuts
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (IsKeyPressed(KEY_T)) ed->ui.transformMode = TMODE_TRANSLATE;
        if (IsKeyPressed(KEY_R)) ed->ui.transformMode = TMODE_ROTATE;
        if (IsKeyPressed(KEY_Y)) ed->ui.transformMode = TMODE_SCALE;
    }

    // placement mode: map mouse to ground plane y=0
    if (ed->ui.placementMode) {
        Vector2 mouse = GetMousePosition();
        // map screen mouse to viewport-local UV
        float localX = (mouse.x - ed->ui.vpImageX) / ed->ui.vpImageW;
        float localY = (mouse.y - ed->ui.vpImageY) / ed->ui.vpImageH;

        bool inViewport = localX >= 0 && localX <= 1 && localY >= 0 && localY <= 1;
        if (inViewport) {
            // map UV to render texture pixel coords
            Vector2 rtMouse = { localX * (float)ed->ui.viewportW,
                                localY * (float)ed->ui.viewportH };
            Ray ray = GetScreenToWorldRayEx(rtMouse, ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);

            // intersect with ground plane y=0
            if (ray.direction.y != 0.0f) {
                float t = -ray.position.y / ray.direction.y;
                if (t > 0.0f) {
                    ed->ui.placementPos = Vector3{
                        ray.position.x + ray.direction.x * t,
                        0.0f,
                        ray.position.z + ray.direction.z * t
                    };
                    ed->ui.placementValid = true;
                } else {
                    ed->ui.placementValid = false;
                }
            } else {
                ed->ui.placementValid = false;
            }

            // left-click to place
            if (ed->ui.placementValid && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const char *names[] = {
                    "None", "Cube", "Sphere", "Plane",
                    "Cylinder", "Cone", "Torus", "Knot", "Capsule", "Polygon",
                    "Teapot", "Camera", "Light", "Model"
                };
                int idx = scene_add(&ed->scene, names[ed->ui.placementType], ed->ui.placementType);
                if (idx >= 0) {
                    ed->scene.objects[idx].transform.position = ed->ui.placementPos;
                    scene_deselect_all(&ed->scene);
                    scene_select(&ed->scene, ed->scene.objects[idx].id, false, false);
                }
                ed->ui.placementMode = false;
                ed->ui.placementValid = false;
            }
        } else {
            ed->ui.placementValid = false;
        }

        // cancel placement
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            ed->ui.placementMode = false;
            ed->ui.placementValid = false;
        }
    }

    // helper: build ray from screen mouse into viewport
    auto viewport_ray = [&](Vector2 mouse) -> Ray {
        float localX = (mouse.x - ed->ui.vpImageX) / ed->ui.vpImageW;
        float localY = (mouse.y - ed->ui.vpImageY) / ed->ui.vpImageH;
        Vector2 rtMouse = { localX * (float)ed->ui.viewportW, localY * (float)ed->ui.viewportH };
        return GetScreenToWorldRayEx(rtMouse, ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
    };

    // gizmo drag interaction
    if (ed->ui.gizmoDragging) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ed->ui.gizmoDragging = false;
            ed->ui.gizmoActiveAxis = GIZMO_NONE;
        } else {
            SceneObject *obj = scene_first_selected(&ed->scene);
            if (obj) {
                Vector2 mouse = GetMousePosition();
                float dx = mouse.x - ed->ui.gizmoDragStart.x;
                float dy = mouse.y - ed->ui.gizmoDragStart.y;
                float sensitivity = ed->camera.distance / (float)ed->ui.viewportH;

                // project world axis onto screen to get correct drag direction
                auto axis_screen_dir = [&](Vector3 worldAxis) -> Vector2 {
                    Vector3 p = ed->ui.gizmoDragObjStart;
                    Vector2 s0 = GetWorldToScreenEx(p, ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
                    Vector2 s1 = GetWorldToScreenEx(Vector3Add(p, worldAxis), ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
                    Vector2 dir = { s1.x - s0.x, s1.y - s0.y };
                    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if (len > 0.001f) { dir.x /= len; dir.y /= len; }
                    return dir;
                };

                if (ed->ui.transformMode == TMODE_TRANSLATE) {
                    Vector3 worldAxis = {0};
                    if (ed->ui.gizmoActiveAxis == GIZMO_X) worldAxis = Vector3{1, 0, 0};
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y) worldAxis = Vector3{0, 1, 0};
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z) worldAxis = Vector3{0, 0, 1};

                    Vector2 screenDir = axis_screen_dir(worldAxis);
                    float delta = (dx * screenDir.x + dy * screenDir.y) * sensitivity;

                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.position.x = ed->ui.gizmoDragObjStart.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.position.y = ed->ui.gizmoDragObjStart.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.position.z = ed->ui.gizmoDragObjStart.z + delta;
                } else if (ed->ui.transformMode == TMODE_ROTATE) {
                    float delta = dx * 0.5f; // degrees per pixel
                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.rotation.x = ed->ui.gizmoDragObjStart.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.rotation.y = ed->ui.gizmoDragObjStart.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.rotation.z = ed->ui.gizmoDragObjStart.z + delta;
                } else if (ed->ui.transformMode == TMODE_SCALE) {
                    float delta = dx * sensitivity;
                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.scale.x = ed->ui.gizmoDragObjStart.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.scale.y = ed->ui.gizmoDragObjStart.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.scale.z = ed->ui.gizmoDragObjStart.z + delta;
                }
            }
        }
    }

    // viewport click: gizmo pick first, then object select
    if (!ed->ui.gizmoDragging && !ed->ui.placementMode
        && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        float localX = (mouse.x - ed->ui.vpImageX) / ed->ui.vpImageW;
        float localY = (mouse.y - ed->ui.vpImageY) / ed->ui.vpImageH;
        bool inVP = localX >= 0 && localX <= 1 && localY >= 0 && localY <= 1;
        if (inVP) {
            // try gizmo first
            bool gizmoHit = false;
            SceneObject *sel = scene_first_selected(&ed->scene);
            if (sel) {
                Ray ray = viewport_ray(mouse);
                GizmoAxis axis = gizmo_hit_test(sel, ray, ed->ui.transformMode);
                if (axis != GIZMO_NONE) {
                    ed->ui.gizmoActiveAxis = axis;
                    ed->ui.gizmoDragging = true;
                    ed->ui.gizmoDragStart = mouse;
                    if (ed->ui.transformMode == TMODE_TRANSLATE)
                        ed->ui.gizmoDragObjStart = sel->transform.position;
                    else if (ed->ui.transformMode == TMODE_ROTATE)
                        ed->ui.gizmoDragObjStart = sel->transform.rotation;
                    else
                        ed->ui.gizmoDragObjStart = sel->transform.scale;
                    gizmoHit = true;
                }
            }

            // if gizmo not hit, do object selection
            if (!gizmoHit) {
                RayHitResult hit = raycast_from_mouse(&ed->scene, ed->camera.cam, mouse,
                    ed->ui.vpImageX, ed->ui.vpImageY, ed->ui.vpImageW, ed->ui.vpImageH,
                    ed->ui.viewportW, ed->ui.viewportH);
                bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                if (hit.hit) {
                    uint32_t id = ed->scene.objects[hit.objectIndex].id;
                    scene_select(&ed->scene, id, ctrl, ctrl);
                } else if (!ctrl) {
                    scene_deselect_all(&ed->scene);
                }
            }
        }
    }

    // delete key — remove all selected objects
    if (ed->scene.selectedCount > 0 && IsKeyPressed(KEY_DELETE) && !ImGui::GetIO().WantCaptureKeyboard) {
        // collect IDs first since indices shift during removal
        uint32_t ids[MAX_SELECTED];
        int count = ed->scene.selectedCount;
        for (int i = 0; i < count; i++) ids[i] = ed->scene.selectedIds[i];
        for (int i = 0; i < count; i++) {
            int idx = scene_find_by_id(&ed->scene, ids[i]);
            if (idx >= 0) scene_remove(&ed->scene, idx);
        }
    }
}

void editor_draw(Editor *ed) {
    ui_update_layout(&ed->ui);

    // determine active camera
    Camera3D activeCam = ed->camera.cam;
    float activeNear = ed->camera.nearPlane;
    float activeFar = ed->camera.farPlane;
    float activeFov = ed->camera.fov;
    bool activeOrtho = ed->camera.ortho;

    if (ed->ui.activeCameraId != 0) {
        SceneObject *camObj = scene_get_by_id(&ed->scene, ed->ui.activeCameraId);
        if (camObj && camObj->type == OBJ_CAMERA) {
            // build Camera3D from scene camera transform
            // camera looks along -Z in local space, rotated by euler angles
            Vector3 pos = camObj->transform.position;
            Vector3 rot = camObj->transform.rotation;
            // compute forward direction from rotation
            Matrix rotMat = MatrixRotateXYZ(Vector3{rot.x * DEG2RAD, rot.y * DEG2RAD, rot.z * DEG2RAD});
            Vector3 forward = Vector3Transform(Vector3{0, 0, -1}, rotMat);
            Vector3 up = Vector3Transform(Vector3{0, 1, 0}, rotMat);

            activeCam.position = pos;
            activeCam.target = Vector3Add(pos, forward);
            activeCam.up = up;
            activeCam.fovy = camObj->camFov;
            activeCam.projection = camObj->camOrtho ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
            activeNear = camObj->camNear;
            activeFar = camObj->camFar;
            activeFov = camObj->camFov;
            activeOrtho = camObj->camOrtho;
        }
    }

    // collect lights for this frame
    lighting_collect(&ed->lighting, &ed->scene);
    lighting_update_shader(&ed->lighting, activeCam.position);

    // shadow pass: render depth from the first directional light
    bool hasShadow = false;
    if (ed->shadowMap.initialized) {
        for (int i = 0; i < ed->lighting.lightCount; i++) {
            if (ed->lighting.lights[i].type == LIGHT_DIRECTIONAL) {
                Vector3 dir = ed->lighting.lights[i].direction;
                float sceneRadius = activeFar * 0.3f;
                if (sceneRadius > 50.0f) sceneRadius = 50.0f;
                Vector3 sceneCenter = ed->camera.target;

                shadowmap_begin(&ed->shadowMap, dir, sceneCenter, sceneRadius);
                BeginShaderMode(ed->shadowMap.depthShader);
                scene_draw(&ed->scene, DRAW_SOLID, NULL);
                EndShaderMode();
                shadowmap_end(&ed->shadowMap);
                hasShadow = true;
                break;
            }
        }
    }
    lighting_bind_shadow(&ed->lighting, hasShadow ? &ed->shadowMap : NULL);

    // 3D scene to render texture
    BeginTextureMode(ed->ui.viewportRT);
        ClearBackground(DARKGRAY);
        BeginMode3D(activeCam);
            // override near/far clipping planes
            {
                double aspect = (double)ed->ui.viewportW / (double)ed->ui.viewportH;
                Matrix proj;
                if (activeOrtho) {
                    double top = activeFov / 2.0;
                    double right = top * aspect;
                    proj = MatrixOrtho(-right, right, -top, top, activeNear, activeFar);
                } else {
                    proj = MatrixPerspective(activeFov * DEG2RAD, aspect, activeNear, activeFar);
                }
                rlSetMatrixProjection(proj);
            }
            if (ed->ui.showGrid) {
                DrawGrid(ed->ui.gridSize, ed->ui.gridSpacing);
            }
            // main pass with lighting + shadows
            scene_draw(&ed->scene, ed->ui.drawMode, &ed->lighting);
            scene_draw_selection(&ed->scene);
            scene_draw_gizmo(&ed->scene, ed->ui.transformMode, ed->ui.gizmoActiveAxis);
            if (ed->ui.placementMode && ed->ui.placementValid) {
                scene_draw_preview(ed->ui.placementType, ed->ui.placementPos);
            }
        EndMode3D();
    EndTextureMode();

    // UI
    BeginDrawing();
        ClearBackground(Color{24, 24, 24, 255});
        rlImGuiBegin();
            ui_menu_bar(&ed->scene, &ed->camera, &ed->timeline, &ed->ui);
            ui_dockspace(&ed->ui);
            ui_viewport(&ed->ui);
            ui_hierarchy(&ed->scene, &ed->ui);
            ui_add_object(&ed->scene, &ed->ui);
            ui_camera(&ed->scene, &ed->camera, &ed->ui);
            ui_properties(&ed->scene, &ed->ui);
            ui_timeline(&ed->scene, &ed->timeline, &ed->ui);
            ui_error_popup(&ed->ui);
            ui_shortcut_popup();
        rlImGuiEnd();
    EndDrawing();
}

void editor_shutdown(Editor *ed) {
    for (int i = 0; i < ed->scene.objectCount; i++) {
        SceneObject *obj = &ed->scene.objects[i];
        if (obj->modelLoaded) UnloadModel(obj->model);
        if (obj->material.hasTexture) UnloadTexture(obj->material.texture);
    }
    lighting_shutdown(&ed->lighting);
    shadowmap_unload(&ed->shadowMap);
    ui_shutdown(&ed->ui);
    rlImGuiShutdown();
    CloseWindow();
}
