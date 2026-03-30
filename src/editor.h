#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "imgui.h"

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MAX_OBJECTS        4096
#define MAX_NAME_LEN       64
#define MAX_MATERIALS      256
#define MAX_CHILDREN       64

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

typedef enum {
    VIEWPORT_SOLID      = 0,
    VIEWPORT_WIREFRAME  = 1,
    VIEWPORT_UNLIT      = 2,
    VIEWPORT_MATERIAL   = 3,
} ViewportShadingMode;

typedef enum {
    GIZMO_NONE      = 0,
    GIZMO_TRANSLATE = 1,
    GIZMO_ROTATE    = 2,
    GIZMO_SCALE     = 3,
} GizmoMode;

typedef enum {
    EDITOR_OBJECT = 0,   // normal selection / transform
    EDITOR_EDIT   = 1,   // mesh edit mode
} EditorMode;

// ---------------------------------------------------------------------------
// transform_wrapper
// ---------------------------------------------------------------------------

typedef struct {
    Vector3    location;
    Vector3    rotation_euler;  // degrees XYZ — used for ImGui display
    Quaternion rotation_quat;  // authoritative rotation
    Vector3    scale;
    Matrix     local_matrix;   // cached — rebuilt when dirty == true
    Matrix     world_matrix;   // cached after scene_resolve_world_matrices()
    bool       dirty;
} transform_wrapper;

// ---------------------------------------------------------------------------
// Material
// Wraps raylib Material with editor-facing metadata for ImGui panels
// ---------------------------------------------------------------------------

typedef struct {
    char      name[MAX_NAME_LEN];
    Material  rl_material;          // raylib Material — passed directly to DrawModel()
    Color     base_color;           // mirrors MAP_ALBEDO tint, edited via ImGui
    float     metallic;             // [0,1] — push to shader uniform on change
    float     roughness;            // [0,1] — push to shader uniform on change
    float     emission_strength;
    Color     emission_color;
    bool      double_sided;
} SceneMaterial;

// ---------------------------------------------------------------------------
// Scene object
// Every visible object is a Model loaded via LoadModel() / LoadModelFromMesh().
// Lights and cameras are separate arrays — they are not scene objects.
// ---------------------------------------------------------------------------

typedef struct {
    uint32_t    id;                     // unique, never 0
    char        name[MAX_NAME_LEN];
    char        source_path[256];       // file path passed to LoadModel(), empty if procedural

    Model       model;                  // raylib Model — owns GPU buffers
    BoundingBox aabb;                   // world-space, recomputed on transform change
    uint32_t    material_id;            // index into Scene.materials[]

    transform_wrapper   transform;

    // Hierarchy
    uint32_t    parent_id;              // 0 = no parent
    uint32_t    children[MAX_CHILDREN];
    uint32_t    child_count;

    // Editor state
    bool        visible;
    bool        selected;
    bool        hide_render;
    bool        cast_shadow;
    bool        receive_shadow;
    Color       wire_color;             // per-object wireframe tint
} SceneObject;

// ---------------------------------------------------------------------------
// Light  (not a SceneObject — no mesh, lives in its own array)
// ---------------------------------------------------------------------------

typedef enum {
    LIGHT_POINT = 0,
    LIGHT_SUN   = 1,
    LIGHT_SPOT  = 2,
} LightType;

typedef struct {
    uint32_t   id;
    char       name[MAX_NAME_LEN];
    LightType  type;
    Vector3    position;
    Vector3    direction;           // sun / spot
    Color      color;
    float      intensity;
    float      range;               // point / spot attenuation radius
    float      spot_angle;          // degrees, spot only
    float      spot_softness;       // [0,1]
    bool       enabled;
    int        shader_slot;         // index in the rlights uniform array (-1 = unassigned)
} SceneLight;

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

#define MAX_LIGHTS  16

typedef struct {
    // Objects — the flat array your render loop iterates
    SceneObject   objects[MAX_OBJECTS];
    uint32_t      object_count;
    uint32_t      next_object_id;       // monotonic, starts at 1

    uint32_t      active_object_id;     // 0 = none selected

    // Lights
    SceneLight    lights[MAX_LIGHTS];
    uint32_t      light_count;
    uint32_t      next_light_id;

    // Materials
    SceneMaterial materials[MAX_MATERIALS];
    uint32_t      material_count;

    // Scene-wide render settings
    Camera3D      active_camera;        // the final output camera (set by viewport or scene cam object)
    Color         ambient_color;
    float         ambient_intensity;

    Shader        default_shader;       // solid-mode PBR shader, shared across all models
    Shader        wireframe_shader;
} Scene;

// ---------------------------------------------------------------------------
// Viewport
// Rendered off-screen into a RenderTexture2D, then shown via ImGui::Image()
// ---------------------------------------------------------------------------

typedef struct {
    int                 width, height;
    RenderTexture2D     render_target;

    Camera3D            camera;         // editor orbit/fly camera (not a scene object)

    ViewportShadingMode shading_mode;
    GizmoMode           gizmo_mode;
    EditorMode          editor_mode;

    float               grid_size;
    int                 grid_subdivisions;
    bool                show_grid;
    bool                show_gizmos;
    bool                show_stats;

    // Orbit state
    float               orbit_yaw;      // degrees
    float               orbit_pitch;    // degrees
    float               orbit_distance;
    Vector3             orbit_target;

    // Mouse
    Ray                 mouse_ray;      // rebuilt each frame
    bool                is_hovered;     // ImGui window hovered — gates input
    bool                is_focused;
} Viewport;

// ---------------------------------------------------------------------------
// ImGui panel / layout state
// ---------------------------------------------------------------------------

typedef struct {
    bool  show_outliner;
    bool  show_properties;
    bool  show_timeline;
    bool  show_toolbar;
    bool  show_stats_overlay;

    int   active_properties_tab;    // 0=Object 1=Material 2=Scene 3=Render

    float left_panel_width;
    float right_panel_width;
    float bottom_panel_height;
} EditorUI;

// ---------------------------------------------------------------------------
// Top-level application state
// ---------------------------------------------------------------------------

typedef struct {
    Scene      scene;
    Viewport   viewport;
    EditorUI   ui;
    double     delta_time;
    uint64_t   frame_count;
} AppState;

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

void  app_init(AppState *app, int window_width, int window_height, const char *title);
void  app_shutdown(AppState *app);
void  app_run(AppState *app);

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------

void  scene_init(Scene *scene);
void  scene_clear(Scene *scene);    // calls UnloadModel() on all objects, frees lights

// ---------------------------------------------------------------------------
// Object management
// All mesh objects are created via LoadModel() or LoadModelFromMesh()
// Returns new object id, or 0 on failure
// ---------------------------------------------------------------------------

// Load from file (.obj, .glb, .gltf, etc.) — calls LoadModel() internally
uint32_t      scene_load_model(Scene *scene, const char *path, const char *name);

// Create from a procedural raylib Mesh — calls LoadModelFromMesh() internally
uint32_t      scene_add_mesh(Scene *scene, Mesh rl_mesh, const char *name);

// Procedural primitives — thin wrappers around scene_add_mesh + GenMesh*()
uint32_t      scene_add_cube(Scene *scene, const char *name);
uint32_t      scene_add_sphere(Scene *scene, const char *name);
uint32_t      scene_add_plane(Scene *scene, const char *name);
uint32_t      scene_add_cylinder(Scene *scene, const char *name);

void          scene_remove_object(Scene *scene, uint32_t id);   // calls UnloadModel()
SceneObject  *scene_get_object(Scene *scene, uint32_t id);      // NULL if not found
void          scene_set_parent(Scene *scene, uint32_t id, uint32_t parent_id);

// ---------------------------------------------------------------------------
// Light management
// ---------------------------------------------------------------------------

uint32_t    scene_add_light(Scene *scene, LightType type, const char *name);
void        scene_remove_light(Scene *scene, uint32_t id);
SceneLight *scene_get_light(Scene *scene, uint32_t id);

// Push all enabled lights into the shader uniforms (call once per frame before draw)
void        scene_sync_lights_to_shader(Scene *scene, Shader shader);

// ---------------------------------------------------------------------------
// Material management
// ---------------------------------------------------------------------------

uint32_t       scene_add_material(Scene *scene, const char *name);
SceneMaterial *scene_get_material(Scene *scene, uint32_t id);

// Apply material to object: sets obj->material_id and updates obj->model.materials[0]
void           scene_apply_material(Scene *scene, uint32_t obj_id, uint32_t mat_id);

// Push SceneMaterial fields (metallic, roughness, etc.) into shader uniforms
void           scene_sync_material_to_shader(SceneMaterial *mat);

// ---------------------------------------------------------------------------
// transform_wrapper helpers
// ---------------------------------------------------------------------------

void  transform_mark_dirty(transform_wrapper *t);
void  transform_recompute_local(transform_wrapper *t);
void  scene_resolve_world_matrices(Scene *scene);   // propagates parent -> child

// Apply a world-space Matrix back into a transform_wrapper (used by gizmo drag)
void  transform_from_matrix(transform_wrapper *t, Matrix m);

// ---------------------------------------------------------------------------
// Viewport helpers
// ---------------------------------------------------------------------------

void      viewport_init(Viewport *vp, int width, int height);
void      viewport_resize(Viewport *vp, int width, int height);  // recreates RenderTexture2D
void      viewport_free(Viewport *vp);

void      viewport_handle_input(Viewport *vp, AppState *app);
void      viewport_update_camera(Viewport *vp);

// Ray-cast pick against object AABBs — returns object id or 0
uint32_t  viewport_pick(Viewport *vp, Scene *scene, Vector2 mouse_pos);

// ---------------------------------------------------------------------------
// Render
// Renders into vp->render_target using BeginTextureMode / BeginMode3D
// ---------------------------------------------------------------------------

void  scene_render(Scene *scene, Viewport *vp);

// Draw a single object — called inside scene_render's loop:
//   for (uint32_t i = 0; i < scene->object_count; i++)
//       object_draw(&scene->objects[i], scene, vp);
void  object_draw(SceneObject *obj, Scene *scene, Viewport *vp);

void  scene_draw_selection_outlines(Scene *scene, Viewport *vp);
void  gizmo_draw(Scene *scene, Viewport *vp);

// ---------------------------------------------------------------------------
// ImGui panels — call between ImGui::NewFrame() ... ImGui::Render()
// ---------------------------------------------------------------------------

void  ui_draw_main_menu_bar(AppState *app);
void  ui_draw_outliner(AppState *app);
void  ui_draw_properties(AppState *app);
void  ui_draw_toolbar(AppState *app);
void  ui_draw_timeline(AppState *app);
void  ui_draw_viewport_overlay(AppState *app);
void  ui_draw_viewport_panel(AppState *app);    // wraps render_target in an ImGui window
void  ui_draw_all(AppState *app);
