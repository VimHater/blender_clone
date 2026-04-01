#ifndef UI_UI_H
#define UI_UI_H

#include "core/scene.h"
#include "core/camera.h"
#include "core/timeline.h"

// Layout proportions (fraction of screen)
#define UI_HIERARCHY_WIDTH_RATIO  0.15625f  // 300/1920
#define UI_PROPERTIES_WIDTH_RATIO 0.18229f  // 350/1920
#define UI_TIMELINE_HEIGHT_RATIO  0.23148f  // 250/1080
#define UI_MENUBAR_HEIGHT         50.0f

struct UILayout {
    float screenW;
    float screenH;
    float menuBarH;
    float hierarchyW;
    float propertiesW;
    float timelineH;
    float viewportX, viewportY, viewportW, viewportH;
};

struct EditorUI {
    RenderTexture2D viewportRT;
    int viewportW;
    int viewportH;
    bool viewportHovered;
    bool viewportFocused;

    UILayout layout;

    bool showGrid;
    int gridSize;
    float gridSpacing;

    TransformMode transformMode;
    bool showTimeline;
    bool showHierarchy;
    bool showProperties;
};

void ui_init(EditorUI *ui, int vpW, int vpH);
void ui_update_layout(EditorUI *ui);
void ui_shutdown(EditorUI *ui);

void ui_menu_bar(Scene *s, EditorCamera *ec, Timeline *tl, EditorUI *ui);
void ui_viewport(EditorUI *ui);
void ui_hierarchy(Scene *s, EditorUI *ui);
void ui_properties(Scene *s, EditorUI *ui);
void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui);

#endif // UI_UI_H
