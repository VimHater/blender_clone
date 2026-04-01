#include "editor.h"
#include "rlImGui.h"
#include "imgui.h"
#include <cstdio>

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
    rlImGuiEndInitImGui();

    // core systems
    scene_init(&ed->scene);
    editor_camera_init(&ed->camera);
    timeline_init(&ed->timeline);
    ui_init(&ed->ui, screenW, screenH);
    shadowmap_init(&ed->shadowMap, 1024);
    ed->running = true;

    // default scene objects
    int cubeIdx = scene_add(&ed->scene, "Cube", OBJ_CUBE);
    ed->scene.objects[cubeIdx].material.color = WHITE;
    ed->scene.objects[cubeIdx].cubeSize[0] = 2.0f;
    ed->scene.objects[cubeIdx].cubeSize[1] = 2.0f;
    ed->scene.objects[cubeIdx].cubeSize[2] = 2.0f;

    int sphereIdx = scene_add(&ed->scene, "Sphere", OBJ_SPHERE);
    ed->scene.objects[sphereIdx].transform.position = Vector3{0, 0, 3};
    ed->scene.objects[sphereIdx].material.color = RED;
    ed->scene.objects[sphereIdx].sphereRadius = 2.0f;
}

bool editor_should_close(const Editor *ed) {
    return !ed->running || WindowShouldClose();
}

void editor_update(Editor *ed) {
    // font scaling
    ImGui::GetIO().FontGlobalScale = (float)GetScreenHeight() / 1080.0f;

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

    // camera
    bool camInput = ed->ui.viewportHovered && !ImGui::GetIO().WantCaptureMouse;
    editor_camera_update(&ed->camera, camInput);

    // click to select
    if (ed->ui.viewportHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse) {
        Vector2 mouse = GetMousePosition();
        Ray ray = GetScreenToWorldRay(mouse, ed->camera.cam);
        RayHitResult hit = raycast_scene(&ed->scene, ray);
        if (hit.hit) {
            ed->scene.selectedIndex = hit.objectIndex;
        }
    }

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
            if (ed->ui.showGrid) {
                DrawGrid(ed->ui.gridSize, ed->ui.gridSpacing);
            }
            scene_draw(&ed->scene);
        EndMode3D();
    EndTextureMode();

    // UI
    BeginDrawing();
        ClearBackground(Color{30, 30, 30, 255});
        rlImGuiBegin();
            ui_menu_bar(&ed->scene, &ed->camera, &ed->timeline, &ed->ui);
            ui_viewport(&ed->ui);
            ui_hierarchy(&ed->scene, &ed->ui);
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
