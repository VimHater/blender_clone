#include "ui.h"
#include <rlImGui.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#include <io.h>    // _findfirst/_findnext (no Win32 API conflicts with raylib)
#else
#include <dirent.h>
#endif

extern "C" {
#include <tinyfiledialogs.h>
}
#include "builtin_textures/checkerboard.texture_array"
#include "builtin_textures/brick.texture_array"
#include "builtin_textures/sand.texture_array"
#include "builtin_textures/brushed_metal.texture_array"
#include "builtin_textures/wood.texture_array"
#include "builtin_textures/grass.texture_array"

// ---- Init / Shutdown ----

void ui_init(EditorUI *ui, int vpW, int vpH) {
    ui->viewportW = vpW;
    ui->viewportH = vpH;
    ui->viewportRT = LoadRenderTexture(vpW * 2, vpH * 2);
    ui->animViewportRT = LoadRenderTexture(vpW * 2, vpH * 2);
    SetTextureFilter(ui->viewportRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ui->animViewportRT.texture, TEXTURE_FILTER_BILINEAR);
    ui->activeViewportTab = 0;
    ui->viewportHovered = false;
    ui->viewportFocused = false;
    ui->uiScale = 1.0f;
    ui->showGrid = true;
    ui->gridSize = 10;
    ui->gridSpacing = 1.0f;
    ui->transformMode = TMODE_TRANSLATE;
    ui->drawMode = DRAW_SOLID;
    ui->showTimeline = true;
    ui->showConsole = true;
    ui->showHierarchy = true;
    ui->showProperties = true;
    ui->showAddObject = true;
    ui->showCamera = true;
    ui->dockspaceInitialized = false;
    ui->activeCameraId = 0;
    ui->titleBarDragging = false;
    ui->titleBarDragOffset = {0, 0};
    ui->wantClose = false;
    ui->wantSave = false;
    ui->showSaveAsPopup = false;
    strncpy(ui->saveAsName, "project.scene", sizeof(ui->saveAsName));
    ui->showErrorPopup = false;
    ui->errorMessage[0] = '\0';
    ui->gizmoActiveAxis = GIZMO_NONE;
    ui->gizmoDragging = false;
    ui->placementMode = false;
    ui->repeatPlayback = true;
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
    UnloadRenderTexture(ui->animViewportRT);
}

// ---- Dockspace ----

void ui_dockspace(EditorUI *ui) {
    ImGuiViewport *vp = ImGui::GetMainViewport();
    float titleBarHeight = ImGui::GetFrameHeight() * 1.4f;
    ImVec2 dockPos(vp->Pos.x, vp->Pos.y + titleBarHeight);
    ImVec2 dockSize(vp->Size.x, vp->Size.y - titleBarHeight);
    ImGui::SetNextWindowPos(dockPos);
    ImGui::SetNextWindowSize(dockSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

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
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.35f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

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
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Viewport", dockMain);

        // hide tab bar on viewport node
        ImGuiDockNode *vpNode = ImGui::DockBuilderGetNode(dockMain);
        if (vpNode) vpNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

        ImGui::DockBuilderFinish(dockId);
        ui->dockspaceInitFrames = 2; // need a couple frames for tabs to settle
    }

    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    // focus Timeline tab after layout settles
    if (ui->dockspaceInitFrames > 0) {
        ui->dockspaceInitFrames--;
        if (ui->dockspaceInitFrames == 0) {
            ImGui::SetWindowFocus("Timeline");
        }
    }

    ImGui::End();
}

// ---- Error Popup ----

void ui_save_as_popup(EditorUI *ui) {
    if (ui->showSaveAsPopup) {
        ImGui::OpenPopup("Save As");
        ui->showSaveAsPopup = false;
    }
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File name:");
        ImGui::SetNextItemWidth(-40);
        bool enter = ImGui::InputText("##saveas", ui->saveAsName, sizeof(ui->saveAsName),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("...##saveas")) {
            char const *filters[] = {"*.scene", "*.sceneb"};
            char const *result = tinyfd_saveFileDialog(
                "Save As", ui->saveAsName, 2, filters, "Scene files");
            if (result) snprintf(ui->saveAsName, sizeof(ui->saveAsName), "%s", result);
        }
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0)) || enter) {
            ui->wantSave = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ui_error_popup(EditorUI *ui) {
    if (ui->showErrorPopup) {
        ImGui::OpenPopup("Notice");
        ui->showErrorPopup = false;
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(600, 300));
    if (ImGui::BeginPopupModal("Notice", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", ui->errorMessage);
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(-1, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ui_shortcut_popup() {
    if (!IsKeyDown(KEY_SLASH)) return;

    ImGuiIO &io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0)); // auto-size

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##Shortcuts", nullptr, flags)) {
        ImGui::Text("Keyboard Shortcuts");
        ImGui::Separator();

        ImGui::Text("Camera");
        ImGui::BulletText("W/A/S/D   Move camera");
        ImGui::BulletText("Space     Move up");
        ImGui::BulletText("Shift     Move down");
        ImGui::BulletText("Alt hold  FPS camera (mouse look + WASD)");
        ImGui::BulletText("MMB drag  Orbit");
        ImGui::BulletText("Shift+MMB Pan");
        ImGui::BulletText("Scroll    Zoom");

        ImGui::Separator();
        ImGui::Text("Selection");
        ImGui::BulletText("LMB       Select object");
        ImGui::BulletText("Ctrl+LMB  Multi-select");
        ImGui::BulletText("Delete    Delete selected");

        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::BulletText("T         Translate mode");
        ImGui::BulletText("R         Rotate mode");
        ImGui::BulletText("Y         Scale mode");
        ImGui::BulletText("Drag axis Gizmo transform");

        ImGui::Separator();
        ImGui::Text("Placement");
        ImGui::BulletText("LMB       Place object");
        ImGui::BulletText("Esc/RMB   Cancel");
    }
    ImGui::End();
}

// ---- Menu Bar ----

void ui_menu_bar(Scene *s, EditorCamera *ec, Timeline *tl, EditorUI *ui) {
    (void)ec; (void)tl;

    float titleBarHeight = ImGui::GetFrameHeight() * 1.4f;
    float buttonW = titleBarHeight * 0.9f;

    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, titleBarHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.075f, 0.075f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.075f, 0.075f, 0.075f, 1.0f));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    ImGui::Begin("##TitleBar", nullptr, flags);

    // --- Menu bar inside the title bar window ---
    if (ImGui::BeginMenuBar()) {
        // app title
        ImGui::TextColored(ImVec4(0.6f, 0.75f, 1.0f, 1.0f), "Melder");
        ImGui::Separator();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("Examples")) {
                bool found = false;
#ifdef _WIN32
                struct _finddata_t fd;
                intptr_t hFind = _findfirst("examples\\*.scene", &fd);
                if (hFind != -1) {
                    do {
                        const char *name = fd.name;
                        size_t len = strlen(name);
                        char label[256];
                        snprintf(label, sizeof(label), "%.*s", (int)(len - 6), name);
                        if (ImGui::MenuItem(label)) {
                            snprintf(ui->loadPath, sizeof(ui->loadPath), "examples/%s", name);
                            ui->wantLoad = true;
                        }
                        found = true;
                    } while (_findnext(hFind, &fd) == 0);
                    _findclose(hFind);
                }
#else
                DIR *dir = opendir("examples");
                if (dir) {
                    struct dirent *ent;
                    while ((ent = readdir(dir)) != nullptr) {
                        const char *name = ent->d_name;
                        size_t len = strlen(name);
                        if (len > 6 && strcmp(name + len - 6, ".scene") == 0) {
                            char label[256];
                            snprintf(label, sizeof(label), "%.*s", (int)(len - 6), name);
                            if (ImGui::MenuItem(label)) {
                                snprintf(ui->loadPath, sizeof(ui->loadPath), "examples/%s", name);
                                ui->wantLoad = true;
                            }
                            found = true;
                        }
                    }
                    closedir(dir);
                }
#endif
                if (!found) ImGui::TextDisabled("No examples found");
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                char const *filters[] = {"*.scene", "*.sceneb"};
                char const *result = tinyfd_openFileDialog(
                    "Open Scene", "", 2, filters, "Scene files", 0);
                if (result) {
                    snprintf(ui->loadPath, sizeof(ui->loadPath), "%s", result);
                    ui->wantLoad = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                ui->wantSave = true;
            }
            if (ImGui::MenuItem("Save As...")) {
                ui->showSaveAsPopup = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                ui->wantClose = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &ui->showHierarchy);
            ImGui::MenuItem("Properties", nullptr, &ui->showProperties);
            ImGui::MenuItem("Add Object", nullptr, &ui->showAddObject);
            ImGui::MenuItem("Camera", nullptr, &ui->showCamera);
            ImGui::MenuItem("Timeline", nullptr, &ui->showTimeline);
            ImGui::MenuItem("Console", nullptr, &ui->showConsole);
            ImGui::MenuItem("Grid", nullptr, &ui->showGrid);
            ImGui::Separator();
            const char *drawModeNames[] = { "Solid", "Wireframe", "Point" };
            int dm = (int)ui->drawMode;
            if (ImGui::Combo("Draw Mode", &dm, drawModeNames, 3)) {
                ui->drawMode = (DrawMode)dm;
            }
            ImGui::EndMenu();
        }

        // right side: shortcut hint + window buttons flush to edge
        float buttonsW = buttonW * 3; // no spacing between buttons
        float hintW = ImGui::CalcTextSize("press / for Shortcuts").x;
        float rightContentW = hintW + ImGui::GetStyle().ItemSpacing.x + buttonsW;
        ImGui::SameLine(ImGui::GetWindowWidth() - rightContentW);
        ImGui::TextDisabled("press / for Shortcuts");

        // window control buttons — flush to right edge, no background, glow on hover
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::SameLine(ImGui::GetWindowWidth() - buttonsW);

        ImU32 dimCol   = IM_COL32(114, 112, 114, 255); // #727072
        ImU32 glowCol  = IM_COL32(242, 242, 242, 255); // bright white
        ImU32 pressCol = IM_COL32(180, 180, 180, 255);
        ImU32 redGlow  = IM_COL32(255, 85, 85, 255);   // #ff5555
        ImU32 redPress = IM_COL32(204, 51, 51, 255);

        // helper: invisible button with centered colored text
        auto glowButton = [&](const char *id, const char *icon, ImVec2 size,
                              ImU32 normal, ImU32 hover, ImU32 active) -> bool {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::InvisibleButton(id, size);
            bool hovered = ImGui::IsItemHovered();
            bool held = ImGui::IsItemActive();
            ImU32 col = held ? active : (hovered ? hover : normal);
            ImVec2 textSize = ImGui::CalcTextSize(icon);
            ImVec2 textPos(pos.x + (size.x - textSize.x) * 0.5f,
                           pos.y + (size.y - textSize.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(textPos, col, icon);
            return clicked;
        };

        float btnH = ImGui::GetFrameHeight();

        // minimize
        if (glowButton("##min", "\xe2\x80\x94", ImVec2(buttonW, btnH), dimCol, glowCol, pressCol)) {
            MinimizeWindow();
        }
        ImGui::SameLine();
        // maximize/restore
        bool maximized = IsWindowMaximized();
        // if (glowButton("##max", maximized ? "\xe2\x8a\xa1" : "\xe2\x96\xa1",
        if (glowButton("##max", "\xe2\x96\xa1",
                        ImVec2(buttonW, btnH), dimCol, glowCol, pressCol)) {
            if (maximized) RestoreWindow();
            else MaximizeWindow();
        }
        ImGui::SameLine();
        // close — glows red
        if (glowButton("##close ", "\xe2\x9c\x95 ", ImVec2(buttonW, btnH), dimCol, redGlow, redPress)) {
            ui->wantClose = true;
        }

        ImGui::PopStyleVar(); // item spacing

        ImGui::EndMenuBar();
    }

    // --- Draggable area: anywhere on the title bar that isn't a widget ---
    // Check if mouse is in the title bar area and not over any imgui item
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseInTitleBar = mousePos.y >= vp->Pos.y && mousePos.y < vp->Pos.y + titleBarHeight
                        && mousePos.x >= vp->Pos.x && mousePos.x < vp->Pos.x + vp->Size.x - buttonW * 3;

    if (mouseInTitleBar && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        ui->titleBarDragging = true;
        Vector2 mouse = GetMousePosition(); // window-local
        ui->titleBarDragOffset = mouse;
        // un-maximize on drag start so the window detaches
        if (IsWindowMaximized()) {
            RestoreWindow();
        }
    }
    if (ui->titleBarDragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Vector2 mouse = GetMousePosition(); // window-local
            Vector2 winPos = GetWindowPosition();
            SetWindowPosition(
                (int)(winPos.x + mouse.x - ui->titleBarDragOffset.x),
                (int)(winPos.y + mouse.y - ui->titleBarDragOffset.y));
        } else {
            ui->titleBarDragging = false;
        }
    }

    // double-click title bar to maximize/restore
    if (mouseInTitleBar && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        if (IsWindowMaximized()) RestoreWindow();
        else MaximizeWindow();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ---- Viewport ----

void ui_viewport(EditorUI *ui) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    // tabs: Edit and Animation
    ImGui::PushStyleColor(ImGuiCol_Tab,                ImVec4(0.25f, 0.20f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         ImVec4(0.55f, 0.40f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,        ImVec4(0.45f, 0.32f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline,ImVec4(0.80f, 0.55f, 0.20f, 1.0f));
    if (ImGui::BeginTabBar("##ViewportTabs")) {
        ImGuiTabItemFlags editFlags = (ui->activeViewportTab == 0 && !ui->playMode) ? 0 : 0;
        ImGuiTabItemFlags animFlags = 0;
        // programmatic tab switch via activeViewportTab set externally
        static int lastTab = -1;
        if (lastTab != ui->activeViewportTab) {
            if (ui->activeViewportTab == 0) editFlags |= ImGuiTabItemFlags_SetSelected;
            if (ui->activeViewportTab == 1) animFlags |= ImGuiTabItemFlags_SetSelected;
            lastTab = ui->activeViewportTab;
        }
        if (ImGui::BeginTabItem("Edit", nullptr, editFlags)) {
            ui->activeViewportTab = 0;
            lastTab = 0;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animation", nullptr, animFlags)) {
            ui->activeViewportTab = 1;
            lastTab = 1;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(4);

    ui->viewportHovered = ImGui::IsWindowHovered();
    ui->viewportFocused = ImGui::IsWindowFocused();

    ImVec2 cpos = ImGui::GetCursorScreenPos();
    ImVec2 csize = ImGui::GetContentRegionAvail();
    int panelW = (int)csize.x;
    int panelH = (int)csize.y;
    if (panelW < 1) panelW = 1;
    if (panelH < 1) panelH = 1;

    // resize render textures to match panel (2x supersample for AA)
    if (panelW != ui->viewportW || panelH != ui->viewportH) {
        UnloadRenderTexture(ui->viewportRT);
        UnloadRenderTexture(ui->animViewportRT);
        ui->viewportRT = LoadRenderTexture(panelW * 2, panelH * 2);
        ui->animViewportRT = LoadRenderTexture(panelW * 2, panelH * 2);
        // set bilinear filtering for smooth downscale
        SetTextureFilter(ui->viewportRT.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(ui->animViewportRT.texture, TEXTURE_FILTER_BILINEAR);
        ui->viewportW = panelW;
        ui->viewportH = panelH;
    }

    // image fills panel 1:1 — no letterboxing
    ui->vpImageX = cpos.x;
    ui->vpImageY = cpos.y;
    ui->vpImageW = (float)panelW;
    ui->vpImageH = (float)panelH;

    if (ui->activeViewportTab == 1) {
        rlImGuiImageRenderTextureFit(&ui->animViewportRT, true);
    } else {
        rlImGuiImageRenderTextureFit(&ui->viewportRT, true);
    }
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

void ui_properties(Scene *s, Timeline *tl, EditorUI *ui) {
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

    if (ImGui::CollapsingHeader("Transform", 0)) {
        bool changed = false;
        changed |= ImGui::DragFloat3("Position", &obj->transform.position.x, 0.1f);
        changed |= ImGui::DragFloat3("Rotation", &obj->transform.rotation.x, 1.0f);
        changed |= ImGui::DragFloat3("Scale",    &obj->transform.scale.x, 0.05f, 0.01f, 100.0f);
        if (changed && obj->keyframeCount > 0) {
            keyframe_sync(obj, tl->currentFrame);
        }
    }

    if (obj->type != OBJ_LIGHT && obj->type != OBJ_CAMERA && ImGui::CollapsingHeader("Shader", 0)) {
        const char *shaderNames[] = { "Default (Blinn-Phong)", "Unlit", "Toon", "Normal", "Fresnel" };
        int cur = (int)obj->shaderType;
        if (ImGui::Combo("##ShaderType", &cur, shaderNames, SHADER_COUNT)) {
            obj->shaderType = (ShaderType)cur;
        }
    }

    if (obj->type != OBJ_LIGHT && ImGui::CollapsingHeader("Material", 0)) {
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
            static uint32_t lastTexObjId = 0;
            // sync texSel when selected object changes
            if (obj->id != lastTexObjId) {
                lastTexObjId = obj->id;
                if (!obj->material.hasTexture) {
                    texSel = 0;
                } else if (strstr(obj->material.texturePath, "Checkerboard")) texSel = 1;
                else if (strstr(obj->material.texturePath, "Brick")) texSel = 2;
                else if (strstr(obj->material.texturePath, "Sand")) texSel = 3;
                else if (strstr(obj->material.texturePath, "Brushed Metal")) texSel = 4;
                else if (strstr(obj->material.texturePath, "Wood")) texSel = 5;
                else if (strstr(obj->material.texturePath, "Grass")) texSel = 6;
                else texSel = TEX_FROM_FILE;
            }
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
                ImGui::SetNextItemWidth(-120);
                ImGui::InputText("Path##tex", texPathBuf, sizeof(texPathBuf));
                ImGui::SameLine();
                if (ImGui::Button("...##tex")) {
                    char const *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga"};
                    char const *result = tinyfd_openFileDialog(
                        "Select Texture", "", 5, filters, "Image files", 0);
                    if (result) snprintf(texPathBuf, sizeof(texPathBuf), "%s", result);
                }
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

    if (ImGui::CollapsingHeader("Shape", 0)) {
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
            case OBJ_LIGHT: {
                const char *ltNames[] = { "Point", "Directional" };
                int lt = (int)obj->lightType;
                if (ImGui::Combo("Type##light", &lt, ltNames, 2)) {
                    obj->lightType = (LightType)lt;
                }
                float lc[3] = {
                    obj->lightColor.r / 255.0f,
                    obj->lightColor.g / 255.0f,
                    obj->lightColor.b / 255.0f,
                };
                if (ImGui::ColorEdit3("Light Color", lc)) {
                    obj->lightColor = Color{
                        (uint8_t)(lc[0] * 255),
                        (uint8_t)(lc[1] * 255),
                        (uint8_t)(lc[2] * 255),
                        255
                    };
                }
                ImGui::DragFloat("Intensity", &obj->lightIntensity, 0.05f, 0.0f, 10.0f);
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

    // animation scripts
    if (obj && ImGui::CollapsingHeader("Scripts")) {
        for (int s = 0; s < obj->scriptCount; s++) {
            ImGui::PushID(s);
            ImGui::SetNextItemWidth(-90);
            char label[32];
            snprintf(label, sizeof(label), "##Script%d", s);
            ImGui::InputText(label, obj->scriptPaths[s], sizeof(obj->scriptPaths[s]));
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                char const *filters[] = {"*.lua"};
                char const *result = tinyfd_openFileDialog(
                    "Select Script", "", 1, filters, "Lua scripts", 0);
                if (result) snprintf(obj->scriptPaths[s], sizeof(obj->scriptPaths[s]), "%s", result);
            }
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                // remove this script by shifting the rest down
                for (int j = s; j < obj->scriptCount - 1; j++) {
                    memcpy(obj->scriptPaths[j], obj->scriptPaths[j + 1], 256);
                }
                obj->scriptPaths[obj->scriptCount - 1][0] = '\0';
                obj->scriptCount--;
                s--; // re-check this index
            }
            ImGui::PopID();
        }
        if (obj->scriptCount < MAX_SCRIPTS) {
            if (ImGui::Button("+ Add Script")) {
                obj->scriptPaths[obj->scriptCount][0] = '\0';
                obj->scriptCount++;
            }
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
    add_object_button(ui, "Light",      OBJ_LIGHT);

    ImGui::Separator();
    ImGui::Text("Load Model File");
    static char modelPathBuf[256] = {};
    ImGui::SetNextItemWidth(-40);
    ImGui::InputTextWithHint("##ModelPath", "File path (.obj, .gltf, ...)", modelPathBuf, sizeof(modelPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("...##model")) {
        char const *filters[] = {"*.obj", "*.gltf", "*.glb", "*.fbx", "*.iqm"};
        char const *result = tinyfd_openFileDialog(
            "Select Model", "", 5, filters, "3D model files", 0);
        if (result) snprintf(modelPathBuf, sizeof(modelPathBuf), "%s", result);
    }
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

    if (ui->playMode) {
        if (ui->paused) {
            if (ImGui::Button("Resume")) ui->wantPause = true; // toggle pause off
        } else {
            if (ImGui::Button("Pause")) ui->wantPause = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) ui->wantStop = true;
    } else {
        if (ImGui::Button("Play")) ui->wantPlay = true;
    }
    ImGui::SameLine();

    bool unlimited = tl->endFrame >= 999999;
    int sliderMax = unlimited ? 1000 : tl->endFrame;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 330);
    int frame = tl->currentFrame;
    if (ImGui::SliderInt("##Frame", &frame, tl->startFrame, sliderMax)) {
        timeline_set_frame(tl, frame);
    }
    ImGui::SameLine();
    float timeSec = tl->currentFrame / tl->fps;
    if (unlimited) {
        ImGui::Text("Frame: %d (%.1fs)", tl->currentFrame, timeSec);
    } else {
        float totalSec = tl->endFrame / tl->fps;
        ImGui::Text("%d/%d (%.1f/%.1fs)", tl->currentFrame, tl->endFrame, timeSec, totalSec);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Repeat", &ui->repeatPlayback);

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

// ---- Console (Lua scripting) ----

void ui_console(ScriptState *ss, EditorUI *ui) {
    if (!ui->showConsole) return;
    ImGui::Begin("Console", &ui->showConsole);

    // toolbar
    if (ImGui::Button("Clear")) console_clear(&ss->console);

    ImGui::Separator();

    // output log
    float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ConsoleLog", ImVec2(0, -footerHeight), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < ss->console.lineCount; i++) {
        if (strncmp(ss->console.lines[i], "[error]", 7) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextUnformatted(ss->console.lines[i]);
            ImGui::PopStyleColor();
        } else {
            ImGui::TextUnformatted(ss->console.lines[i]);
        }
    }
    if (ss->console.scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        ss->console.scrollToBottom = false;
    }
    ImGui::EndChild();

    // input line
    ImGui::Separator();
    bool reclaimFocus = false;
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##ConsoleInput", ss->console.inputBuf,
                         sizeof(ss->console.inputBuf), inputFlags)) {
        if (ss->console.inputBuf[0]) {
            console_log(&ss->console, "> %s", ss->console.inputBuf);
            script_exec(ss, ss->console.inputBuf);
            ss->console.inputBuf[0] = '\0';
        }
        reclaimFocus = true;
    }
    if (reclaimFocus) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}
