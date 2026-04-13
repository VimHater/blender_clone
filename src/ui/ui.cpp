#include "ui.h"
#include "rlImGui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cstdint>

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
    ui->showTimeline = true;
    ui->showHierarchy = true;
    ui->showProperties = true;
    ui->showAddObject = true;
    ui->showCamera = true;
    ui->dockspaceInitialized = false;
    ui->placementMode = false;
    ui->placementValid = false;
    ui->vpImageX = ui->vpImageY = 0;
    ui->vpImageW = ui->vpImageH = 0;
}

void ui_update_layout(EditorUI *ui) {
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

        ImGui::DockBuilderFinish(dockId);
    }

    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();
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
            if (ImGui::MenuItem("Cube"))       { ui->placementMode = true; ui->placementType = OBJ_CUBE; }
            if (ImGui::MenuItem("Sphere"))     { ui->placementMode = true; ui->placementType = OBJ_SPHERE; }
            if (ImGui::MenuItem("HemiSphere")) { ui->placementMode = true; ui->placementType = OBJ_HEMISPHERE; }
            if (ImGui::MenuItem("Plane"))      { ui->placementMode = true; ui->placementType = OBJ_PLANE; }
            if (ImGui::MenuItem("Cylinder"))   { ui->placementMode = true; ui->placementType = OBJ_CYLINDER; }
            if (ImGui::MenuItem("Cone"))       { ui->placementMode = true; ui->placementType = OBJ_CONE; }
            if (ImGui::MenuItem("Torus"))      { ui->placementMode = true; ui->placementType = OBJ_TORUS; }
            if (ImGui::MenuItem("Knot"))       { ui->placementMode = true; ui->placementType = OBJ_KNOT; }
            if (ImGui::MenuItem("Capsule"))    { ui->placementMode = true; ui->placementType = OBJ_CAPSULE; }
            if (ImGui::MenuItem("Polygon"))    { ui->placementMode = true; ui->placementType = OBJ_POLY; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &ui->showHierarchy);
            ImGui::MenuItem("Properties", nullptr, &ui->showProperties);
            ImGui::MenuItem("Add Object", nullptr, &ui->showAddObject);
            ImGui::MenuItem("Camera", nullptr, &ui->showCamera);
            ImGui::MenuItem("Timeline", nullptr, &ui->showTimeline);
            ImGui::MenuItem("Grid", nullptr, &ui->showGrid);
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

    // track image rect for mouse-to-world mapping
    ImVec2 cpos = ImGui::GetCursorScreenPos();
    ImVec2 csize = ImGui::GetContentRegionAvail();
    float texW = (float)ui->viewportRT.texture.width;
    float texH = (float)ui->viewportRT.texture.height;
    float scale = csize.x / texW;
    if (texH * scale > csize.y) scale = csize.y / texH;
    ui->vpImageW = texW * scale;
    ui->vpImageH = texH * scale;
    ui->vpImageX = cpos.x + (csize.x - ui->vpImageW) * 0.5f;
    ui->vpImageY = cpos.y + (csize.y - ui->vpImageH) * 0.5f;

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
    ImGui::Begin("Scene Hierarchy", &ui->showHierarchy);

    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == -1) {
            hierarchy_draw_node(s, i);
        }
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
        if (ImGui::MenuItem("Add Cube"))       { s->selectedIndex = scene_add(s, "Cube", OBJ_CUBE); }
        if (ImGui::MenuItem("Add Sphere"))     { s->selectedIndex = scene_add(s, "Sphere", OBJ_SPHERE); }
        if (ImGui::MenuItem("Add HemiSphere")) { s->selectedIndex = scene_add(s, "HemiSphere", OBJ_HEMISPHERE); }
        if (ImGui::MenuItem("Add Plane"))      { s->selectedIndex = scene_add(s, "Plane", OBJ_PLANE); }
        if (ImGui::MenuItem("Add Cylinder"))   { s->selectedIndex = scene_add(s, "Cylinder", OBJ_CYLINDER); }
        if (ImGui::MenuItem("Add Cone"))       { s->selectedIndex = scene_add(s, "Cone", OBJ_CONE); }
        if (ImGui::MenuItem("Add Torus"))      { s->selectedIndex = scene_add(s, "Torus", OBJ_TORUS); }
        if (ImGui::MenuItem("Add Knot"))       { s->selectedIndex = scene_add(s, "Knot", OBJ_KNOT); }
        if (ImGui::MenuItem("Add Capsule"))    { s->selectedIndex = scene_add(s, "Capsule", OBJ_CAPSULE); }
        if (ImGui::MenuItem("Add Polygon"))    { s->selectedIndex = scene_add(s, "Polygon", OBJ_POLY); }
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
    ImGui::Begin("Properties", &ui->showProperties);

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
            case OBJ_HEMISPHERE:
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
    add_object_button(ui, "HemiSphere", OBJ_HEMISPHERE);
    add_object_button(ui, "Plane",      OBJ_PLANE);
    add_object_button(ui, "Cylinder",   OBJ_CYLINDER);
    add_object_button(ui, "Cone",       OBJ_CONE);
    add_object_button(ui, "Torus",      OBJ_TORUS);
    add_object_button(ui, "Knot",       OBJ_KNOT);
    add_object_button(ui, "Capsule",    OBJ_CAPSULE);
    add_object_button(ui, "Polygon",    OBJ_POLY);

    ImGui::End();
}

// ---- Camera ----

void ui_camera(EditorCamera *ec, EditorUI *ui) {
    if (!ui->showCamera) return;
    ImGui::Begin("Camera", &ui->showCamera);

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
