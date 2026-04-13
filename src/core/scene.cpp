#include "scene.h"
#include "builtin_objects/teapot.h"
#include <rlgl.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>

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
    obj.torusRadius = 1.0f;
    obj.torusSize = 0.3f;
    obj.torusRadSeg = 16;
    obj.torusSides = 16;
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

static void draw_object(const SceneObject *obj) {
    if (!obj->active || !obj->visible) return;

    Matrix mat = transform_to_matrix(obj->transform);
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(mat));

    Color c = obj->material.color;

    switch (obj->type) {
        case OBJ_CUBE:
            DrawCube(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], c);
            DrawCubeWires(Vector3{0,0,0}, obj->cubeSize[0], obj->cubeSize[1], obj->cubeSize[2], BLACK);
            break;
        case OBJ_SPHERE:
            DrawSphereEx(Vector3{0,0,0}, obj->sphereRadius, obj->sphereRings, obj->sphereSlices, c);
            break;
        case OBJ_HEMISPHERE:
            DrawSphereEx(Vector3{0,0,0}, obj->sphereRadius, obj->sphereRings, obj->sphereSlices, c);
            break;
        case OBJ_PLANE:
            DrawPlane(Vector3{0,0,0}, Vector2{obj->cubeSize[0], obj->cubeSize[2]}, c);
            break;
        case OBJ_CYLINDER:
            DrawCylinder(Vector3{0,0,0}, obj->cylinderRadiusTop, obj->cylinderRadiusBottom,
                         obj->cylinderHeight, 16, c);
            break;
        case OBJ_CONE:
            DrawCylinder(Vector3{0,0,0}, 0.0f, obj->coneRadius, obj->coneHeight, obj->coneSlices, c);
            break;
        case OBJ_TORUS: {
            Mesh mesh = GenMeshTorus(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            Model mdl = LoadModelFromMesh(mesh);
            DrawModel(mdl, Vector3{0,0,0}, 1.0f, c);
            UnloadModel(mdl);
            break;
        }
        case OBJ_KNOT: {
            Mesh mesh = GenMeshKnot(obj->torusRadius, obj->torusSize, obj->torusRadSeg, obj->torusSides);
            Model mdl = LoadModelFromMesh(mesh);
            DrawModel(mdl, Vector3{0,0,0}, 1.0f, c);
            UnloadModel(mdl);
            break;
        }
        case OBJ_CAPSULE:
            DrawCapsule(Vector3{0, -obj->capsuleHeight * 0.5f, 0},
                        Vector3{0,  obj->capsuleHeight * 0.5f, 0},
                        obj->capsuleRadius, 16, 8, c);
            break;
        case OBJ_POLY: {
            Mesh mesh = GenMeshPoly(obj->polySides, obj->polyRadius);
            Model mdl = LoadModelFromMesh(mesh);
            DrawModel(mdl, Vector3{0,0,0}, 1.0f, c);
            UnloadModel(mdl);
            break;
        }
        case OBJ_TEAPOT:
        case OBJ_MODEL_FILE:
            if (obj->modelLoaded) {
                DrawModel(obj->model, Vector3{0,0,0}, 1.0f, c);
            }
            break;
        case OBJ_CAMERA: {
            // draw camera gizmo: a small box body + frustum lines
            DrawCube(Vector3{0,0,0}, 0.3f, 0.2f, 0.3f, c);
            // lens direction is -Z in local space
            float s = 0.4f;
            Vector3 ftl = {-s,  s, -s*2};  // far top left
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

void scene_draw(const Scene *s) {
    for (int i = 0; i < s->objectCount; i++) {
        draw_object(&s->objects[i]);
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
        case OBJ_HEMISPHERE:
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
        case OBJ_TORUS:
        case OBJ_KNOT:
        case OBJ_POLY: {
            // approximate with a bounding sphere wire
            DrawSphereWires(Vector3{0,0,0}, obj->torusRadius + obj->torusSize, 8, 8, sel);
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

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);

    switch (type) {
        case OBJ_CUBE:
            DrawCube(Vector3{0,0,0}, 2, 2, 2, ghost);
            DrawCubeWires(Vector3{0,0,0}, 2, 2, 2, wire);
            break;
        case OBJ_SPHERE:
        case OBJ_HEMISPHERE:
            DrawSphereWires(Vector3{0,0,0}, 1.0f, 16, 16, wire);
            break;
        case OBJ_PLANE:
            DrawPlane(Vector3{0,0,0}, Vector2{2, 2}, ghost);
            break;
        case OBJ_CYLINDER:
            DrawCylinderWires(Vector3{0,0,0}, 0.5f, 0.5f, 2.0f, 16, wire);
            break;
        case OBJ_CONE:
            DrawCylinderWires(Vector3{0,0,0}, 0.0f, 0.5f, 2.0f, 16, wire);
            break;
        case OBJ_TORUS:
            DrawSphereWires(Vector3{0,0,0}, 1.3f, 8, 8, wire);
            break;
        case OBJ_KNOT:
            DrawSphereWires(Vector3{0,0,0}, 1.3f, 8, 8, wire);
            break;
        case OBJ_CAPSULE:
            DrawCapsuleWires(Vector3{0, -1, 0}, Vector3{0, 1, 0}, 0.5f, 16, 8, wire);
            break;
        case OBJ_POLY:
            DrawSphereWires(Vector3{0,0,0}, 1.0f, 8, 8, wire);
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
    getcwd(savedCwd, sizeof(savedCwd));
    obj->model = LoadModel(filePath);
    chdir(savedCwd);

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
}

void object_clear_texture(SceneObject *obj) {
    if (obj->material.hasTexture) {
        UnloadTexture(obj->material.texture);
        obj->material.texture = Texture2D{0};
        obj->material.hasTexture = false;
    }
}
