#ifndef BLENDER_CLONE_H
#define BLENDER_CLONE_H

#include "raylib.h"
#include "raymath.h"
#include <stdint.h>

// ---- Constants ----

#define MAX_OBJECTS      256
#define MAX_KEYFRAMES    512
#define MAX_NAME_LEN     64
#define MAX_CHILDREN     64

// ---- Enums ----

enum ObjectType {
    OBJ_NONE = 0,
    OBJ_CUBE,
    OBJ_SPHERE,
    OBJ_PLANE,
    OBJ_CYLINDER,
    OBJ_MODEL_FILE,
};

enum TransformMode {
    TMODE_TRANSLATE,
    TMODE_ROTATE,
    TMODE_SCALE,
};

enum PlaybackState {
    PLAYBACK_STOPPED,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED,
};

// ---- Core Structs ----

struct ObjectTransform {
    Vector3 position;
    Vector3 rotation;   // euler angles in degrees
    Vector3 scale;
};

ObjectTransform transform_default();
Matrix transform_to_matrix(const ObjectTransform &t);
ObjectTransform transform_lerp(const ObjectTransform &a, const ObjectTransform &b, float t);

struct ObjectMaterial {
    Color color;
    Texture2D texture;
    bool hasTexture;
    float roughness;
    float metallic;
};

ObjectMaterial material_default();

struct Keyframe {
    int frame;
    ObjectTransform transform;
};

struct SceneObject {
    char name[MAX_NAME_LEN];
    ObjectType type;
    bool active;
    bool visible;
    int parentIndex;        // -1 = root

    ObjectTransform transform;
    ObjectMaterial material;

    // primitive params
    float cubeSize[3];      // w, h, d for cubes
    float sphereRadius;
    float cylinderRadiusTop;
    float cylinderRadiusBottom;
    float cylinderHeight;
    int sphereRings;
    int sphereSlices;

    // loaded model (OBJ_MODEL_FILE)
    Model model;
    bool modelLoaded;
    char modelPath[256];

    // animation keyframes
    Keyframe keyframes[MAX_KEYFRAMES];
    int keyframeCount;
};

SceneObject object_default(const char *name, ObjectType type);

// ---- Scene ----

struct Scene {
    SceneObject objects[MAX_OBJECTS];
    int objectCount;
    int selectedIndex;      // -1 = none
};

void   scene_init(Scene *s);
int    scene_add(Scene *s, const char *name, ObjectType type);
void   scene_remove(Scene *s, int index);
SceneObject *scene_selected(Scene *s);
void   scene_draw(const Scene *s);
int    scene_get_children(const Scene *s, int parentIndex, int *outChildren, int maxOut);

// ---- Editor Camera ----

struct EditorCamera {
    Camera3D cam;
    Vector3 target;
    float distance;
    float yaw;              // radians
    float pitch;            // radians
    float zoomSpeed;
    float panSpeed;
    float orbitSpeed;
    float fov;
    bool ortho;
};

void editor_camera_init(EditorCamera *ec);
void editor_camera_update(EditorCamera *ec, bool inputAllowed);
void editor_camera_sync(EditorCamera *ec);     // rebuild cam from yaw/pitch/distance
void editor_camera_focus(EditorCamera *ec, Vector3 point, float dist);

// ---- Raycasting ----

struct RayHitResult {
    bool hit;
    int objectIndex;
    Vector3 point;
    Vector3 normal;
    float distance;
};

RayHitResult raycast_scene(const Scene *s, Ray ray);
RayHitResult raycast_from_mouse(const Scene *s, const EditorCamera *ec, Vector2 mousePos,
                                int vpX, int vpY, int vpW, int vpH);

// ---- Timeline / Animation ----

struct Timeline {
    int currentFrame;
    int startFrame;
    int endFrame;
    float fps;
    PlaybackState state;
    double lastTickTime;
};

void timeline_init(Timeline *tl);
void timeline_update(Timeline *tl);
void timeline_play(Timeline *tl);
void timeline_pause(Timeline *tl);
void timeline_stop(Timeline *tl);
void timeline_set_frame(Timeline *tl, int frame);

void keyframe_insert(SceneObject *obj, int frame);
void keyframe_remove(SceneObject *obj, int frame);
void keyframe_evaluate(const SceneObject *obj, int frame, ObjectTransform *outTransform);

// ---- Shadow / Lighting ----

struct ShadowMap {
    RenderTexture2D depthTarget;
    int resolution;
    Camera3D lightCam;
    Shader depthShader;
    Shader shadowShader;
    bool initialized;
};

void shadowmap_init(ShadowMap *sm, int resolution);
void shadowmap_begin(ShadowMap *sm, Vector3 lightDir);
void shadowmap_end(ShadowMap *sm);
void shadowmap_unload(ShadowMap *sm);

// ---- Texture / Bitmap ----

Texture2D load_bitmap(const char *path);
void      object_set_texture(SceneObject *obj, const char *path);
void      object_clear_texture(SceneObject *obj);

// ---- UI Panels ----

struct EditorUI {
    RenderTexture2D viewportRT;
    int viewportW;
    int viewportH;
    bool viewportHovered;
    bool viewportFocused;

    bool showGrid;
    int gridSize;
    float gridSpacing;

    TransformMode transformMode;
    bool showTimeline;
    bool showHierarchy;
    bool showProperties;
};

void ui_init(EditorUI *ui, int vpW, int vpH);
void ui_shutdown(EditorUI *ui);

void ui_menu_bar(Scene *s, EditorCamera *ec, Timeline *tl, EditorUI *ui);
void ui_viewport(EditorUI *ui);
void ui_hierarchy(Scene *s, EditorUI *ui);
void ui_properties(Scene *s, EditorUI *ui);
void ui_timeline(Scene *s, Timeline *tl, EditorUI *ui);

// ---- Editor (top-level) ----

struct Editor {
    Scene scene;
    EditorCamera camera;
    Timeline timeline;
    EditorUI ui;
    ShadowMap shadowMap;
};

void editor_init(Editor *ed, int screenW, int screenH);
void editor_update(Editor *ed);
void editor_draw(Editor *ed);
void editor_shutdown(Editor *ed);

#endif // BLENDER_CLONE_H
