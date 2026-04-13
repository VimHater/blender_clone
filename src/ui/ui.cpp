#include "ui.h"
#include <rlImGui.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstdint>
#include "builtin_textures/checkerboard.h"
#include "builtin_textures/brick.h"
#include "builtin_textures/sand.h"
#include "builtin_textures/brushed_metal.h"
#include "builtin_textures/wood.h"
#include "builtin_textures/grass.h"

// ---- Init / Shutdown ----

void ui_init(EditorUI *ui, int vpW, int vpH) {
    ui->viewportW = vpW;
    ui->viewportH = vpH;
    ui->viewportRT = LoadRenderTexture(vpW, vpH);
    ui->viewportHovered = false;
    ui->viewportFocused = false;
    ui->uiScale = 1.0f;
    ui->showGrid = true;
    ui->gridSize = 10;
    ui->gridSpacing = 1.0f;
    ui->transformMode = TMODE_TRANSLATE;
    ui->drawMode = DRAW_SOLID;
    ui->showTimeline = true;
    ui->showHierarchy = true;
    ui->showProperties = true;
    ui->showAddObject = true;
    ui->showCamera = true;
    ui->dockspaceInitialized = false;
    ui->activeCameraId = 0;
    ui->showErrorPopup = false;
    ui->errorMessage[0] = '\0';
    ui->gizmoActiveAxis = GIZMO_NONE;
    ui->gizmoDragging = false;
    ui->placementMode = false;
    ui->placementValid = false;
    ui->vpImageX = ui->vpImageY = 0;
    ui->vpImageW = ui->vpImageH = 0;
}

void ui_update_layout(EditorUI *ui) {
    // render texture is now resized in ui_viewport to match panel size
    (void)ui;
}

void ui_shutdown(EditorUI *ui) {
    UnloadRenderTexture(ui->viewportRT);
}

// ---- Dockspace ----

void ui_dockspace(EditorUI *ui) {
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockspaceWindow", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockId = ImGui::GetID("MainDockspace");

    // build default layout once
    if (!ui->dockspaceInitialized) {
        ui->dockspaceInitialized = true;
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

        ImGuiID dockMain = dockId;
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.15f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);

        // split left: hierarchy on top, add object on bottom
        ImGuiID dockLeftTop, dockLeftBottom;
        ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.45f, &dockLeftBottom, &dockLeftTop);

        // split right: properties on top, camera on bottom
        ImGuiID dockRightTop, dockRightBottom;
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, &dockRightBottom, &dockRightTop);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeftTop);
        ImGui::DockBuilderDockWindow("Add Object", dockLeftBottom);
        ImGui::DockBuilderDockWindow("Properties", dockRightTop);
        ImGui::DockBuilderDockWindow("Camera", dockRightBottom);
        ImGui::DockBuilderDockWindow("Timeline", dockBottom);
        ImGui::DockBuilderDockWindow("Viewport", dockMain);

        // hide tab bar on viewport node
        ImGuiDockNode *vpNode = ImGui::DockBuilderGetNode(dockMain);
        if (vpNode) vpNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

        ImGui::DockBuilderFinish(dockId);
    }

    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();
}

// ---- Error Popup ----

void ui_error_popup(EditorUI *ui) {
    if (ui->showErrorPopup) {
        ImGui::OpenPopup("Error");
        ui->showErrorPopup = false;
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(600, 300));
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", ui->errorMessage);
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---- Menu Bar ----

void ui_menu_bar(Scene *s, EditorCamera *ec, Timeline *tl, EditorUI *ui) {
    (void)ec; (void)tl;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                CloseWindow();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &ui->showHierarchy);
            ImGui::MenuItem("Properties", nullptr, &ui->showProperties);
            ImGui::MenuItem("Add Object", nullptr, &ui->showAddObject);
            ImGui::MenuItem("Camera", nullptr, &ui->showCamera);
            ImGui::MenuItem("Timeline", nullptr, &ui->showTimeline);
            ImGui::MenuItem("Grid", nullptr, &ui->showGrid);
            ImGui::Separator();
            const char *drawModeNames[] = { "Solid", "Wireframe", "Point" };
            int dm = (int)ui->drawMode;
            if (ImGui::Combo("Draw Mode", &dm, drawModeNames, 3)) {
                ui->drawMode = (DrawMode)dm;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ---- Viewport ----

void ui_viewport(EditorUI *ui) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");
    ui->viewportHovered = ImGui::IsWindowHovered();
    ui->viewportFocused = ImGui::IsWindowFocused();

    ImVec2 cpos = ImGui::GetCursorScreenPos();
    ImVec2 csize = ImGui::GetContentRegionAvail();
    int panelW = (int)csize.x;
    int panelH = (int)csize.y;
    if (panelW < 1) panelW = 1;
    if (panelH < 1) panelH = 1;

    // resize render texture to match panel exactly
    if (panelW != ui->viewportW || panelH != ui->viewportH) {
        UnloadRenderTexture(ui->viewportRT);
        ui->viewportRT = LoadRenderTexture(panelW, panelH);
        ui->viewportW = panelW;
        ui->viewportH = panelH;
    }

    // image fills panel 1:1 — no letterboxing
    ui->vpImageX = cpos.x;
    ui->vpImageY = cpos.y;
    ui->vpImageW = (float)panelW;
    ui->vpImageH = (float)panelH;

    rlImGuiImageRenderTextureFit(&ui->viewportRT, true);
    ImGui::End();
    ImGui::PopStyleVar();
}

// ---- Hierarchy ----

// track rename state
static uint32_t s_renameId = 0;
static char s_renameBuf[MAX_NAME_LEN] = {};
// track last clicked index for shift-select range
static int s_lastClickedIndex = -1;

static void hierarchy_draw_node(Scene *s, int index, EditorUI *ui) {
    SceneObject *obj = &s->objects[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (scene_is_selected(s, obj->id)) flags |= ImGuiTreeNodeFlags_Selected;

    bool hasChildren = false;
    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == index) { hasChildren = true; break; }
    }
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    // if renaming this object, show input instead of label
    bool renaming = (s_renameId == obj->id && s_renameId != 0);
    bool open;
    if (renaming) {
        open = ImGui::TreeNodeEx((void *)(intptr_t)obj->id, flags, "");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##rename", s_renameBuf, MAX_NAME_LEN,
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (s_renameBuf[0] != '\0') {
                snprintf(obj->name, MAX_NAME_LEN, "%s", s_renameBuf);
            }
            s_renameId = 0;
        }
        // cancel on escape or click elsewhere
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))) {
            s_renameId = 0;
        }
    } else {
        open = ImGui::TreeNodeEx((void *)(intptr_t)obj->id, flags, "%s", obj->name);

        // double-click to rename
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            s_renameId = obj->id;
            snprintf(s_renameBuf, MAX_NAME_LEN, "%s", obj->name);
        }

        // single click for selection
        if (ImGui::IsItemClicked(0) && !ImGui::IsMouseDoubleClicked(0)) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;
            if (shift && s_lastClickedIndex >= 0) {
                scene_select_range(s, s_lastClickedIndex, index);
            } else {
                scene_select(s, obj->id, ctrl, ctrl);
            }
            s_lastClickedIndex = index;
        }
    }

    if (open) {
        for (int i = 0; i < s->objectCount; i++) {
            if (s->objects[i].parentIndex == index) {
                hierarchy_draw_node(s, i, ui);
            }
        }
        ImGui::TreePop();
    }
}

static void hierarchy_context_add(Scene *s, const char *label, const char *name, ObjectType type) {
    if (ImGui::MenuItem(label)) {
        int idx = scene_add(s, name, type);
        if (idx >= 0) {
            scene_deselect_all(s);
            scene_select(s, s->objects[idx].id, false, false);
        }
    }
}

void ui_hierarchy(Scene *s, EditorUI *ui) {
    if (!ui->showHierarchy) return;
    ImGui::Begin("Scene Hierarchy", &ui->showHierarchy);

    // click empty space to deselect
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
        scene_deselect_all(s);
    }

    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == -1) {
            hierarchy_draw_node(s, i, ui);
        }
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
        hierarchy_context_add(s, "Add Cube",       "Cube",       OBJ_CUBE);
        hierarchy_context_add(s, "Add Sphere",     "Sphere",     OBJ_SPHERE);
        hierarchy_context_add(s, "Add Plane",      "Plane",      OBJ_PLANE);
        hierarchy_context_add(s, "Add Cylinder",   "Cylinder",   OBJ_CYLINDER);
        hierarchy_context_add(s, "Add Cone",       "Cone",       OBJ_CONE);
        hierarchy_context_add(s, "Add Torus",      "Torus",      OBJ_TORUS);
        hierarchy_context_add(s, "Add Knot",       "Knot",       OBJ_KNOT);
        hierarchy_context_add(s, "Add Capsule",    "Capsule",    OBJ_CAPSULE);
        hierarchy_context_add(s, "Add Polygon",    "Polygon",    OBJ_POLY);
        ImGui::Separator();
        if (s->selectedCount > 0) {
            if (ImGui::MenuItem("Delete Selected")) {
                uint32_t ids[MAX_SELECTED];
                int count = s->selectedCount;
                for (int i = 0; i < count; i++) ids[i] = s->selectedIds[i];
                for (int i = 0; i < count; i++) {
                    int idx = scene_find_by_id(s, ids[i]);
                    if (idx >= 0) scene_remove(s, idx);
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ---- Properties ----

void ui_properties(Scene *s, EditorUI *ui) {
    if (!ui->showProperties) return;
    ImGui::Begin("Properties", &ui->showProperties);

    SceneObject *obj = scene_first_selected(s);
    if (!obj) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }
    if (s->selectedCount > 1) {
        ImGui::Text("%d objects selected", s->selectedCount);
        ImGui::Separator();
    }

    ImGui::InputText("Name", obj->name, MAX_NAME_LEN);
    ImGui::Checkbox("Visible", &obj->visible);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &obj->transform.position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &obj->transform.rotation.x, 1.0f);
        ImGui::DragFloat3("Scale",    &obj->transform.scale.x, 0.05f, 0.01f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        float col[4] = {
            obj->material.color.r / 255.0f,
            obj->material.color.g / 255.0f,
            obj->material.color.b / 255.0f,
            obj->material.color.a / 255.0f,
        };
        if (ImGui::ColorEdit4("Color", col)) {
            obj->material.color = Color{
                (uint8_t)(col[0] * 255),
                (uint8_t)(col[1] * 255),
                (uint8_t)(col[2] * 255),
                (uint8_t)(col[3] * 255),
            };
        }
        ImGui::SliderFloat("Roughness", &obj->material.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic",  &obj->material.metallic, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Texture");
        if (obj->material.hasTexture) {
            ImGui::Text("Current: %s", obj->material.texturePath);
            ImGui::Text("Size: %dx%d", obj->material.texture.width, obj->material.texture.height);
        }
        {
            const char *texOptions[] = {
                "None", "Checkerboard", "Brick", "Sand",
                "Brushed Metal", "Wood", "Grass", "From file..."
            };
            static const int TEX_COUNT = 8;
            static const int TEX_FROM_FILE = 7;
            static int texSel = 0;
            // only sync to None if we're not already in "From file..." mode (waiting for path input)
            if (!obj->material.hasTexture && texSel != TEX_FROM_FILE) texSel = 0;
            if (ImGui::Combo("##TexSelect", &texSel, texOptions, TEX_COUNT)) {
                switch (texSel) {
                    case 0: object_clear_texture(obj); break;
                    case 1: object_set_texture_builtin(obj, "Checkerboard",
                                CHECKERBOARD_PNG, CHECKERBOARD_PNG_LEN, ".png"); break;
                    case 2: object_set_texture_builtin(obj, "Brick",
                                BRICK_BMP, BRICK_BMP_LEN, ".bmp"); break;
                    case 3: object_set_texture_builtin(obj, "Sand",
                                SAND_PNG, SAND_PNG_LEN, ".png"); break;
                    case 4: object_set_texture_builtin(obj, "Brushed Metal",
                                BRUSHED_METAL_PNG, BRUSHED_METAL_PNG_LEN, ".png"); break;
                    case 5: object_set_texture_builtin(obj, "Wood",
                                WOOD_PNG, WOOD_PNG_LEN, ".png"); break;
                    case 6: object_set_texture_builtin(obj, "Grass",
                                GRASS_PNG, GRASS_PNG_LEN, ".png"); break;
                    default: break;
                }
            }
            if (texSel == TEX_FROM_FILE) {
                static char texPathBuf[256] = {};
                ImGui::InputText("Path##tex", texPathBuf, sizeof(texPathBuf));
                ImGui::SameLine();
                if (ImGui::Button("Load##tex")) {
                    if (texPathBuf[0] != '\0') {
                        object_set_texture(obj, texPathBuf);
                        if (!obj->material.hasTexture) {
                            snprintf(ui->errorMessage, sizeof(ui->errorMessage),
                                     "Failed to load texture:\n%s", texPathBuf);
                            ui->showErrorPopup = true;
                            texSel = 0;  // reset on failure
                        }
                        texPathBuf[0] = '\0';
                    }
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        switch (obj->type) {
            case OBJ_CUBE:
                ImGui::DragFloat3("Size (W/H/D)", obj->cubeSize, 0.1f, 0.1f, 50.0f);
                break;
            case OBJ_SPHERE:
                ImGui::DragFloat("Radius", &obj->sphereRadius, 0.05f, 0.1f, 50.0f);
                ImGui::SliderInt("Rings",  &obj->sphereRings, 4, 64);
                ImGui::SliderInt("Slices", &obj->sphereSlices, 4, 64);
                break;
            case OBJ_PLANE:
                ImGui::DragFloat("Width",  &obj->cubeSize[0], 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Length", &obj->cubeSize[2], 0.1f, 0.1f, 100.0f);
                break;
            case OBJ_CYLINDER:
                ImGui::DragFloat("Top Radius",    &obj->cylinderRadiusTop, 0.05f, 0.0f, 50.0f);
                ImGui::DragFloat("Bottom Radius", &obj->cylinderRadiusBottom, 0.05f, 0.0f, 50.0f);
                ImGui::DragFloat("Height",        &obj->cylinderHeight, 0.1f, 0.1f, 50.0f);
                break;
            case OBJ_CONE:
                ImGui::DragFloat("Radius", &obj->coneRadius, 0.05f, 0.01f, 50.0f);
                ImGui::DragFloat("Height", &obj->coneHeight, 0.1f, 0.1f, 50.0f);
                ImGui::SliderInt("Slices", &obj->coneSlices, 3, 64);
                break;
            case OBJ_TORUS:
            case OBJ_KNOT:
                ImGui::DragFloat("Radius",  &obj->torusRadius, 0.05f, 0.1f, 50.0f);
                ImGui::DragFloat("Tube Size", &obj->torusSize, 0.02f, 0.01f, 10.0f);
                ImGui::SliderInt("Radial Segments", &obj->torusRadSeg, 3, 64);
                ImGui::SliderInt("Sides",   &obj->torusSides, 3, 64);
                break;
            case OBJ_CAPSULE:
                ImGui::DragFloat("Radius", &obj->capsuleRadius, 0.05f, 0.01f, 50.0f);
                ImGui::DragFloat("Height", &obj->capsuleHeight, 0.1f, 0.1f, 50.0f);
                break;
            case OBJ_POLY:
                ImGui::SliderInt("Sides",  &obj->polySides, 3, 64);
                ImGui::DragFloat("Radius", &obj->polyRadius, 0.05f, 0.1f, 50.0f);
                break;
            case OBJ_CAMERA: {
                ImGui::DragFloat("FOV", &obj->camFov, 0.5f, 1.0f, 179.0f, "%.1f");
                ImGui::DragFloat("Near", &obj->camNear, 0.01f, 0.001f, 100.0f, "%.3f");
                ImGui::DragFloat("Far", &obj->camFar, 1.0f, 1.0f, 100000.0f, "%.0f");
                bool ortho = obj->camOrtho;
                if (ImGui::Checkbox("Orthographic", &ortho)) obj->camOrtho = ortho;
                break;
            }
            case OBJ_MODEL_FILE:
                ImGui::Text("Path: %s", obj->modelPath);
                if (obj->modelLoaded) {
                    ImGui::Text("Meshes: %d", obj->model.meshCount);
                    ImGui::Text("Materials: %d", obj->model.materialCount);
                } else {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Failed to load model");
                }
                break;
            default:
                break;
        }
    }

    ImGui::End();
}

// ---- Add Object ----

static void add_object_button(EditorUI *ui, const char *label, ObjectType type) {
    if (ImGui::Button(label, ImVec2(-1, 0))) {
        ui->placementMode = true;
        ui->placementType = type;
        ui->placementValid = false;
    }
}

void ui_add_object(Scene *s, EditorUI *ui) {
    if (!ui->showAddObject) return;
    ImGui::Begin("Add Object", &ui->showAddObject);

    if (ui->placementMode) {
        ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "Click viewport to place...");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "ESC / Right-click to cancel");
        ImGui::Separator();
    }

    add_object_button(ui, "Cube",       OBJ_CUBE);
    add_object_button(ui, "Sphere",     OBJ_SPHERE);
    add_object_button(ui, "Plane",      OBJ_PLANE);
    add_object_button(ui, "Cylinder",   OBJ_CYLINDER);
    add_object_button(ui, "Cone",       OBJ_CONE);
    add_object_button(ui, "Torus",      OBJ_TORUS);
    add_object_button(ui, "Knot",       OBJ_KNOT);
    add_object_button(ui, "Capsule",    OBJ_CAPSULE);
    add_object_button(ui, "Polygon",    OBJ_POLY);
    add_object_button(ui, "Teapot",     OBJ_TEAPOT);
    add_object_button(ui, "Camera",     OBJ_CAMERA);

    ImGui::Separator();
    ImGui::Text("Load Model File");
    static char modelPathBuf[256] = {};
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##ModelPath", "File path (.obj, .gltf, ...)", modelPathBuf, sizeof(modelPathBuf));
    if (ImGui::Button("Load Model", ImVec2(-1, 0))) {
        if (modelPathBuf[0] != '\0') {
            int idx = scene_add_model(s, modelPathBuf);
            if (idx >= 0) {
                if (!s->objects[idx].modelLoaded) {
                    snprintf(ui->errorMessage, sizeof(ui->errorMessage),
                             "Failed to load model:\n%s", modelPathBuf);
                    ui->showErrorPopup = true;
                    scene_remove(s, idx);
                } else {
                    scene_deselect_all(s);
                    scene_select(s, s->objects[idx].id, false, false);
                }
            } else {
                snprintf(ui->errorMessage, sizeof(ui->errorMessage),
                         "Failed to add model (scene full):\n%s", modelPathBuf);
                ui->showErrorPopup = true;
            }
            modelPathBuf[0] = '\0';
        }
    }

    ImGui::End();
}

// ---- Camera ----

void ui_camera(Scene *s, EditorCamera *ec, EditorUI *ui) {
    if (!ui->showCamera) return;
    ImGui::Begin("Camera", &ui->showCamera);

    // ---- Active camera selector ----
    ImGui::SeparatorText("Active Camera");
    {
        // build list: "Editor Camera" + all scene camera objects
        const char *editorLabel = "Editor Camera";
        int currentIdx = 0; // 0 = editor camera

        // count scene cameras
        int camCount = 0;
        for (int i = 0; i < s->objectCount; i++) {
            if (s->objects[i].type == OBJ_CAMERA) camCount++;
        }

        // items: editor + scene cameras
        const char *items[MAX_OBJECTS + 1];
        uint32_t itemIds[MAX_OBJECTS + 1];
        items[0] = editorLabel;
        itemIds[0] = 0;
        int n = 1;
        for (int i = 0; i < s->objectCount; i++) {
            if (s->objects[i].type == OBJ_CAMERA) {
                items[n] = s->objects[i].name;
                itemIds[n] = s->objects[i].id;
                if (s->objects[i].id == ui->activeCameraId) currentIdx = n;
                n++;
            }
        }

        if (ImGui::Combo("##ActiveCam", &currentIdx, items, n)) {
            ui->activeCameraId = itemIds[currentIdx];
        }
    }

    // validate: if active camera was deleted, fall back to editor
    if (ui->activeCameraId != 0 && scene_find_by_id(s, ui->activeCameraId) < 0) {
        ui->activeCameraId = 0;
    }

    ImGui::Separator();

    if (ui->activeCameraId == 0) {
        // ---- Editor camera controls ----
        ImGui::SeparatorText("Projection");
        const char *projItems[] = { "Perspective", "Orthographic" };
        int projIdx = ec->ortho ? 1 : 0;
        if (ImGui::Combo("Projection", &projIdx, projItems, 2)) {
            ec->ortho = (projIdx == 1);
        }
        ImGui::DragFloat("FOV", &ec->fov, 0.5f, 1.0f, 179.0f, "%.1f");
        ImGui::DragFloat("Near", &ec->nearPlane, 0.01f, 0.001f, 100.0f, "%.3f");
        ImGui::DragFloat("Far", &ec->farPlane, 1.0f, 1.0f, 100000.0f, "%.0f");

        ImGui::SeparatorText("Orbit");
        ImGui::DragFloat("Distance", &ec->distance, 0.1f, 1.0f, 200.0f, "%.1f");
        float yawDeg = ec->yaw * RAD2DEG;
        float pitchDeg = ec->pitch * RAD2DEG;
        if (ImGui::DragFloat("Yaw", &yawDeg, 0.5f)) ec->yaw = yawDeg * DEG2RAD;
        if (ImGui::DragFloat("Pitch", &pitchDeg, 0.5f, -85.0f, 85.0f)) ec->pitch = pitchDeg * DEG2RAD;

        ImGui::SeparatorText("Target");
        ImGui::DragFloat3("Position", &ec->target.x, 0.1f);

        ImGui::SeparatorText("Speed");
        ImGui::DragFloat("Move (WASD)", &ec->moveSpeed, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Zoom", &ec->zoomSpeed, 0.05f, 0.1f, 10.0f, "%.2f");
        ImGui::DragFloat("Pan", &ec->panSpeed, 0.001f, 0.001f, 0.1f, "%.3f");
        ImGui::DragFloat("Orbit Speed", &ec->orbitSpeed, 0.0005f, 0.001f, 0.05f, "%.4f");

        ImGui::SeparatorText("Info");
        ImGui::Text("Cam Pos: %.1f, %.1f, %.1f", ec->cam.position.x, ec->cam.position.y, ec->cam.position.z);

        if (ImGui::Button("Reset Camera")) {
            ec->target = Vector3{0, 0, 0};
            ec->distance = 15.0f;
            ec->yaw = 0.8f;
            ec->pitch = 0.6f;
            ec->fov = 45.0f;
            ec->nearPlane = 0.1f;
            ec->farPlane = 1000.0f;
            ec->ortho = false;
        }

        editor_camera_sync(ec);
    } else {
        // ---- Scene camera controls ----
        SceneObject *camObj = scene_get_by_id(s, ui->activeCameraId);
        if (camObj) {
            ImGui::Text("Viewing through: %s", camObj->name);
            ImGui::SeparatorText("Camera Settings");
            ImGui::DragFloat("FOV", &camObj->camFov, 0.5f, 1.0f, 179.0f, "%.1f");
            ImGui::DragFloat("Near", &camObj->camNear, 0.01f, 0.001f, 100.0f, "%.3f");
            ImGui::DragFloat("Far", &camObj->camFar, 1.0f, 1.0f, 100000.0f, "%.0f");
            bool ortho = camObj->camOrtho;
            if (ImGui::Checkbox("Orthographic", &ortho)) camObj->camOrtho = ortho;

            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Position", &camObj->transform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &camObj->transform.rotation.x, 1.0f);
        }
    }

    ImGui::End();
}

// ---- Timeline ----

void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui) {
    if (!ui->showTimeline) return;
    ImGui::Begin("Timeline", &ui->showTimeline);

    if (tl->state == PLAYBACK_PLAYING) {
        if (ImGui::Button("Pause")) timeline_pause(tl);
    } else {
        if (ImGui::Button("Play")) timeline_play(tl);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) timeline_stop(tl);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 200);
    int frame = tl->currentFrame;
    if (ImGui::SliderInt("##Frame", &frame, tl->startFrame, tl->endFrame)) {
        timeline_set_frame(tl, frame);
    }
    ImGui::SameLine();
    ImGui::Text("Frame: %d", tl->currentFrame);

    SceneObject *obj = scene_first_selected(s);
    if (obj) {
        ImGui::Separator();
        ImGui::Text("Keyframes: %s", obj->name);
        ImGui::SameLine();
        if (ImGui::Button("Insert Key")) {
            keyframe_insert(obj, tl->currentFrame);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Key")) {
            keyframe_remove(obj, tl->currentFrame);
        }

        if (obj->keyframeCount > 0) {
            ImGui::Text("Keys:");
            ImGui::SameLine();
            for (int i = 0; i < obj->keyframeCount; i++) {
                char label[16];
                snprintf(label, sizeof(label), "%d", obj->keyframes[i].frame);
                if (ImGui::SmallButton(label)) {
                    timeline_set_frame(tl, obj->keyframes[i].frame);
                }
                if (i < obj->keyframeCount - 1) ImGui::SameLine();
            }
        }
    }

    ImGui::End();
}
