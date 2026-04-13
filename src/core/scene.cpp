#include "scene.h"
#include "builtin_objects/teapot.h"
#include <rlgl.h>
#include <cstring>
#include <cstdio>
#ifdef _WIN32
#include <direct.h>
#define platform_getcwd _getcwd
#define platform_chdir  _chdir
#else
#include <unistd.h>
#define platform_getcwd getcwd
#define platform_chdir  chdir
#endif

// ---- ObjectTransform ----

ObjectTransform transform_default() {
    ObjectTransform t;
    t.position = Vector3{0, 0, 0};
    t.rotation = Vector3{0, 0, 0};
    t.scale    = Vector3{1, 1, 1};
    return t;
}

Matrix transform_to_matrix(const ObjectTransform &t) {
    Matrix mat = MatrixIdentity();
    mat = MatrixMultiply(mat, MatrixScale(t.scale.x, t.scale.y, t.scale.z));
    mat = MatrixMultiply(mat, MatrixRotateXYZ(Vector3{
        t.rotation.x * DEG2RAD,
        t.rotation.y * DEG2RAD,
        t.rotation.z * DEG2RAD
    }));
    mat = MatrixMultiply(mat, MatrixTranslate(t.position.x, t.position.y, t.position.z));
    return mat;
}

ObjectTransform transform_lerp(const ObjectTransform &a, const ObjectTransform &b, float t) {
    ObjectTransform r;
    r.position = Vector3Lerp(a.position, b.position, t);
    r.rotation = Vector3Lerp(a.rotation, b.rotation, t);
    r.scale    = Vector3Lerp(a.scale, b.scale, t);
    return r;
}

// ---- ObjectMaterial ----

ObjectMaterial material_default() {
    ObjectMaterial m;
    m.color = WHITE;
    m.texture = Texture2D{0};
    m.hasTexture = false;
    m.texturePath[0] = '\0';
    m.roughness = 0.5f;
    m.metallic = 0.0f;
    return m;
}

// ---- SceneObject ----

SceneObject object_default(const char *name, ObjectType type) {
    SceneObject obj;
    memset(&obj, 0, sizeof(obj));
    snprintf(obj.name, MAX_NAME_LEN, "%s", name);
    obj.type = type;
    obj.active = true;
    obj.visible = true;
    obj.parentIndex = -1;
    obj.transform = transform_default();
    obj.material = material_default();

    // per-type defaults
    obj.cubeSize[0] = 2.0f;
    obj.cubeSize[1] = 2.0f;
    obj.cubeSize[2] = 2.0f;
    obj.sphereRadius = 1.0f;
    obj.sphereRings = 16;
    obj.sphereSlices = 16;
    obj.cylinderRadiusTop = 0.5f;
    obj.cylinderRadiusBottom = 0.5f;
    obj.cylinderHeight = 2.0f;
    // torus/knot defaults (overridden per-type below)
    obj.torusRadius = 0.5f;
    obj.torusSize = 1.0f;
    obj.torusRadSeg = 16;
    obj.torusSides = 30;
    obj.capsuleRadius = 0.5f;
    obj.capsuleHeight = 2.0f;
    obj.polySides = 6;
    obj.polyRadius = 1.0f;
    obj.coneRadius = 0.5f;
    obj.coneHeight = 2.0f;
    obj.coneSlices = 16;
    obj.camFov = 45.0f;
    obj.camNear = 0.1f;
    obj.camFar = 1000.0f;
    obj.camOrtho = false;
    obj.modelLoaded = false;
    obj.keyframeCount = 0;

    if (type == OBJ_KNOT) {
        obj.torusRadius = 1.8f;
        obj.torusSides = 64;
    }

    if (type == OBJ_TEAPOT) {
        obj.model = load_model_from_obj_data(TEAPOT_OBJ_DATA);
        obj.modelLoaded = (obj.model.meshCount > 0);
    }

    return obj;
}

// ---- Scene ----

void scene_init(Scene *s) {
    memset(s, 0, sizeof(*s));
    s->objectCount = 0;
    s->nextId = 1;
    s->selectedCount = 0;
}

// ---- Naming ----

void scene_generate_name(const Scene *s, const char *baseName, char *outName, int outLen) {
    // check if baseName itself is unique
    bool taken = false;
    for (int i = 0; i < s->objectCount; i++) {
        if (strcmp(s->objects[i].name, baseName) == 0) { taken = true; break; }
    }
    if (!taken) {
        snprintf(outName, outLen, "%s", baseName);
        return;
    }
    // try baseName.001, .002, ...
    for (int n = 1; n < 10000; n++) {
        snprintf(outName, outLen, "%s.%03d", baseName, n);
        taken = false;
        for (int i = 0; i < s->objectCount; i++) {
            if (strcmp(s->objects[i].name, outName) == 0) { taken = true; break; }
        }
        if (!taken) return;
    }
    // fallback
    snprintf(outName, outLen, "%s.???", baseName);
}

// ---- Add / Remove ----

int scene_add(Scene *s, const char *name, ObjectType type) {
    if (s->objectCount >= MAX_OBJECTS) return -1;

    // generate unique name before adding to the array
    char uniqueName[MAX_NAME_LEN];
    scene_generate_name(s, name, uniqueName, MAX_NAME_LEN);

    int idx = s->objectCount++;
    s->objects[idx] = object_default(uniqueName, type);
    s->objects[idx].id = s->nextId++;

    return idx;
}

void scene_remove(Scene *s, int index) {
    if (index < 0 || index >= s->objectCount) return;

    uint32_t removedId = s->objects[index].id;

    if (s->objects[index].modelLoaded) {
        UnloadModel(s->objects[index].model);
    }
    if (s->objects[index].material.hasTexture) {
        UnloadTexture(s->objects[index].material.texture);
    }

    for (int i = index; i < s->objectCount - 1; i++) {
        s->objects[i] = s->objects[i + 1];
    }
    s->objectCount--;

    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].parentIndex == index) {
            s->objects[i].parentIndex = -1;
        } else if (s->objects[i].parentIndex > index) {
            s->objects[i].parentIndex--;
        }
    }

    // remove from selection
    for (int i = 0; i < s->selectedCount; i++) {
        if (s->selectedIds[i] == removedId) {
            s->selectedIds[i] = s->selectedIds[--s->selectedCount];
            break;
        }
    }
}

// ---- Selection ----

bool scene_is_selected(const Scene *s, uint32_t id) {
    for (int i = 0; i < s->selectedCount; i++) {
        if (s->selectedIds[i] == id) return true;
    }
    return false;
}

void scene_select(Scene *s, uint32_t id, bool toggle, bool add) {
    if (toggle) {
        // toggle: if already selected, deselect; otherwise add
        for (int i = 0; i < s->selectedCount; i++) {
            if (s->selectedIds[i] == id) {
                s->selectedIds[i] = s->selectedIds[--s->selectedCount];
                return;
            }
        }
        if (s->selectedCount < MAX_SELECTED) {
            s->selectedIds[s->selectedCount++] = id;
        }
    } else if (add) {
        // add to selection without clearing
        if (!scene_is_selected(s, id) && s->selectedCount < MAX_SELECTED) {
            s->selectedIds[s->selectedCount++] = id;
        }
    } else {
        // exclusive select
        s->selectedCount = 1;
        s->selectedIds[0] = id;
    }
}

void scene_select_range(Scene *s, int fromIndex, int toIndex) {
    if (fromIndex > toIndex) { int tmp = fromIndex; fromIndex = toIndex; toIndex = tmp; }
    for (int i = fromIndex; i <= toIndex && i < s->objectCount; i++) {
        if (!scene_is_selected(s, s->objects[i].id) && s->selectedCount < MAX_SELECTED) {
            s->selectedIds[s->selectedCount++] = s->objects[i].id;
        }
    }
}

void scene_deselect_all(Scene *s) {
    s->selectedCount = 0;
}

SceneObject *scene_first_selected(Scene *s) {
    if (s->selectedCount == 0) return nullptr;
    return scene_get_by_id(s, s->selectedIds[0]);
}

int scene_find_by_id(const Scene *s, uint32_t id) {
    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].id == id) return i;
    }
    return -1;
}

SceneObject *scene_get_by_id(Scene *s, uint32_t id) {
    int idx = scene_find_by_id(s, id);
    return (idx >= 0) ? &s->objects[idx] : nullptr;
}

// generate a mesh for a given object type (returns true if mesh was generated)
static bool gen_object_mesh(const SceneObject *obj, Mesh *out) {
    switch (obj->type) {
        case OBJ_CUBE:
            *out = GenMeshCube(obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2]);
            return true;
        case OBJ_SPHERE:
            *out = GenMeshSphere(obj->sphereRadius, obj->sphereRings, obj->sphereSlices);
            return true;
        case OBJ_PLANE:
            *out = GenMeshPlane(obj->cubeSize[0], obj->cubeSize[2], 1, 1);
            return true;
        case OBJ_CYLINDER:
            *out = GenMeshCylinder(obj->cylinderRadiusTop, obj->cylinderHeight, 16);
            return true;
        case OBJ_CONE:
            *out = GenMeshCone(obj->coneRadius, obj->coneHeight, obj->coneSlices);
            return true;
        case OBJ_TORUS:
            *out = GenMeshTorus(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            return true;
        case OBJ_KNOT:
            *out = GenMeshKnot(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            return true;
        case OBJ_POLY:
            *out = GenMeshPoly(obj->polySides, obj->polyRadius);
            return true;
        case OBJ_CAPSULE:
            // GenMeshCapsule is not in raylib, skip
            return false;
        default: return false;
    }
}

// draw object as a model (with optional texture)
static void draw_model_object(const SceneObject *obj, DrawMode mode, Color c) {
    Mesh mesh = {0};
    if (!gen_object_mesh(obj, &mesh)) return;
    Model mdl = LoadModelFromMesh(mesh);
    if (obj->material.hasTexture) {
        mdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = obj->material.texture;
    }
    if (mode == DRAW_SOLID)          DrawModel(mdl, Vector3{0,0,0}, 1.0f, c);
    else if (mode == DRAW_WIREFRAME) DrawModelWires(mdl, Vector3{0,0,0}, 1.0f, c);
    else                             DrawModelPoints(mdl, Vector3{0,0,0}, 1.0f, c);
    // don't unload the texture — it belongs to the scene object
    mdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{0};
    UnloadModel(mdl);
}

static void draw_object(const SceneObject *obj, DrawMode mode) {
    if (!obj->active || !obj->visible) return;

    Matrix mat = transform_to_matrix(obj->transform);
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(mat));

    Color c = obj->material.color;
    bool textured = obj->material.hasTexture;

    // for textured primitives (or mesh-only types), use model-based rendering
    if (textured && obj->type != OBJ_CAMERA && obj->type != OBJ_TEAPOT
        && obj->type != OBJ_MODEL_FILE) {
        draw_model_object(obj, mode, c);
        rlPopMatrix();
        return;
    }

    switch (obj->type) {
        case OBJ_CUBE:
            if (mode == DRAW_SOLID) {
                DrawCube(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], c);
                DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], BLACK);
            } else if (mode == DRAW_WIREFRAME) {
                DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], c);
            } else {
                DrawPoint3D(Vector3{0,0,0}, c);
            }
            break;
        case OBJ_SPHERE:
            if (mode == DRAW_SOLID)
                DrawSphereEx(Vector3{0,0,0}, obj->sphereRadius, obj->sphereRings, obj->sphereSlices, c);
            else if (mode == DRAW_WIREFRAME)
                DrawSphereWires(Vector3{0,0,0}, obj->sphereRadius, obj->sphereRings, obj->sphereSlices, c);
            else
                DrawPoint3D(Vector3{0,0,0}, c);
            break;
        case OBJ_PLANE:
            if (mode == DRAW_SOLID)
                DrawPlane(Vector3{0,0,0}, Vector2{obj->cubeSize[0], obj->cubeSize[2]}, c);
            else if (mode == DRAW_WIREFRAME)
                DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], 0.01f, obj->cubeSize[2], c);
            else
                DrawPoint3D(Vector3{0,0,0}, c);
            break;
        case OBJ_CYLINDER:
            if (mode == DRAW_SOLID)
                DrawCylinder(Vector3{0,0,0}, obj->cylinderRadiusTop, obj->cylinderRadiusBottom,
                             obj->cylinderHeight, 16, c);
            else if (mode == DRAW_WIREFRAME)
                DrawCylinderWires(Vector3{0,0,0}, obj->cylinderRadiusTop, obj->cylinderRadiusBottom,
                                  obj->cylinderHeight, 16, c);
            else
                DrawPoint3D(Vector3{0,0,0}, c);
            break;
        case OBJ_CONE:
            if (mode == DRAW_SOLID)
                DrawCylinder(Vector3{0,0,0}, 0.0f, obj->coneRadius, obj->coneHeight, obj->coneSlices, c);
            else if (mode == DRAW_WIREFRAME)
                DrawCylinderWires(Vector3{0,0,0}, 0.0f, obj->coneRadius, obj->coneHeight, obj->coneSlices, c);
            else
                DrawPoint3D(Vector3{0,0,0}, c);
            break;
        case OBJ_TORUS:
        case OBJ_KNOT:
        case OBJ_POLY:
            draw_model_object(obj, mode, c);
            break;
        case OBJ_CAPSULE:
            if (mode == DRAW_SOLID)
                DrawCapsule(Vector3{0, -obj->capsuleHeight * 0.5f, 0},
                            Vector3{0,  obj->capsuleHeight * 0.5f, 0},
                            obj->capsuleRadius, 16, 8, c);
            else if (mode == DRAW_WIREFRAME)
                DrawCapsuleWires(Vector3{0, -obj->capsuleHeight * 0.5f, 0},
                                 Vector3{0,  obj->capsuleHeight * 0.5f, 0},
                                 obj->capsuleRadius, 16, 8, c);
            else
                DrawPoint3D(Vector3{0,0,0}, c);
            break;
        case OBJ_TEAPOT:
        case OBJ_MODEL_FILE:
            if (obj->modelLoaded) {
                // apply texture to model if present
                Texture2D origTex = {0};
                if (textured) {
                    origTex = obj->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
                    obj->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = obj->material.texture;
                }
                if (mode == DRAW_SOLID)          DrawModel(obj->model, Vector3{0,0,0}, 1.0f, c);
                else if (mode == DRAW_WIREFRAME) DrawModelWires(obj->model, Vector3{0,0,0}, 1.0f, c);
                else                             DrawModelPoints(obj->model, Vector3{0,0,0}, 1.0f, c);
                if (textured) {
                    obj->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = origTex;
                }
            }
            break;
        case OBJ_CAMERA: {
            DrawCube(Vector3{0,0,0}, 0.3f, 0.2f, 0.3f, c);
            float s = 0.4f;
            Vector3 ftl = {-s,  s, -s*2};
            Vector3 ftr = { s,  s, -s*2};
            Vector3 fbl = {-s, -s, -s*2};
            Vector3 fbr = { s, -s, -s*2};
            Vector3 o = {0, 0, 0};
            DrawLine3D(o, ftl, c); DrawLine3D(o, ftr, c);
            DrawLine3D(o, fbl, c); DrawLine3D(o, fbr, c);
            DrawLine3D(ftl, ftr, c); DrawLine3D(ftr, fbr, c);
            DrawLine3D(fbr, fbl, c); DrawLine3D(fbl, ftl, c);
            break;
        }
        default:
            break;
    }

    rlPopMatrix();
}

void scene_draw(const Scene *s, DrawMode mode) {
    for (int i = 0; i < s->objectCount; i++) {
        draw_object(&s->objects[i], mode);
    }
}

static void draw_selection_outline(const SceneObject *obj) {
    if (!obj->active || !obj->visible) return;

    Color sel = ORANGE;
    Matrix mat = transform_to_matrix(obj->transform);
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(mat));

    switch (obj->type) {
        case OBJ_CUBE:
            DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], sel);
            break;
        case OBJ_SPHERE:
            DrawSphereWires(Vector3{0,0,0}, obj->sphereRadius, obj->sphereRings, obj->sphereSlices, sel);
            break;
        case OBJ_PLANE:
            DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], 0.01f, obj->cubeSize[2], sel);
            break;
        case OBJ_CYLINDER:
            DrawCylinderWires(Vector3{0,0,0}, obj->cylinderRadiusTop, obj->cylinderRadiusBottom,
                              obj->cylinderHeight, 16, sel);
            break;
        case OBJ_CONE:
            DrawCylinderWires(Vector3{0,0,0}, 0.0f, obj->coneRadius, obj->coneHeight, obj->coneSlices, sel);
            break;
        case OBJ_TORUS: {
            Mesh m = GenMeshTorus(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            Model md = LoadModelFromMesh(m);
            DrawModelWires(md, Vector3{0,0,0}, 1.0f, sel);
            UnloadModel(md);
            break;
        }
        case OBJ_KNOT: {
            Mesh m = GenMeshKnot(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            Model md = LoadModelFromMesh(m);
            DrawModelWires(md, Vector3{0,0,0}, 1.0f, sel);
            UnloadModel(md);
            break;
        }
        case OBJ_POLY: {
            DrawSphereWires(Vector3{0,0,0}, obj->polyRadius, 8, 8, sel);
            break;
        }
        case OBJ_CAPSULE:
            DrawCapsuleWires(Vector3{0, -obj->capsuleHeight * 0.5f, 0},
                             Vector3{0,  obj->capsuleHeight * 0.5f, 0},
                             obj->capsuleRadius, 16, 8, sel);
            break;
        case OBJ_TEAPOT:
        case OBJ_MODEL_FILE:
            if (obj->modelLoaded) {
                BoundingBox bb = GetModelBoundingBox(obj->model);
                DrawBoundingBox(bb, sel);
            }
            break;
        case OBJ_CAMERA:
            DrawCubeWires(Vector3{0,0,0}, 0.3f, 0.2f, 0.3f, sel);
            break;
        default:
            break;
    }

    rlPopMatrix();
}

void scene_draw_selection(const Scene *s) {
    for (int i = 0; i < s->objectCount; i++) {
        if (scene_is_selected(s, s->objects[i].id)) {
            draw_selection_outline(&s->objects[i]);
        }
    }
}

// ---- Gizmo ----

static const float GIZMO_BASE_LENGTH = 1.4f;
static const float GIZMO_MIN_LENGTH = 1.0f;

// approximate bounding radius of an object (unscaled by gizmo, just object extent)
static float object_radius(const SceneObject *obj) {
    Vector3 s = obj->transform.scale;
    float ms = s.x > s.y ? (s.x > s.z ? s.x : s.z) : (s.y > s.z ? s.y : s.z);
    switch (obj->type) {
        case OBJ_CUBE:      return 0.5f * ms * sqrtf(obj->cubeSize[0]*obj->cubeSize[0] + obj->cubeSize[1]*obj->cubeSize[1] + obj->cubeSize[2]*obj->cubeSize[2]);
        case OBJ_SPHERE:    return obj->sphereRadius * ms;
        case OBJ_PLANE:     return 0.5f * ms * sqrtf(obj->cubeSize[0]*obj->cubeSize[0] + obj->cubeSize[2]*obj->cubeSize[2]);
        case OBJ_CYLINDER:  return fmaxf(fmaxf(obj->cylinderRadiusTop, obj->cylinderRadiusBottom), obj->cylinderHeight * 0.5f) * ms;
        case OBJ_CONE:      return fmaxf(obj->coneRadius, obj->coneHeight * 0.5f) * ms;
        case OBJ_TORUS:
        case OBJ_KNOT:      return (obj->torusSize + obj->torusRadius) * ms;
        case OBJ_CAPSULE:   return fmaxf(obj->capsuleRadius, obj->capsuleHeight * 0.5f + obj->capsuleRadius) * ms;
        case OBJ_POLY:      return obj->polyRadius * ms;
        case OBJ_CAMERA:    return 0.5f;
        case OBJ_TEAPOT:
        case OBJ_MODEL_FILE:
            if (obj->modelLoaded) {
                BoundingBox bb = GetModelBoundingBox(obj->model);
                float dx = (bb.max.x - bb.min.x) * s.x;
                float dy = (bb.max.y - bb.min.y) * s.y;
                float dz = (bb.max.z - bb.min.z) * s.z;
                return 0.5f * sqrtf(dx*dx + dy*dy + dz*dz);
            }
            return 1.0f;
        default: return 1.0f;
    }
}

static float gizmo_length_for(const SceneObject *obj) {
    float r = object_radius(obj);
    float len = r * 0.7f + GIZMO_BASE_LENGTH;
    if (len < GIZMO_MIN_LENGTH) len = GIZMO_MIN_LENGTH;
    return len;
}

void scene_draw_gizmo(const Scene *s, TransformMode mode, GizmoAxis activeAxis) {
    if (s->selectedCount == 0) return;
    // find first selected object
    const SceneObject *obj = nullptr;
    for (int i = 0; i < s->objectCount; i++) {
        if (scene_is_selected(s, s->objects[i].id)) { obj = &s->objects[i]; break; }
    }
    if (!obj) return;

    Vector3 p = obj->transform.position;
    float len = gizmo_length_for(obj);
    float t = len * 0.025f; // thickness scales with length

    Color xCol = (activeAxis == GIZMO_X) ? YELLOW : RED;
    Color yCol = (activeAxis == GIZMO_Y) ? YELLOW : GREEN;
    Color zCol = (activeAxis == GIZMO_Z) ? YELLOW : BLUE;

    if (mode == TMODE_TRANSLATE) {
        // axis lines
        DrawLine3D(p, Vector3{p.x + len, p.y, p.z}, xCol);
        DrawLine3D(p, Vector3{p.x, p.y + len, p.z}, yCol);
        DrawLine3D(p, Vector3{p.x, p.y, p.z + len}, zCol);
        // arrowheads (DrawCylinder draws along +Y, so rotate to point along each axis)
        float coneR = t * 2;
        float coneH = t * 8;
        // X arrow: rotate +Y → +X (rotate -90 around Z)
        rlPushMatrix();
        rlTranslatef(p.x + len, p.y, p.z);
        rlRotatef(-90, 0, 0, 1);
        DrawCylinder(Vector3{0,0,0}, 0.0f, coneR, coneH, 8, xCol);
        rlPopMatrix();
        // Y arrow: already along +Y
        DrawCylinder(Vector3{p.x, p.y + len, p.z}, 0.0f, coneR, coneH, 8, yCol);
        // Z arrow: rotate +Y → +Z (rotate 90 around X)
        rlPushMatrix();
        rlTranslatef(p.x, p.y, p.z + len);
        rlRotatef(90, 1, 0, 0);
        DrawCylinder(Vector3{0,0,0}, 0.0f, coneR, coneH, 8, zCol);
        rlPopMatrix();
    } else if (mode == TMODE_ROTATE) {
        // draw circles for each axis
        DrawCircle3D(p, len * 0.8f, Vector3{0, 1, 0}, 0, xCol);   // X rotation = around X = YZ plane
        DrawCircle3D(p, len * 0.8f, Vector3{1, 0, 0}, 90, yCol);   // Y rotation
        DrawCircle3D(p, len * 0.8f, Vector3{0, 0, 1}, 90, zCol);   // Z rotation
    } else if (mode == TMODE_SCALE) {
        // axis lines with cubes at end
        DrawLine3D(p, Vector3{p.x + len, p.y, p.z}, xCol);
        DrawLine3D(p, Vector3{p.x, p.y + len, p.z}, yCol);
        DrawLine3D(p, Vector3{p.x, p.y, p.z + len}, zCol);
        float cs = t * 4;
        DrawCube(Vector3{p.x + len, p.y, p.z}, cs, cs, cs, xCol);
        DrawCube(Vector3{p.x, p.y + len, p.z}, cs, cs, cs, yCol);
        DrawCube(Vector3{p.x, p.y, p.z + len}, cs, cs, cs, zCol);
    }
}

GizmoAxis gizmo_hit_test(const SceneObject *obj, Ray ray, TransformMode mode) {
    Vector3 gizmoPos = obj->transform.position;
    float len = gizmo_length_for(obj);
    float r = len * 0.075f; // hit radius scales with length
    (void)mode;

    // test each axis as a thin bounding box
    BoundingBox xBox = {
        Vector3{gizmoPos.x, gizmoPos.y - r, gizmoPos.z - r},
        Vector3{gizmoPos.x + len, gizmoPos.y + r, gizmoPos.z + r}
    };
    BoundingBox yBox = {
        Vector3{gizmoPos.x - r, gizmoPos.y, gizmoPos.z - r},
        Vector3{gizmoPos.x + r, gizmoPos.y + len, gizmoPos.z + r}
    };
    BoundingBox zBox = {
        Vector3{gizmoPos.x - r, gizmoPos.y - r, gizmoPos.z},
        Vector3{gizmoPos.x + r, gizmoPos.y + r, gizmoPos.z + len}
    };

    float bestDist = 1e30f;
    GizmoAxis best = GIZMO_NONE;

    RayCollision col = GetRayCollisionBox(ray, xBox);
    if (col.hit && col.distance < bestDist) { bestDist = col.distance; best = GIZMO_X; }
    col = GetRayCollisionBox(ray, yBox);
    if (col.hit && col.distance < bestDist) { bestDist = col.distance; best = GIZMO_Y; }
    col = GetRayCollisionBox(ray, zBox);
    if (col.hit && col.distance < bestDist) { bestDist = col.distance; best = GIZMO_Z; }

    return best;
}

// cached built-in models for preview
static Model s_teapotPreview = {0};
static bool  s_teapotPreviewLoaded = false;

static Model get_teapot_preview() {
    if (!s_teapotPreviewLoaded) {
        s_teapotPreview = load_model_from_obj_data(TEAPOT_OBJ_DATA);
        s_teapotPreviewLoaded = true;
    }
    return s_teapotPreview;
}

void scene_draw_preview(ObjectType type, Vector3 pos) {
    Color ghost = Color{0, 200, 255, 120};
    Color wire  = Color{0, 200, 255, 200};

    // use default object values so preview matches actual placed object
    SceneObject d = object_default("_preview", type);

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);

    switch (type) {
        case OBJ_CUBE:
            DrawCube(Vector3{0,0,0}, d.cubeSize[0], d.cubeSize[1], d.cubeSize[2], ghost);
            DrawCubeWires(Vector3{0,0,0}, d.cubeSize[0], d.cubeSize[1], d.cubeSize[2], wire);
            break;
        case OBJ_SPHERE:
            DrawSphereWires(Vector3{0,0,0}, d.sphereRadius, d.sphereRings, d.sphereSlices, wire);
            break;
        case OBJ_PLANE:
            DrawPlane(Vector3{0,0,0}, Vector2{d.cubeSize[0], d.cubeSize[2]}, ghost);
            break;
        case OBJ_CYLINDER:
            DrawCylinderWires(Vector3{0,0,0}, d.cylinderRadiusTop, d.cylinderRadiusBottom, d.cylinderHeight, 16, wire);
            break;
        case OBJ_CONE:
            DrawCylinderWires(Vector3{0,0,0}, 0.0f, d.coneRadius, d.coneHeight, d.coneSlices, wire);
            break;
        case OBJ_TORUS: {
            Mesh m = GenMeshTorus(d.torusRadius, d.torusSize, d.torusRadSeg, d.torusSides);
            Model md = LoadModelFromMesh(m);
            DrawModelWires(md, Vector3{0,0,0}, 1.0f, wire);
            UnloadModel(md);
            break;
        }
        case OBJ_KNOT: {
            Mesh m = GenMeshKnot(d.torusRadius, d.torusSize, d.torusRadSeg, d.torusSides);
            Model md = LoadModelFromMesh(m);
            DrawModelWires(md, Vector3{0,0,0}, 1.0f, wire);
            UnloadModel(md);
            break;
        }
        case OBJ_CAPSULE: {
            float halfH = d.capsuleHeight / 2.0f - d.capsuleRadius;
            DrawCapsuleWires(Vector3{0, -halfH, 0}, Vector3{0, halfH, 0}, d.capsuleRadius, 16, 8, wire);
            break;
        }
        case OBJ_POLY:
            DrawSphereWires(Vector3{0,0,0}, d.polyRadius, 8, d.polySides, wire);
            break;
        case OBJ_TEAPOT: {
            Model m = get_teapot_preview();
            if (m.meshCount > 0) {
                DrawModel(m, Vector3{0,0,0}, 1.0f, ghost);
            }
            break;
        }
        case OBJ_CAMERA:
            DrawCubeWires(Vector3{0,0,0}, 0.3f, 0.2f, 0.3f, wire);
            break;
        default:
            break;
    }

    rlPopMatrix();
}

int scene_get_children(const Scene *s, int parentIndex, int *outChildren, int maxOut) {
    int count = 0;
    for (int i = 0; i < s->objectCount && count < maxOut; i++) {
        if (s->objects[i].parentIndex == parentIndex) {
            outChildren[count++] = i;
        }
    }
    return count;
}

// ---- Built-in Model Loading ----

Model load_model_from_obj_data(const char *objData) {
    // write to a unique temp file, load, delete
    char tmpPath[L_tmpnam + 8];
    tmpnam(tmpPath);
    strncat(tmpPath, ".obj", sizeof(tmpPath) - strlen(tmpPath) - 1);

    FILE *f = fopen(tmpPath, "w");
    if (f) {
        fputs(objData, f);
        fclose(f);
    }
    Model m = LoadModel(tmpPath);
    remove(tmpPath);
    return m;
}

// ---- Model Loading ----
// TODO: support .obj with .mtl files (raylib hangs loading many textures via mtl;
//       need to strip mtllib, load geometry only, then apply textures manually)

int scene_add_model(Scene *s, const char *filePath) {
    if (s->objectCount >= MAX_OBJECTS) return -1;

    // extract filename for display name (strip path)
    const char *baseName = filePath;
    for (const char *p = filePath; *p; p++) {
        if (*p == '/' || *p == '\\') baseName = p + 1;
    }

    int idx = scene_add(s, baseName, OBJ_MODEL_FILE);
    if (idx < 0) return -1;

    SceneObject *obj = &s->objects[idx];
    snprintf(obj->modelPath, sizeof(obj->modelPath), "%s", filePath);

    // save/restore CWD — raylib's LoadModel changes it to resolve textures
    char savedCwd[1024];
    platform_getcwd(savedCwd, sizeof(savedCwd));
    obj->model = LoadModel(filePath);
    platform_chdir(savedCwd);

    obj->modelLoaded = (obj->model.meshCount > 0);
    printf("[MODEL] meshes=%d, materials=%d, loaded=%d\n",
           obj->model.meshCount, obj->model.materialCount, obj->modelLoaded);

    return idx;
}

// ---- Texture / Bitmap ----

Texture2D load_bitmap(const char *path) {
    return LoadTexture(path);
}

void object_set_texture(SceneObject *obj, const char *path) {
    if (obj->material.hasTexture) {
        UnloadTexture(obj->material.texture);
    }
    obj->material.texture = LoadTexture(path);
    obj->material.hasTexture = (obj->material.texture.id != 0);
    if (obj->material.hasTexture) {
        snprintf(obj->material.texturePath, sizeof(obj->material.texturePath), "%s", path);
    } else {
        obj->material.texturePath[0] = '\0';
    }
}

void object_set_texture_builtin(SceneObject *obj, const char *name,
                                const unsigned char *data, unsigned int len) {
    if (obj->material.hasTexture) {
        UnloadTexture(obj->material.texture);
    }
    Image img = LoadImageFromMemory(".png", data, len);
    obj->material.texture = LoadTextureFromImage(img);
    UnloadImage(img);
    obj->material.hasTexture = (obj->material.texture.id != 0);
    if (obj->material.hasTexture) {
        snprintf(obj->material.texturePath, sizeof(obj->material.texturePath), "[built-in] %s", name);
    } else {
        obj->material.texturePath[0] = '\0';
    }
}

void object_clear_texture(SceneObject *obj) {
    if (obj->material.hasTexture) {
        UnloadTexture(obj->material.texture);
        obj->material.texture = Texture2D{0};
        obj->material.hasTexture = false;
        obj->material.texturePath[0] = '\0';
    }
}
