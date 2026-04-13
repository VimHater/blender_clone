#include "editor.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>

void editor_init(Editor *ed, int screenW, int screenH) {
    // window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "Blender Clone");
    SetTargetFPS(60);

    // imgui + font
    rlImGuiBeginInitImGui();
    ImGuiIO &io = ImGui::GetIO();
    char fontPath[512];
    snprintf(fontPath, sizeof(fontPath), "%s../font/JetBrainsMonoNerdFont-Medium.ttf", GetApplicationDirectory());
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(fontPath, 36.0f, &fontCfg);
    io.FontGlobalScale = 1.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // disable imgui.ini
    rlImGuiEndInitImGui();

    // core systems
    scene_init(&ed->scene);
    editor_camera_init(&ed->camera);
    timeline_init(&ed->timeline);
    ui_init(&ed->ui, screenW, screenH);
    shadowmap_init(&ed->shadowMap, 1024);
    ed->running = true;
}

bool editor_should_close(const Editor *ed) {
    return !ed->running || WindowShouldClose();
}

void editor_update(Editor *ed) {
    // font scaling
    ImGui::GetIO().FontGlobalScale = ((float)GetScreenHeight() / 1080.0f) * ed->ui.uiScale;

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
            Ray ray = GetScreenToWorldRay(rtMouse, ed->camera.cam);

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
                    "None", "Cube", "Sphere", "HemiSphere", "Plane",
                    "Cylinder", "Cone", "Torus", "Knot", "Capsule", "Polygon", "Model"
                };
                int idx = scene_add(&ed->scene, names[ed->ui.placementType], ed->ui.placementType);
                if (idx >= 0) {
                    ed->scene.objects[idx].transform.position = ed->ui.placementPos;
                    ed->scene.selectedIndex = idx;
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

    // TODO: add viewport click-to-select (raycast picking in render texture)

    // delete key
    if (ed->scene.selectedIndex >= 0 && IsKeyPressed(KEY_DELETE) && !ImGui::GetIO().WantCaptureKeyboard) {
        scene_remove(&ed->scene, ed->scene.selectedIndex);
    }
}

void editor_draw(Editor *ed) {
    ui_update_layout(&ed->ui);

    // 3D scene to render texture
    BeginTextureMode(ed->ui.viewportRT);
        ClearBackground(DARKGRAY);
        BeginMode3D(ed->camera.cam);
            // override near/far clipping planes
            {
                double aspect = (double)ed->ui.viewportW / (double)ed->ui.viewportH;
                Matrix proj;
                if (ed->camera.ortho) {
                    double top = ed->camera.fov / 2.0;
                    double right = top * aspect;
                    proj = MatrixOrtho(-right, right, -top, top,
                                       ed->camera.nearPlane, ed->camera.farPlane);
                } else {
                    proj = MatrixPerspective(ed->camera.fov * DEG2RAD, aspect,
                                             ed->camera.nearPlane, ed->camera.farPlane);
                }
                rlSetMatrixProjection(proj);
            }
            if (ed->ui.showGrid) {
                DrawGrid(ed->ui.gridSize, ed->ui.gridSpacing);
            }
            scene_draw(&ed->scene);
            scene_draw_selection(&ed->scene);
            if (ed->ui.placementMode && ed->ui.placementValid) {
                scene_draw_preview(ed->ui.placementType, ed->ui.placementPos);
            }
        EndMode3D();
    EndTextureMode();

    // UI
    BeginDrawing();
        ClearBackground(Color{30, 30, 30, 255});
        rlImGuiBegin();
            ui_menu_bar(&ed->scene, &ed->camera, &ed->timeline, &ed->ui);
            ui_dockspace(&ed->ui);
            ui_viewport(&ed->ui);
            ui_hierarchy(&ed->scene, &ed->ui);
            ui_add_object(&ed->scene, &ed->ui);
            ui_camera(&ed->camera, &ed->ui);
            ui_properties(&ed->scene, &ed->ui);
            ui_timeline(&ed->scene, &ed->timeline, &ed->ui);
        rlImGuiEnd();
    EndDrawing();
}

void editor_shutdown(Editor *ed) {
    for (int i = 0; i < ed->scene.objectCount; i++) {
        SceneObject *obj = &ed->scene.objects[i];
        if (obj->modelLoaded) UnloadModel(obj->model);
        if (obj->material.hasTexture) UnloadTexture(obj->material.texture);
    }
    shadowmap_unload(&ed->shadowMap);
    ui_shutdown(&ed->ui);
    rlImGuiShutdown();
    CloseWindow();
}
