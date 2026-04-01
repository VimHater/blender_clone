#include "ui.h"
#include "rlImGui.h"
#include "imgui.h"
#include <cstdio>
#include <cstdint>

// ---- Layout ----

static void compute_layout(UILayout *l) {
    l->screenW = (float)GetScreenWidth();
    l->screenH = (float)GetScreenHeight();
    l->menuBarH = UI_MENUBAR_HEIGHT;
    l->hierarchyW = l->screenW * UI_HIERARCHY_WIDTH_RATIO;
    l->propertiesW = l->screenW * UI_PROPERTIES_WIDTH_RATIO;
    l->timelineH = l->screenH * UI_TIMELINE_HEIGHT_RATIO;
    l->viewportX = l->hierarchyW;
    l->viewportY = l->menuBarH;
    l->viewportW = l->screenW - l->hierarchyW - l->propertiesW;
    l->viewportH = l->screenH - l->menuBarH - l->timelineH;
}

void ui_init(EditorUI *ui, int vpW, int vpH) {
    ui->viewportW = vpW;
    ui->viewportH = vpH;
    ui->viewportRT = LoadRenderTexture(vpW, vpH);
    ui->viewportHovered = false;
    ui->viewportFocused = false;
    ui->showGrid = true;
    ui->gridSize = 10;
    ui->gridSpacing = 1.0f;
    ui->transformMode = TMODE_TRANSLATE;
    ui->showTimeline = true;
    ui->showHierarchy = true;
    ui->showProperties = true;
    compute_layout(&ui->layout);
}

void ui_update_layout(EditorUI *ui) {
    compute_layout(&ui->layout);

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw != ui->viewportW || sh != ui->viewportH) {
        UnloadRenderTexture(ui->viewportRT);
        ui->viewportRT = LoadRenderTexture(sw, sh);
        ui->viewportW = sw;
        ui->viewportH = sh;
    }
}

void ui_shutdown(EditorUI *ui) {
    UnloadRenderTexture(ui->viewportRT);
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
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) {
                int idx = scene_add(s, "Cube", OBJ_CUBE);
                s->selectedIndex = idx;
            }
            if (ImGui::MenuItem("Sphere")) {
                int idx = scene_add(s, "Sphere", OBJ_SPHERE);
                s->selectedIndex = idx;
            }
            if (ImGui::MenuItem("Plane")) {
                int idx = scene_add(s, "Plane", OBJ_PLANE);
                s->selectedIndex = idx;
            }
            if (ImGui::MenuItem("Cylinder")) {
                int idx = scene_add(s, "Cylinder", OBJ_CYLINDER);
                s->selectedIndex = idx;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &ui->showHierarchy);
            ImGui::MenuItem("Properties", nullptr, &ui->showProperties);
            ImGui::MenuItem("Timeline", nullptr, &ui->showTimeline);
            ImGui::MenuItem("Grid", nullptr, &ui->showGrid);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ---- Viewport ----

void ui_viewport(EditorUI *ui) {
    const UILayout &l = ui->layout;
    ImGui::SetNextWindowPos(ImVec2(l.viewportX, l.viewportY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(l.viewportW, l.viewportH), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ui->viewportHovered = ImGui::IsWindowHovered();
    ui->viewportFocused = ImGui::IsWindowFocused();
    rlImGuiImageRenderTextureFit(&ui->viewportRT, true);
    ImGui::End();
    ImGui::PopStyleVar();
}

// ---- Hierarchy ----

static void hierarchy_draw_node(Scene *s, int index) {
    SceneObject *obj = &s->objects[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (s->selectedIndex == index) flags |= ImGuiTreeNodeFlags_Selected;

    bool hasChildren = false;
    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == index) { hasChildren = true; break; }
    }
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    bool open = ImGui::TreeNodeEx((void *)(intptr_t)index, flags, "%s", obj->name);
    if (ImGui::IsItemClicked()) {
        s->selectedIndex = index;
    }

    if (open) {
        for (int i = 0; i < s->objectCount; i++) {
            if (s->objects[i].parentIndex == index) {
                hierarchy_draw_node(s, i);
            }
        }
        ImGui::TreePop();
    }
}

void ui_hierarchy(Scene *s, EditorUI *ui) {
    if (!ui->showHierarchy) return;
    const UILayout &l = ui->layout;
    ImGui::SetNextWindowPos(ImVec2(0, l.menuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(l.hierarchyW, l.screenH - l.menuBarH), ImGuiCond_Always);
    ImGui::Begin("Scene Hierarchy", &ui->showHierarchy, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == -1) {
            hierarchy_draw_node(s, i);
        }
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
        if (ImGui::MenuItem("Add Cube"))     { scene_add(s, "Cube", OBJ_CUBE); }
        if (ImGui::MenuItem("Add Sphere"))   { scene_add(s, "Sphere", OBJ_SPHERE); }
        if (ImGui::MenuItem("Add Plane"))    { scene_add(s, "Plane", OBJ_PLANE); }
        if (ImGui::MenuItem("Add Cylinder")) { scene_add(s, "Cylinder", OBJ_CYLINDER); }
        ImGui::Separator();
        if (s->selectedIndex >= 0) {
            if (ImGui::MenuItem("Delete Selected")) {
                scene_remove(s, s->selectedIndex);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ---- Properties ----

void ui_properties(Scene *s, EditorUI *ui) {
    if (!ui->showProperties) return;
    const UILayout &l = ui->layout;
    ImGui::SetNextWindowPos(ImVec2(l.screenW - l.propertiesW, l.menuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(l.propertiesW, l.screenH - l.menuBarH), ImGuiCond_Always);
    ImGui::Begin("Properties", &ui->showProperties, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    SceneObject *obj = scene_selected(s);
    if (!obj) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
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
            default:
                break;
        }
    }

    ImGui::End();
}

// ---- Timeline ----

void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui) {
    if (!ui->showTimeline) return;
    const UILayout &l = ui->layout;
    ImGui::SetNextWindowPos(ImVec2(l.viewportX, l.viewportY + l.viewportH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(l.viewportW, l.timelineH), ImGuiCond_Always);
    ImGui::Begin("Timeline", &ui->showTimeline, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

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

    SceneObject *obj = scene_selected(s);
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
