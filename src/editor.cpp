#include "editor.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "imgui.h"
#include "core/raycast.h"
#include "core/serializer.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static const float BASE_FONT_SIZE = 36.0f;
#ifdef _WIN32
static const float GLOBAL_FONT_SCALE = 0.5f;

#else
static const float GLOBAL_FONT_SCALE = 0.8f;
#endif // _WIN32

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
    io.FontGlobalScale = GLOBAL_FONT_SCALE;
    ui->lastFontSize = fontSize;
}

void editor_init(Editor *ed, int screenW, int screenH) {
    // window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED | FLAG_MSAA_4X_HINT);
    InitWindow(screenW, screenH, "Melder");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable ESC to quit

    // core systems (init ui early so fontPath is available)
    scene_init(&ed->scene);
    editor_camera_init(&ed->camera);
    timeline_init(&ed->timeline);
    ed->lastKeyframeFrame = ed->timeline.currentFrame;
    script_init(&ed->scripting, &ed->scene, &ed->timeline);
    ui_init(&ed->ui, screenW, screenH);
    shadowmap_init(&ed->shadowMap, 8192);

    // init undo
    memset(&ed->undo, 0, sizeof(ed->undo));
    ed->undo.current = -1;
    ed->clipboard.count = 0;

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
    io.FontGlobalScale = GLOBAL_FONT_SCALE;
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

static void editor_snapshot(Editor *ed) {
    ed->snapshotCount = ed->scene.objectCount;
    for (int i = 0; i < ed->snapshotCount; i++) {
        ed->snapshot[i].transform = ed->scene.objects[i].transform;
        ed->snapshot[i].color = ed->scene.objects[i].material.color;
        ed->snapshot[i].visible = ed->scene.objects[i].visible;
        ed->snapshot[i].velocity = ed->scene.objects[i].velocity;
        ed->snapshot[i].usePhysics = ed->scene.objects[i].usePhysics;
        ed->snapshot[i].useGravity = ed->scene.objects[i].useGravity;
        ed->snapshot[i].isStatic = ed->scene.objects[i].isStatic;
        ed->snapshot[i].mass = ed->scene.objects[i].mass;
        ed->snapshot[i].restitution = ed->scene.objects[i].restitution;
        ed->snapshot[i].friction = ed->scene.objects[i].friction;
    }
}

static void editor_restore(Editor *ed) {
    for (int i = 0; i < ed->snapshotCount && i < ed->scene.objectCount; i++) {
        ed->scene.objects[i].transform = ed->snapshot[i].transform;
        ed->scene.objects[i].material.color = ed->snapshot[i].color;
        ed->scene.objects[i].visible = ed->snapshot[i].visible;
        ed->scene.objects[i].velocity = ed->snapshot[i].velocity;
        ed->scene.objects[i].usePhysics = ed->snapshot[i].usePhysics;
        ed->scene.objects[i].useGravity = ed->snapshot[i].useGravity;
        ed->scene.objects[i].isStatic = ed->snapshot[i].isStatic;
        ed->scene.objects[i].mass = ed->snapshot[i].mass;
        ed->scene.objects[i].restitution = ed->snapshot[i].restitution;
        ed->scene.objects[i].friction = ed->snapshot[i].friction;
    }
}

static void editor_play(Editor *ed) {
    if (ed->playMode) return;
    editor_snapshot(ed);
    // reset physics on all objects — Lua scripts must enable it explicitly
    for (int i = 0; i < ed->scene.objectCount; i++) {
        ed->scene.objects[i].usePhysics = false;
        ed->scene.objects[i].velocity = {0, 0, 0};
    }
    script_load_object_scripts(&ed->scripting);
    script_play(&ed->scripting);
    timeline_play(&ed->timeline);
    ed->playMode = true;
    ed->ui.playMode = true;
    ed->ui.activeViewportTab = 1; // switch to animation tab
}

static void editor_stop(Editor *ed) {
    if (!ed->playMode) return;
    script_stop(&ed->scripting);
    timeline_stop(&ed->timeline);
    editor_restore(ed);
    ed->playMode = false;
    ed->ui.playMode = false;
    ed->ui.paused = false;
    ed->ui.activeViewportTab = 0; // switch back to edit tab
}

void editor_update(Editor *ed) {
    // font scaling — rebuild atlas when target pixel size changes
    float targetFontSize = BASE_FONT_SIZE * ((float)GetScreenHeight() / 1080.0f) * ed->ui.uiScale;
    if (targetFontSize < 10.0f) targetFontSize = 10.0f;
    // only rebuild if size changed by more than 1px (avoid constant rebuilds during resize)
    if (fabsf(targetFontSize - ed->ui.lastFontSize) > 1.0f) {
        rebuild_font(&ed->ui, targetFontSize);
    }

    // handle undo push requested by UI
    if (ed->ui.undoPending) {
        undo_push(ed);
        ed->ui.undoPending = false;
    }

    // handle load request from UI
    if (ed->ui.wantLoad) {
        ed->ui.wantLoad = false;
        if (ed->playMode) editor_stop(ed);
        if (!editor_load(ed, ed->ui.loadPath)) {
            snprintf(ed->ui.errorMessage, sizeof(ed->ui.errorMessage),
                     "Failed to load: %s", ed->ui.loadPath);
            ed->ui.showErrorPopup = true;
        } else {
            snprintf(ed->ui.currentFilePath, sizeof(ed->ui.currentFilePath), "%s", ed->ui.loadPath);
        }
    }

    // handle play/stop/pause requests from UI
    if (ed->ui.wantPlay) {
        ed->ui.wantPlay = false;
        editor_play(ed);
    }
    if (ed->ui.wantPause) {
        ed->ui.wantPause = false;
        if (ed->playMode) {
            ed->ui.paused = !ed->ui.paused;
        }
    }
    if (ed->ui.wantStop) {
        ed->ui.wantStop = false;
        editor_stop(ed);
    }

    // animation (only in play mode and not paused)
    if (ed->playMode && !ed->ui.paused) {
        float dt = GetFrameTime();
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

        // physics step
        physics_step(&ed->scene, dt);

        // Lua scripting update
        script_update(&ed->scripting, dt);

        // check if animation reached end of duration
        if (ed->timeline.endFrame < 999999 && !ed->ui.repeatPlayback) {
            double elapsed = GetTime() - ed->scripting.startTime;
            double duration = (double)ed->timeline.endFrame / ed->timeline.fps;
            if (elapsed >= duration) {
                editor_stop(ed);
            }
        }
    }

    // evaluate keyframes when scrubbing timeline (not during play mode)
    if (!ed->playMode && ed->timeline.currentFrame != ed->lastKeyframeFrame) {
        ed->lastKeyframeFrame = ed->timeline.currentFrame;
        for (int i = 0; i < ed->scene.objectCount; i++) {
            SceneObject *obj = &ed->scene.objects[i];
            if (obj->active && obj->keyframeCount > 0) {
                ObjectTransform t;
                keyframe_evaluate(obj, ed->timeline.currentFrame, &t);
                obj->transform = t;
            }
        }
    }

    // camera (disable orbit/pan when in placement mode so clicks go to placement)
    editor_camera_update(&ed->camera,
        ed->ui.viewportHovered && !ImGui::GetIO().WantCaptureKeyboard,
        ed->ui.vpImageX, ed->ui.vpImageY, ed->ui.vpImageW, ed->ui.vpImageH);

    // Ctrl+S save shortcut
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S)) {
        ed->ui.wantSave = true;
    }

    // handle save request
    if (ed->ui.wantSave) {
        ed->ui.wantSave = false;
        char savePath[768];
        if (ed->ui.currentFilePath[0]) {
            // absolute path or path set by Open/Save As
            if (ed->ui.currentFilePath[0] == '/') {
                snprintf(savePath, sizeof(savePath), "%s", ed->ui.currentFilePath);
            } else {
                const char *appDir = GetApplicationDirectory();
                snprintf(savePath, sizeof(savePath), "%s%s", appDir, ed->ui.currentFilePath);
            }
        } else {
            const char *appDir = GetApplicationDirectory();
            snprintf(savePath, sizeof(savePath), "%s%s", appDir, ed->ui.saveAsName);
        }
        if (editor_save(ed, savePath)) {
            snprintf(ed->ui.currentFilePath, sizeof(ed->ui.currentFilePath), "%s", savePath);
            snprintf(ed->ui.errorMessage, sizeof(ed->ui.errorMessage), "Saved to %s", savePath);
        } else {
            snprintf(ed->ui.errorMessage, sizeof(ed->ui.errorMessage), "Failed to save!");
        }
        ed->ui.showErrorPopup = true;
    }

    // Ctrl+Z undo, Ctrl+Y redo
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
        undo_perform(ed);
    }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
        redo_perform(ed);
    }

    // clipboard: keyboard shortcuts set flags, same as UI menu
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_C) && ed->scene.selectedCount > 0) ed->ui.wantCopy = true;
    if (ctrl && IsKeyPressed(KEY_X) && ed->scene.selectedCount > 0) ed->ui.wantCut = true;
    if (ctrl && IsKeyPressed(KEY_V) && ed->clipboard.count > 0)     ed->ui.wantPaste = true;
    if (ctrl && IsKeyPressed(KEY_D) && ed->scene.selectedCount > 0 && !ImGui::GetIO().WantCaptureKeyboard)
        ed->ui.wantDuplicate = true;

    // helper: copy selected objects to clipboard
    auto clipboard_copy = [&]() {
        ed->clipboard.count = 0;
        for (int i = 0; i < ed->scene.objectCount && ed->clipboard.count < MAX_SELECTED; i++) {
            if (scene_is_selected(&ed->scene, ed->scene.objects[i].id)) {
                ed->clipboard.objects[ed->clipboard.count] = ed->scene.objects[i];
                ed->clipboard.objects[ed->clipboard.count].material.texture = {};
                if (ed->scene.objects[i].modelLoaded) {
                    ed->clipboard.objects[ed->clipboard.count].model = {};
                    ed->clipboard.objects[ed->clipboard.count].modelLoaded = false;
                }
                ed->clipboard.count++;
            }
        }
        ed->ui.hasClipboard = ed->clipboard.count > 0;
    };

    // Copy
    if (ed->ui.wantCopy) {
        ed->ui.wantCopy = false;
        if (ed->scene.selectedCount > 0) {
            ed->clipboard.isCut = false;
            clipboard_copy();
        }
    }
    // Cut
    if (ed->ui.wantCut) {
        ed->ui.wantCut = false;
        if (ed->scene.selectedCount > 0) {
            undo_push(ed);
            ed->clipboard.isCut = true;
            clipboard_copy();
            uint32_t ids[MAX_SELECTED];
            int count = ed->scene.selectedCount;
            for (int i = 0; i < count; i++) ids[i] = ed->scene.selectedIds[i];
            for (int i = 0; i < count; i++) {
                int idx = scene_find_by_id(&ed->scene, ids[i]);
                if (idx >= 0) scene_remove(&ed->scene, idx);
            }
        }
    }
    // Paste
    if (ed->ui.wantPaste) {
        ed->ui.wantPaste = false;
        if (ed->clipboard.count > 0) {
            undo_push(ed);
            Vector3 pastePos = {0, 0, 0};
            bool hasPastePos = false;
            Vector2 mouse = GetMousePosition();
            float localX = (mouse.x - ed->ui.vpImageX) / ed->ui.vpImageW;
            float localY = (mouse.y - ed->ui.vpImageY) / ed->ui.vpImageH;
            if (localX >= 0 && localX <= 1 && localY >= 0 && localY <= 1) {
                Vector2 rtMouse = { localX * (float)ed->ui.viewportW, localY * (float)ed->ui.viewportH };
                Ray ray = GetScreenToWorldRayEx(rtMouse, ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
                RayHitResult hit = raycast_scene(&ed->scene, ray);
                if (hit.hit && hit.distance < 50.0f) {
                    pastePos = hit.point;
                    hasPastePos = true;
                } else if (ray.direction.y != 0.0f) {
                    float t = -ray.position.y / ray.direction.y;
                    if (t > 0.0f && t < 50.0f) {
                        pastePos = Vector3{ ray.position.x + ray.direction.x * t, 0.0f,
                                            ray.position.z + ray.direction.z * t };
                        hasPastePos = true;
                    }
                }
            }
            if (!hasPastePos) {
                Vector3 fwd = Vector3Normalize(Vector3Subtract(ed->camera.cam.target, ed->camera.cam.position));
                pastePos = Vector3Add(ed->camera.cam.position, Vector3Scale(fwd, 10.0f));
            }
            Vector3 clipCenter = {0, 0, 0};
            for (int i = 0; i < ed->clipboard.count; i++)
                clipCenter = Vector3Add(clipCenter, ed->clipboard.objects[i].transform.position);
            clipCenter = Vector3Scale(clipCenter, 1.0f / ed->clipboard.count);

            scene_deselect_all(&ed->scene);
            for (int i = 0; i < ed->clipboard.count; i++) {
                if (ed->scene.objectCount >= MAX_OBJECTS) break;
                int idx = ed->scene.objectCount++;
                ed->scene.objects[idx] = ed->clipboard.objects[i];
                ed->scene.objects[idx].id = ed->scene.nextId++;
                Vector3 offset = Vector3Subtract(ed->clipboard.objects[i].transform.position, clipCenter);
                ed->scene.objects[idx].transform.position = Vector3Add(pastePos, offset);
                char unique[MAX_NAME_LEN];
                scene_generate_name(&ed->scene, ed->clipboard.objects[i].name, unique, MAX_NAME_LEN);
                snprintf(ed->scene.objects[idx].name, MAX_NAME_LEN, "%s", unique);
                if (ed->clipboard.objects[i].material.texturePath[0])
                    object_reload_texture(&ed->scene.objects[idx]);
                ed->scene.objects[idx].parentIndex = -1;
                scene_select(&ed->scene, ed->scene.objects[idx].id, false, true);
            }
        }
    }
    // Duplicate
    if (ed->ui.wantDuplicate) {
        ed->ui.wantDuplicate = false;
        if (ed->scene.selectedCount > 0) {
            undo_push(ed);
            SceneObject copies[MAX_SELECTED];
            int copyCount = 0;
            for (int i = 0; i < ed->scene.objectCount && copyCount < MAX_SELECTED; i++) {
                if (scene_is_selected(&ed->scene, ed->scene.objects[i].id)) {
                    copies[copyCount] = ed->scene.objects[i];
                    copies[copyCount].material.texture = {};
                    if (ed->scene.objects[i].modelLoaded) {
                        copies[copyCount].model = {};
                        copies[copyCount].modelLoaded = false;
                    }
                    copyCount++;
                }
            }
            scene_deselect_all(&ed->scene);
            for (int i = 0; i < copyCount; i++) {
                if (ed->scene.objectCount >= MAX_OBJECTS) break;
                int idx = ed->scene.objectCount++;
                ed->scene.objects[idx] = copies[i];
                ed->scene.objects[idx].id = ed->scene.nextId++;
                ed->scene.objects[idx].transform.position.x += 1.0f;
                ed->scene.objects[idx].transform.position.y += 1.0f;
                ed->scene.objects[idx].transform.position.z += 1.0f;
                char unique[MAX_NAME_LEN];
                scene_generate_name(&ed->scene, copies[i].name, unique, MAX_NAME_LEN);
                snprintf(ed->scene.objects[idx].name, MAX_NAME_LEN, "%s", unique);
                if (copies[i].material.texturePath[0])
                    object_reload_texture(&ed->scene.objects[idx]);
                ed->scene.objects[idx].parentIndex = -1;
                scene_select(&ed->scene, ed->scene.objects[idx].id, false, true);
            }
        }
    }

    // transform mode shortcuts
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (IsKeyPressed(KEY_T)) ed->ui.transformMode = TMODE_TRANSLATE;
        if (IsKeyPressed(KEY_R)) ed->ui.transformMode = TMODE_ROTATE;
        if (IsKeyPressed(KEY_Y) && !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL))
            ed->ui.transformMode = TMODE_SCALE;
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

            // 1) try raycast against scene geometry
            float maxDist = 50.0f;
            RayHitResult hit = raycast_scene(&ed->scene, ray);
            if (hit.hit && hit.distance < maxDist) {
                // place on surface with small offset along normal
                ed->ui.placementPos = Vector3{
                    hit.point.x + hit.normal.x * 0.01f,
                    hit.point.y + hit.normal.y * 0.01f,
                    hit.point.z + hit.normal.z * 0.01f
                };
                ed->ui.placementValid = true;
            }
            // 2) fall back to ground plane y=0
            else if (ray.direction.y != 0.0f) {
                float t = -ray.position.y / ray.direction.y;
                if (t > 0.0f && t < maxDist) {
                    ed->ui.placementPos = Vector3{
                        ray.position.x + ray.direction.x * t,
                        0.0f,
                        ray.position.z + ray.direction.z * t
                    };
                    ed->ui.placementValid = true;
                } else {
                    // ground too far or behind camera — fixed distance along ray
                    float d = (t > 0.0f) ? maxDist : 10.0f;
                    ed->ui.placementPos = Vector3{
                        ray.position.x + ray.direction.x * d,
                        ray.position.y + ray.direction.y * d,
                        ray.position.z + ray.direction.z * d
                    };
                    ed->ui.placementValid = true;
                }
            } else {
                // ray parallel to ground — fixed distance along ray
                ed->ui.placementPos = Vector3{
                    ray.position.x + ray.direction.x * 10.0f,
                    ray.position.y + ray.direction.y * 10.0f,
                    ray.position.z + ray.direction.z * 10.0f
                };
                ed->ui.placementValid = true;
            }

            // left-click to place
            if (ed->ui.placementValid && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const char *names[] = {
                    "None", "Cube", "Sphere", "Plane",
                    "Cylinder", "Cone", "Torus", "Knot", "Capsule", "Polygon",
                    "Teapot", "Camera", "Light", "Model"
                };
                undo_push(ed);
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
            // sync transform to keyframe if on one
            for (int di = 0; di < ed->ui.gizmoDragCount; di++) {
                SceneObject *obj = scene_get_by_id(&ed->scene, ed->ui.gizmoDragIds[di]);
                if (obj && obj->keyframeCount > 0) {
                    keyframe_sync(obj, ed->timeline.currentFrame);
                }
            }
            ed->ui.gizmoDragging = false;
            ed->ui.gizmoActiveAxis = GIZMO_NONE;
        } else {
            Vector2 mouse = GetMousePosition();
            float dx = mouse.x - ed->ui.gizmoDragStart.x;
            float dy = mouse.y - ed->ui.gizmoDragStart.y;
            float sensitivity = ed->camera.distance / (float)ed->ui.viewportH;

            // project world axis onto screen to get correct drag direction
            auto axis_screen_dir = [&](Vector3 worldAxis) -> Vector2 {
                Vector3 p = ed->ui.gizmoDragCenter;
                Vector2 s0 = GetWorldToScreenEx(p, ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
                Vector2 s1 = GetWorldToScreenEx(Vector3Add(p, worldAxis), ed->camera.cam, ed->ui.viewportW, ed->ui.viewportH);
                Vector2 dir = { s1.x - s0.x, s1.y - s0.y };
                float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if (len > 0.001f) { dir.x /= len; dir.y /= len; }
                return dir;
            };

            // compute delta once
            float delta = 0;
            if (ed->ui.transformMode == TMODE_TRANSLATE || ed->ui.transformMode == TMODE_SCALE) {
                Vector3 worldAxis = {0};
                if (ed->ui.gizmoActiveAxis == GIZMO_X) worldAxis = Vector3{1, 0, 0};
                else if (ed->ui.gizmoActiveAxis == GIZMO_Y) worldAxis = Vector3{0, 1, 0};
                else if (ed->ui.gizmoActiveAxis == GIZMO_Z) worldAxis = Vector3{0, 0, 1};
                Vector2 screenDir = axis_screen_dir(worldAxis);
                delta = (dx * screenDir.x + dy * screenDir.y) * sensitivity;
            } else if (ed->ui.transformMode == TMODE_ROTATE) {
                delta = (dx + dy) * 0.5f;
            }

            // apply to all dragged objects
            for (int di = 0; di < ed->ui.gizmoDragCount; di++) {
                SceneObject *obj = scene_get_by_id(&ed->scene, ed->ui.gizmoDragIds[di]);
                if (!obj) continue;
                Vector3 start = ed->ui.gizmoDragStarts[di];

                if (ed->ui.transformMode == TMODE_TRANSLATE) {
                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.position.x = start.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.position.y = start.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.position.z = start.z + delta;
                } else if (ed->ui.transformMode == TMODE_ROTATE) {
                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.rotation.x = start.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.rotation.y = start.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.rotation.z = start.z + delta;
                } else if (ed->ui.transformMode == TMODE_SCALE) {
                    if (ed->ui.gizmoActiveAxis == GIZMO_X)
                        obj->transform.scale.x = start.x + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Y)
                        obj->transform.scale.y = start.y + delta;
                    else if (ed->ui.gizmoActiveAxis == GIZMO_Z)
                        obj->transform.scale.z = start.z + delta;
                }
            }
        }
    }

    // viewport click: gizmo pick first, then object select
    if (!ed->ui.gizmoDragging && !ed->ui.placementMode
        && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        && !IsKeyDown(KEY_LEFT_ALT)) {
        Vector2 mouse = GetMousePosition();
        float localX = (mouse.x - ed->ui.vpImageX) / ed->ui.vpImageW;
        float localY = (mouse.y - ed->ui.vpImageY) / ed->ui.vpImageH;
        bool inVP = localX >= 0 && localX <= 1 && localY >= 0 && localY <= 1;
        if (inVP) {
            // try gizmo first
            bool gizmoHit = false;
            if (ed->scene.selectedCount > 0) {
                // compute selection center and max radius
                Vector3 center = {0, 0, 0};
                float maxRadius = 0;
                int selCount = 0;
                for (int i = 0; i < ed->scene.objectCount; i++) {
                    if (scene_is_selected(&ed->scene, ed->scene.objects[i].id)) {
                        center = Vector3Add(center, ed->scene.objects[i].transform.position);
                        float r = object_radius(&ed->scene.objects[i]);
                        if (r > maxRadius) maxRadius = r;
                        selCount++;
                    }
                }
                center = Vector3Scale(center, 1.0f / selCount);

                Ray ray = viewport_ray(mouse);
                GizmoAxis axis = gizmo_hit_test(center, maxRadius, ray, ed->ui.transformMode, ed->camera.cam.position);
                if (axis != GIZMO_NONE) {
                    undo_push(ed);
                    ed->ui.gizmoActiveAxis = axis;
                    ed->ui.gizmoDragging = true;
                    ed->ui.gizmoDragStart = mouse;
                    ed->ui.gizmoDragCenter = center;

                    // store per-object start values
                    ed->ui.gizmoDragCount = 0;
                    for (int i = 0; i < ed->scene.objectCount && ed->ui.gizmoDragCount < MAX_SELECTED; i++) {
                        if (scene_is_selected(&ed->scene, ed->scene.objects[i].id)) {
                            int di = ed->ui.gizmoDragCount;
                            ed->ui.gizmoDragIds[di] = ed->scene.objects[i].id;
                            if (ed->ui.transformMode == TMODE_TRANSLATE)
                                ed->ui.gizmoDragStarts[di] = ed->scene.objects[i].transform.position;
                            else if (ed->ui.transformMode == TMODE_ROTATE)
                                ed->ui.gizmoDragStarts[di] = ed->scene.objects[i].transform.rotation;
                            else
                                ed->ui.gizmoDragStarts[di] = ed->scene.objects[i].transform.scale;
                            ed->ui.gizmoDragCount++;
                        }
                    }
                    // keep first selected for axis_screen_dir reference
                    ed->ui.gizmoDragObjStart = ed->ui.gizmoDragStarts[0];
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
        undo_push(ed);
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

    // shadow pass: render depth from the first shadow-casting light
    bool hasShadow = false;
    bool shadowIsPoint = false;
    int shadowLightIdx = 0;
    Vector3 shadowLightPos = {0};
    Vector3 shadowViewDir = {0};
    if (ed->shadowMap.initialized) {
        // compute scene bounds from all geometry objects
        Vector3 sceneCenter = {0, 0, 0};
        float sceneRadius = 10.0f;
        {
            Vector3 bmin = { 1e9f,  1e9f,  1e9f};
            Vector3 bmax = {-1e9f, -1e9f, -1e9f};
            int geomCount = 0;
            for (int i = 0; i < ed->scene.objectCount; i++) {
                const SceneObject *obj = &ed->scene.objects[i];
                if (!obj->active || !obj->visible) continue;
                if (obj->type == OBJ_LIGHT || obj->type == OBJ_CAMERA) continue;
                BoundingBox bb = scene_get_bounds(&ed->scene, i);
                if (bb.min.x < bmin.x) bmin.x = bb.min.x;
                if (bb.min.y < bmin.y) bmin.y = bb.min.y;
                if (bb.min.z < bmin.z) bmin.z = bb.min.z;
                if (bb.max.x > bmax.x) bmax.x = bb.max.x;
                if (bb.max.y > bmax.y) bmax.y = bb.max.y;
                if (bb.max.z > bmax.z) bmax.z = bb.max.z;
                geomCount++;
            }
            if (geomCount > 0) {
                sceneCenter = Vector3{
                    (bmin.x + bmax.x) * 0.5f,
                    (bmin.y + bmax.y) * 0.5f,
                    (bmin.z + bmax.z) * 0.5f};
                float dx = bmax.x - bmin.x;
                float dy = bmax.y - bmin.y;
                float dz = bmax.z - bmin.z;
                sceneRadius = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f + 5.0f;
                if (sceneRadius < 15.0f) sceneRadius = 15.0f;
            }
        }

        // directional and spot lights cast shadows
        for (int i = 0; i < ed->lighting.lightCount; i++) {
            LightData *ld = &ed->lighting.lights[i];
            if (ld->type == LIGHT_DIRECTIONAL) {
                shadowmap_begin(&ed->shadowMap, ld->position, ld->direction, sceneCenter, sceneRadius);
                rlDisableBackfaceCulling();
                BeginShaderMode(ed->shadowMap.depthShader);
                scene_draw(&ed->scene, DRAW_SOLID, NULL);
                EndShaderMode();
                rlEnableBackfaceCulling();
                shadowmap_end(&ed->shadowMap);
                hasShadow = true;
                shadowLightIdx = i;
                break;
            }
            if (ld->type == LIGHT_SPOT) {
                float outerDeg = acosf(ld->spotOuterCos) * RAD2DEG;
                shadowmap_begin_spot(&ed->shadowMap, ld->position, ld->direction, outerDeg, sceneRadius);
                rlDisableBackfaceCulling();
                BeginShaderMode(ed->shadowMap.depthShader);
                scene_draw(&ed->scene, DRAW_SOLID, NULL);
                EndShaderMode();
                rlEnableBackfaceCulling();
                shadowmap_end(&ed->shadowMap);
                hasShadow = true;
                shadowLightIdx = i;
                break;
            }
        }
    }
    lighting_bind_shadow(&ed->lighting, hasShadow ? &ed->shadowMap : NULL, shadowLightIdx, shadowIsPoint, shadowLightPos, shadowViewDir);

    // helper lambda: render 3D scene into a given render texture
    auto renderScene = [&](RenderTexture2D rt, bool showGizmos) {
        BeginTextureMode(rt);
            ClearBackground(DARKGRAY);
            BeginMode3D(activeCam);
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
                scene_draw(&ed->scene, ed->ui.drawMode, &ed->lighting);
                if (showGizmos) {
                    scene_draw_selection(&ed->scene);
                    scene_draw_gizmo(&ed->scene, ed->ui.transformMode, ed->ui.gizmoActiveAxis, activeCam.position);
                    if (ed->ui.placementMode && ed->ui.placementValid) {
                        scene_draw_preview(ed->ui.placementType, ed->ui.placementPos);
                    }
                }
            EndMode3D();
        EndTextureMode();
    };

    if (ed->playMode) {
        // save current animated state
        ObjectSnapshot animState[MAX_OBJECTS];
        int animCount = ed->scene.objectCount;
        for (int i = 0; i < animCount; i++) {
            animState[i].transform = ed->scene.objects[i].transform;
            animState[i].color = ed->scene.objects[i].material.color;
            animState[i].visible = ed->scene.objects[i].visible;
            animState[i].velocity = ed->scene.objects[i].velocity;
        }

        // render edit viewport with snapshot (original) transforms
        editor_restore(ed);
        renderScene(ed->ui.viewportRT, true);

        // restore animated state and render animation viewport
        for (int i = 0; i < animCount && i < ed->scene.objectCount; i++) {
            ed->scene.objects[i].transform = animState[i].transform;
            ed->scene.objects[i].material.color = animState[i].color;
            ed->scene.objects[i].visible = animState[i].visible;
            ed->scene.objects[i].velocity = animState[i].velocity;
        }
        renderScene(ed->ui.animViewportRT, false);
    } else {
        renderScene(ed->ui.viewportRT, true);
        // always render animation viewport so it shows the scene when the tab is active
        renderScene(ed->ui.animViewportRT, false);
    }

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
            ui_properties(&ed->scene, &ed->timeline, &ed->ui);
            ui_timeline(&ed->scene, &ed->timeline, &ed->ui);
            ui_console(&ed->scripting, &ed->ui);
            ui_save_as_popup(&ed->ui);
            ui_error_popup(&ed->ui);
            ui_shortcut_popup();
        rlImGuiEnd();
    EndDrawing();
}

bool editor_save(Editor *ed, const char *path) {
    EditorState state = { &ed->scene, &ed->camera, &ed->timeline, &ed->scripting, &ed->ui };
    int len = (int)strlen(path);
    if (len > 7 && strcmp(path + len - 7, ".sceneb") == 0)
        return save_binary(path, &state);
    return save_text(path, &state);
}

bool editor_load(Editor *ed, const char *path) {
    EditorState state = { &ed->scene, &ed->camera, &ed->timeline, &ed->scripting, &ed->ui };
    int len = (int)strlen(path);
    if (len > 7 && strcmp(path + len - 7, ".sceneb") == 0)
        return load_binary(path, &state);
    return load_text(path, &state);
}

void editor_shutdown(Editor *ed) {
    for (int i = 0; i < ed->scene.objectCount; i++) {
        SceneObject *obj = &ed->scene.objects[i];
        if (obj->modelLoaded) UnloadModel(obj->model);
        if (obj->material.hasTexture) UnloadTexture(obj->material.texture);
    }
    // free undo states
    for (int i = 0; i < ed->undo.count; i++) {
        delete ed->undo.states[i];
    }
    script_shutdown(&ed->scripting);
    lighting_shutdown(&ed->lighting);
    shadowmap_unload(&ed->shadowMap);
    ui_shutdown(&ed->ui);
    rlImGuiShutdown();
    CloseWindow();
}

// ---- Undo/Redo ----

void undo_push(Editor *ed) {
    // discard any redo states beyond current
    for (int i = ed->undo.current + 1; i < ed->undo.count; i++) {
        delete ed->undo.states[i];
        ed->undo.states[i] = nullptr;
    }
    ed->undo.count = ed->undo.current + 1;

    // if at max capacity, shift everything down
    if (ed->undo.count >= UNDO_MAX_LEVELS) {
        delete ed->undo.states[0];
        for (int i = 1; i < ed->undo.count; i++) {
            ed->undo.states[i - 1] = ed->undo.states[i];
        }
        ed->undo.count--;
    }

    // save current scene state
    UndoState *state = new UndoState();
    memcpy(&state->scene, &ed->scene, sizeof(Scene));
    // clear GPU handles in snapshot so they aren't double-freed
    for (int i = 0; i < state->scene.objectCount; i++) {
        state->scene.objects[i].model = Model{0};
        state->scene.objects[i].modelLoaded = false;
        state->scene.objects[i].material.texture = Texture2D{0};
        state->scene.objects[i].material.hasTexture = false;
    }

    ed->undo.states[ed->undo.count] = state;
    ed->undo.count++;
    ed->undo.current = ed->undo.count - 1;
}

void undo_perform(Editor *ed) {
    if (ed->undo.current < 0) return;

    // save current state as redo point if we're at the top
    if (ed->undo.current == ed->undo.count - 1) {
        // push current state so we can redo back to it
        if (ed->undo.count < UNDO_MAX_LEVELS) {
            UndoState *state = new UndoState();
            memcpy(&state->scene, &ed->scene, sizeof(Scene));
            for (int i = 0; i < state->scene.objectCount; i++) {
                state->scene.objects[i].model = Model{0};
                state->scene.objects[i].modelLoaded = false;
                state->scene.objects[i].material.texture = Texture2D{0};
                state->scene.objects[i].material.hasTexture = false;
            }
            ed->undo.states[ed->undo.count] = state;
            ed->undo.count++;
        }
    }

    // restore from current undo state
    UndoState *state = ed->undo.states[ed->undo.current];

    // restore scene data fields (preserve GPU handles)
    for (int i = 0; i < MAX_OBJECTS; i++) {
        Model m = ed->scene.objects[i].model;
        bool ml = ed->scene.objects[i].modelLoaded;
        Texture2D tex = ed->scene.objects[i].material.texture;
        bool ht = ed->scene.objects[i].material.hasTexture;

        ed->scene.objects[i] = state->scene.objects[i];

        // restore GPU handles
        ed->scene.objects[i].model = m;
        ed->scene.objects[i].modelLoaded = ml;
        ed->scene.objects[i].material.texture = tex;
        ed->scene.objects[i].material.hasTexture = ht;

        // reload texture if path changed
        if (i < state->scene.objectCount && state->scene.objects[i].material.texturePath[0]) {
            if (!ht || strcmp(ed->scene.objects[i].material.texturePath,
                             state->scene.objects[i].material.texturePath) != 0) {
                // path differs, need to reload
                strncpy(ed->scene.objects[i].material.texturePath,
                        state->scene.objects[i].material.texturePath, 256);
                if (ht) UnloadTexture(tex);
                ed->scene.objects[i].material.hasTexture = false;
                object_reload_texture(&ed->scene.objects[i]);
            }
        }
    }
    ed->scene.objectCount = state->scene.objectCount;
    ed->scene.nextId = state->scene.nextId;
    memcpy(ed->scene.selectedIds, state->scene.selectedIds, sizeof(state->scene.selectedIds));
    ed->scene.selectedCount = state->scene.selectedCount;

    ed->undo.current--;
}

void redo_perform(Editor *ed) {
    if (ed->undo.current + 2 >= ed->undo.count) return;

    ed->undo.current += 2;
    // reuse undo_perform logic by temporarily adjusting
    UndoState *state = ed->undo.states[ed->undo.current];

    for (int i = 0; i < MAX_OBJECTS; i++) {
        Model m = ed->scene.objects[i].model;
        bool ml = ed->scene.objects[i].modelLoaded;
        Texture2D tex = ed->scene.objects[i].material.texture;
        bool ht = ed->scene.objects[i].material.hasTexture;

        ed->scene.objects[i] = state->scene.objects[i];

        ed->scene.objects[i].model = m;
        ed->scene.objects[i].modelLoaded = ml;
        ed->scene.objects[i].material.texture = tex;
        ed->scene.objects[i].material.hasTexture = ht;

        if (i < state->scene.objectCount && state->scene.objects[i].material.texturePath[0]) {
            if (!ht || strcmp(ed->scene.objects[i].material.texturePath,
                             state->scene.objects[i].material.texturePath) != 0) {
                strncpy(ed->scene.objects[i].material.texturePath,
                        state->scene.objects[i].material.texturePath, 256);
                if (ht) UnloadTexture(tex);
                ed->scene.objects[i].material.hasTexture = false;
                object_reload_texture(&ed->scene.objects[i]);
            }
        }
    }
    ed->scene.objectCount = state->scene.objectCount;
    ed->scene.nextId = state->scene.nextId;
    memcpy(ed->scene.selectedIds, state->scene.selectedIds, sizeof(state->scene.selectedIds));
    ed->scene.selectedCount = state->scene.selectedCount;

    ed->undo.current--;
}
