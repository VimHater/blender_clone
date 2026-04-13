#ifndef UI_UI_H
#define UI_UI_H

#include "core/scene.h"
#include "core/camera.h"
#include "core/timeline.h"

struct EditorUI {
    RenderTexture2D viewportRT;
    int viewportW;
    int viewportH;
    bool viewportHovered;
    bool viewportFocused;

    // viewport image rect (screen-space, after fit+center)
    float vpImageX, vpImageY;
    float vpImageW, vpImageH;

    float uiScale;

    bool showGrid;
    int gridSize;
    float gridSpacing;

    TransformMode transformMode;
    bool showTimeline;
    bool showHierarchy;
    bool showProperties;
    bool showAddObject;
    bool showCamera;

    bool dockspaceInitialized;

    // placement mode
    bool placementMode;
    ObjectType placementType;
    Vector3 placementPos;
    bool placementValid;    // mouse is over viewport and hit the ground plane
};

void ui_init(EditorUI *ui, int vpW, int vpH);
void ui_update_layout(EditorUI *ui);
void ui_shutdown(EditorUI *ui);

void ui_dockspace(EditorUI *ui);
void ui_menu_bar(Scene *s, EditorCamera *ec, Timeline *tl, EditorUI *ui);
void ui_viewport(EditorUI *ui);
void ui_hierarchy(Scene *s, EditorUI *ui);
void ui_properties(Scene *s, EditorUI *ui);
void ui_add_object(Scene *s, EditorUI *ui);
void ui_camera(EditorCamera *ec, EditorUI *ui);
void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui);

#endif // UI_UI_H
