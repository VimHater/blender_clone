#include "editor.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>

static const float BASE_FONT_SIZE = 36.0f;
static const float GLOBAL_FONT_SCALE = 2.0f;

static void rebuild_font(EditorUI *ui, float fontSize) {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(ui->fontPath, fontSize, &fontCfg);
    io.Fonts->Build();
    io.FontGlobalScale = GLOBAL_FONT_SCALE;
    ui->lastFontSize = fontSize;
}

void editor_init(Editor *ed, int screenW, int screenH) {
    // window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "Blender Clone");
    SetTargetFPS(60);

    // core systems (init ui early so fontPath is available)
    scene_init(&ed->scene);
    editor_camera_init(&ed->camera);
    timeline_init(&ed->timeline);
    ui_init(&ed->ui, screenW, screenH);
    shadowmap_init(&ed->shadowMap, 1024);

    // store font path
    snprintf(ed->ui.fontPath, sizeof(ed->ui.fontPath),
             "%s../font/JetBrainsMonoNerdFont-Medium.ttf", GetApplicationDirectory());

    // imgui + font
    rlImGuiBeginInitImGui();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // disable imgui.ini
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    float initSize = BASE_FONT_SIZE * ((float)screenH / 1080.0f) * ed->ui.uiScale;
    io.Fonts->AddFontFromFileTTF(ed->ui.fontPath, initSize, &fontCfg);
    io.FontGlobalScale = 1.0f;
    ed->ui.lastFontSize = initSize;
    rlImGuiEndInitImGui();

    ed->running = true;
}

bool editor_should_close(const Editor *ed) {
    return !ed->running || WindowShouldClose();
}

void editor_update(Editor *ed) {
    // font scaling — rebuild atlas when target pixel size changes
    float targetFontSize = BASE_FONT_SIZE * ((float)GetScreenHeight() / 1080.0f) * ed->ui.uiScale;
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
                    "Cylinder", "Cone", "Torus", "Knot", "Capsule", "Polygon",
                    "Teapot", "Camera", "Model"
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

    // TODO: add viewport click-to-select (raycast picking in render texture)

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
            ui_camera(&ed->scene, &ed->camera, &ed->ui);
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
