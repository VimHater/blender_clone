#ifndef UI_UI_H
#define UI_UI_H

#include <core/scene.h>
#include <core/camera.h>
#include <core/timeline.h>
#include <core/scripting.h>

struct EditorUI {
    RenderTexture2D viewportRT;      // edit viewport
    RenderTexture2D animViewportRT;  // animation viewport
    int viewportW;
    int viewportH;
    bool viewportHovered;
    bool viewportFocused;
    int activeViewportTab;  // 0 = Edit, 1 = Animation

    // viewport image rect (screen-space, after fit+center)
    float vpImageX, vpImageY;
    float vpImageW, vpImageH;

    float uiScale;
    float lastFontSize;         // last rasterized font size (to detect when rebuild is needed)
    char fontPath[512];

    bool showGrid;
    int gridSize;
    float gridSpacing;

    TransformMode transformMode;
    DrawMode drawMode;
    bool showTimeline;
    bool showConsole;
    bool showHierarchy;
    bool showProperties;
    bool showAddObject;
    bool showCamera;

    bool dockspaceInitialized;
    int dockspaceInitFrames;    // frames remaining to apply post-init focus

    // active camera: 0 = editor camera, otherwise ID of a scene camera object
    uint32_t activeCameraId;

    // gizmo interaction
    GizmoAxis gizmoActiveAxis;
    bool gizmoDragging;
    Vector2 gizmoDragStart;         // screen mouse pos at drag start
    Vector3 gizmoDragObjStart;      // object position/rotation/scale at drag start (first selected)
    Vector3 gizmoDragCenter;        // center of all selected objects at drag start
    Vector3 gizmoDragStarts[MAX_SELECTED]; // per-object start values for multi-select
    uint32_t gizmoDragIds[MAX_SELECTED];   // IDs of objects being dragged
    int gizmoDragCount;                    // number of objects in multi-drag

    // title bar dragging
    bool titleBarDragging;
    Vector2 titleBarDragOffset;
    bool wantClose;       // set by title bar close button
    bool wantSave;        // set by File > Save
    bool wantLoad;        // set by File > Open / Examples
    char loadPath[256];   // path to load
    bool wantPlay;        // set by Play button in timeline
    bool wantStop;        // set by Stop button in timeline
    bool wantPause;       // set by Pause button in timeline
    bool playMode;        // true while animation is running
    bool paused;          // true while animation is paused
    bool repeatPlayback;  // loop animation at end of duration
    bool showSaveAsPopup; // File > Save As popup
    char saveAsName[256]; // filename input buffer
    char currentFilePath[512]; // path of currently open file (for Save)

    bool undoPending;     // set by UI before modifying scene

    // clipboard actions (set by UI, handled by editor)
    bool wantCopy;
    bool wantCut;
    bool wantPaste;
    bool wantDuplicate;
    bool hasClipboard;    // true when clipboard has content (for greying out Paste)

    // error popup
    bool showErrorPopup;
    char errorMessage[256];

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
void ui_properties(Scene *s, Timeline *tl, EditorUI *ui);
void ui_add_object(Scene *s, EditorUI *ui);
void ui_camera(Scene *s, EditorCamera *ec, EditorUI *ui);
void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui);
void ui_console(ScriptState *ss, EditorUI *ui);
void ui_save_as_popup(EditorUI *ui);
void ui_error_popup(EditorUI *ui);
void ui_shortcut_popup();

#endif // UI_UI_H
