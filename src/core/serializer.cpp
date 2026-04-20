#include "serializer.h"
#include "scene.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ---- Binary format ----

#define BINARY_MAGIC 0x4D454C44  // "DLEM"
#define BINARY_VERSION 1

struct BinaryHeader {
    uint32_t magic;
    uint32_t version;
    int objectCount;
    uint32_t nextId;
    int selectedCount;
};

// serializable subset of SceneObject (no GPU handles)
struct ObjectData {
    uint32_t id;
    char name[MAX_NAME_LEN];
    int type;
    bool active;
    bool visible;
    int parentIndex;

    // transform
    float posX, posY, posZ;
    float rotX, rotY, rotZ;
    float scaX, scaY, scaZ;

    // material (no texture handle)
    unsigned char colR, colG, colB, colA;
    bool hasTexture;
    char texturePath[256];
    float roughness;
    float metallic;

    // primitive params
    float cubeSize[3];
    float sphereRadius;
    float cylinderRadiusTop, cylinderRadiusBottom, cylinderHeight;
    int sphereRings, sphereSlices;
    float torusRadius, torusSize;
    int torusRadSeg, torusSides;
    float capsuleRadius, capsuleHeight;
    int polySides;
    float polyRadius;
    float coneRadius, coneHeight;
    int coneSlices;

    // camera
    float camFov, camNear, camFar;
    bool camOrtho;

    // shader
    int shaderType;

    // light
    int lightType;
    unsigned char lightR, lightG, lightB, lightA;
    float lightIntensity;

    // model path
    char modelPath[256];

    // script paths
    int scriptCount;
    char scriptPaths[MAX_SCRIPTS][256];

    // keyframes
    int keyframeCount;
};

struct KeyframeData {
    int frame;
    float posX, posY, posZ;
    float rotX, rotY, rotZ;
    float scaX, scaY, scaZ;
};

struct CameraData {
    float targetX, targetY, targetZ;
    float distance, yaw, pitch;
    float zoomSpeed, panSpeed, orbitSpeed, moveSpeed;
    float fov, nearPlane, farPlane;
    bool ortho;
};

struct TimelineData {
    int currentFrame, startFrame, endFrame;
    float fps;
};

struct UIData {
    float uiScale;
    bool showGrid;
    int gridSize;
    float gridSpacing;
    int transformMode;
    int drawMode;
    bool showTimeline, showConsole, showHierarchy, showProperties, showAddObject, showCamera;
};

static void obj_to_data(const SceneObject *obj, ObjectData *d) {
    memset(d, 0, sizeof(*d));
    d->id = obj->id;
    strncpy(d->name, obj->name, MAX_NAME_LEN);
    d->type = (int)obj->type;
    d->active = obj->active;
    d->visible = obj->visible;
    d->parentIndex = obj->parentIndex;

    d->posX = obj->transform.position.x; d->posY = obj->transform.position.y; d->posZ = obj->transform.position.z;
    d->rotX = obj->transform.rotation.x; d->rotY = obj->transform.rotation.y; d->rotZ = obj->transform.rotation.z;
    d->scaX = obj->transform.scale.x;    d->scaY = obj->transform.scale.y;    d->scaZ = obj->transform.scale.z;

    d->colR = obj->material.color.r; d->colG = obj->material.color.g;
    d->colB = obj->material.color.b; d->colA = obj->material.color.a;
    d->hasTexture = obj->material.hasTexture;
    strncpy(d->texturePath, obj->material.texturePath, 256);
    d->roughness = obj->material.roughness;
    d->metallic = obj->material.metallic;

    memcpy(d->cubeSize, obj->cubeSize, sizeof(d->cubeSize));
    d->sphereRadius = obj->sphereRadius;
    d->cylinderRadiusTop = obj->cylinderRadiusTop;
    d->cylinderRadiusBottom = obj->cylinderRadiusBottom;
    d->cylinderHeight = obj->cylinderHeight;
    d->sphereRings = obj->sphereRings;
    d->sphereSlices = obj->sphereSlices;
    d->torusRadius = obj->torusRadius; d->torusSize = obj->torusSize;
    d->torusRadSeg = obj->torusRadSeg; d->torusSides = obj->torusSides;
    d->capsuleRadius = obj->capsuleRadius; d->capsuleHeight = obj->capsuleHeight;
    d->polySides = obj->polySides; d->polyRadius = obj->polyRadius;
    d->coneRadius = obj->coneRadius; d->coneHeight = obj->coneHeight; d->coneSlices = obj->coneSlices;

    d->camFov = obj->camFov; d->camNear = obj->camNear; d->camFar = obj->camFar; d->camOrtho = obj->camOrtho;
    d->shaderType = (int)obj->shaderType;
    d->lightType = (int)obj->lightType;
    d->lightR = obj->lightColor.r; d->lightG = obj->lightColor.g;
    d->lightB = obj->lightColor.b; d->lightA = obj->lightColor.a;
    d->lightIntensity = obj->lightIntensity;
    strncpy(d->modelPath, obj->modelPath, 256);
    d->scriptCount = obj->scriptCount;
    for (int s = 0; s < obj->scriptCount && s < MAX_SCRIPTS; s++)
        strncpy(d->scriptPaths[s], obj->scriptPaths[s], 256);
    d->keyframeCount = obj->keyframeCount;
}

static void data_to_obj(const ObjectData *d, SceneObject *obj) {
    *obj = object_default(d->name, (ObjectType)d->type);
    obj->id = d->id;
    obj->active = d->active;
    obj->visible = d->visible;
    obj->parentIndex = d->parentIndex;

    obj->transform.position = {d->posX, d->posY, d->posZ};
    obj->transform.rotation = {d->rotX, d->rotY, d->rotZ};
    obj->transform.scale = {d->scaX, d->scaY, d->scaZ};

    obj->material.color = {d->colR, d->colG, d->colB, d->colA};
    obj->material.hasTexture = d->hasTexture;
    strncpy(obj->material.texturePath, d->texturePath, 256);
    obj->material.roughness = d->roughness;
    obj->material.metallic = d->metallic;

    memcpy(obj->cubeSize, d->cubeSize, sizeof(d->cubeSize));
    obj->sphereRadius = d->sphereRadius;
    obj->cylinderRadiusTop = d->cylinderRadiusTop;
    obj->cylinderRadiusBottom = d->cylinderRadiusBottom;
    obj->cylinderHeight = d->cylinderHeight;
    obj->sphereRings = d->sphereRings;
    obj->sphereSlices = d->sphereSlices;
    obj->torusRadius = d->torusRadius; obj->torusSize = d->torusSize;
    obj->torusRadSeg = d->torusRadSeg; obj->torusSides = d->torusSides;
    obj->capsuleRadius = d->capsuleRadius; obj->capsuleHeight = d->capsuleHeight;
    obj->polySides = d->polySides; obj->polyRadius = d->polyRadius;
    obj->coneRadius = d->coneRadius; obj->coneHeight = d->coneHeight; obj->coneSlices = d->coneSlices;

    obj->camFov = d->camFov; obj->camNear = d->camNear; obj->camFar = d->camFar; obj->camOrtho = d->camOrtho;
    obj->shaderType = (ShaderType)d->shaderType;
    obj->lightType = (LightType)d->lightType;
    obj->lightColor = {d->lightR, d->lightG, d->lightB, d->lightA};
    obj->lightIntensity = d->lightIntensity;
    strncpy(obj->modelPath, d->modelPath, 256);
    obj->scriptCount = d->scriptCount;
    for (int s = 0; s < d->scriptCount && s < MAX_SCRIPTS; s++)
        strncpy(obj->scriptPaths[s], d->scriptPaths[s], 256);
    obj->keyframeCount = d->keyframeCount;
}

// ---- Binary save/load ----

bool save_binary(const char *path, const EditorState *state) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    Scene *s = state->scene;
    BinaryHeader hdr = {};
    hdr.magic = BINARY_MAGIC;
    hdr.version = BINARY_VERSION;
    hdr.objectCount = s->objectCount;
    hdr.nextId = s->nextId;
    hdr.selectedCount = s->selectedCount;
    fwrite(&hdr, sizeof(hdr), 1, f);

    // selected IDs
    fwrite(s->selectedIds, sizeof(uint32_t), s->selectedCount, f);

    // objects
    for (int i = 0; i < s->objectCount; i++) {
        ObjectData d;
        obj_to_data(&s->objects[i], &d);
        fwrite(&d, sizeof(d), 1, f);

        // keyframes
        for (int k = 0; k < s->objects[i].keyframeCount; k++) {
            const Keyframe *kf = &s->objects[i].keyframes[k];
            KeyframeData kd;
            kd.frame = kf->frame;
            kd.posX = kf->transform.position.x; kd.posY = kf->transform.position.y; kd.posZ = kf->transform.position.z;
            kd.rotX = kf->transform.rotation.x; kd.rotY = kf->transform.rotation.y; kd.rotZ = kf->transform.rotation.z;
            kd.scaX = kf->transform.scale.x;    kd.scaY = kf->transform.scale.y;    kd.scaZ = kf->transform.scale.z;
            fwrite(&kd, sizeof(kd), 1, f);
        }
    }

    // camera
    CameraData cd = {};
    cd.targetX = state->camera->target.x; cd.targetY = state->camera->target.y; cd.targetZ = state->camera->target.z;
    cd.distance = state->camera->distance;
    cd.yaw = state->camera->yaw; cd.pitch = state->camera->pitch;
    cd.zoomSpeed = state->camera->zoomSpeed; cd.panSpeed = state->camera->panSpeed;
    cd.orbitSpeed = state->camera->orbitSpeed; cd.moveSpeed = state->camera->moveSpeed;
    cd.fov = state->camera->fov; cd.nearPlane = state->camera->nearPlane; cd.farPlane = state->camera->farPlane;
    cd.ortho = state->camera->ortho;
    fwrite(&cd, sizeof(cd), 1, f);

    // timeline
    TimelineData td = {};
    td.currentFrame = state->timeline->currentFrame;
    td.startFrame = state->timeline->startFrame;
    td.endFrame = state->timeline->endFrame;
    td.fps = state->timeline->fps;
    fwrite(&td, sizeof(td), 1, f);

    // ui prefs
    UIData ud = {};
    ud.uiScale = state->ui->uiScale;
    ud.showGrid = state->ui->showGrid;
    ud.gridSize = state->ui->gridSize;
    ud.gridSpacing = state->ui->gridSpacing;
    ud.transformMode = (int)state->ui->transformMode;
    ud.drawMode = (int)state->ui->drawMode;
    ud.showTimeline = state->ui->showTimeline;
    ud.showConsole = state->ui->showConsole;
    ud.showHierarchy = state->ui->showHierarchy;
    ud.showProperties = state->ui->showProperties;
    ud.showAddObject = state->ui->showAddObject;
    ud.showCamera = state->ui->showCamera;
    fwrite(&ud, sizeof(ud), 1, f);

    // script path (length-prefixed string)
    {
        uint16_t len = (uint16_t)strlen(state->scripting->console.scriptPath);
        fwrite(&len, sizeof(len), 1, f);
        if (len > 0) fwrite(state->scripting->console.scriptPath, 1, len, f);
    }

    fclose(f);
    return true;
}

bool load_binary(const char *path, EditorState *state) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    BinaryHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != BINARY_MAGIC || hdr.version != BINARY_VERSION) {
        fclose(f);
        return false;
    }

    Scene *s = state->scene;
    // clear existing objects
    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].modelLoaded) UnloadModel(s->objects[i].model);
        if (s->objects[i].material.hasTexture) UnloadTexture(s->objects[i].material.texture);
    }
    s->objectCount = hdr.objectCount;
    s->nextId = hdr.nextId;
    s->selectedCount = hdr.selectedCount;

    if (s->selectedCount > 0)
        fread(s->selectedIds, sizeof(uint32_t), s->selectedCount, f);

    for (int i = 0; i < s->objectCount; i++) {
        ObjectData d;
        fread(&d, sizeof(d), 1, f);
        data_to_obj(&d, &s->objects[i]);

        for (int k = 0; k < d.keyframeCount; k++) {
            KeyframeData kd;
            fread(&kd, sizeof(kd), 1, f);
            s->objects[i].keyframes[k].frame = kd.frame;
            s->objects[i].keyframes[k].transform.position = {kd.posX, kd.posY, kd.posZ};
            s->objects[i].keyframes[k].transform.rotation = {kd.rotX, kd.rotY, kd.rotZ};
            s->objects[i].keyframes[k].transform.scale = {kd.scaX, kd.scaY, kd.scaZ};
        }

        // reload texture if it had one
        if (s->objects[i].material.hasTexture && s->objects[i].material.texturePath[0]) {
            s->objects[i].material.hasTexture = false; // reset, let object_set_texture handle it
            object_set_texture(&s->objects[i], s->objects[i].material.texturePath);
        }

        // reload model if it had one
        if (s->objects[i].type == OBJ_MODEL_FILE && s->objects[i].modelPath[0]) {
            s->objects[i].model = LoadModel(s->objects[i].modelPath);
            s->objects[i].modelLoaded = true;
        }
    }

    // camera
    CameraData cd;
    fread(&cd, sizeof(cd), 1, f);
    state->camera->target = {cd.targetX, cd.targetY, cd.targetZ};
    state->camera->distance = cd.distance;
    state->camera->yaw = cd.yaw; state->camera->pitch = cd.pitch;
    state->camera->zoomSpeed = cd.zoomSpeed; state->camera->panSpeed = cd.panSpeed;
    state->camera->orbitSpeed = cd.orbitSpeed; state->camera->moveSpeed = cd.moveSpeed;
    state->camera->fov = cd.fov; state->camera->nearPlane = cd.nearPlane; state->camera->farPlane = cd.farPlane;
    state->camera->ortho = cd.ortho;
    editor_camera_sync(state->camera);

    // timeline
    TimelineData td;
    fread(&td, sizeof(td), 1, f);
    state->timeline->currentFrame = td.currentFrame;
    state->timeline->startFrame = td.startFrame;
    state->timeline->endFrame = td.endFrame;
    state->timeline->fps = td.fps;
    state->timeline->state = PLAYBACK_STOPPED;

    // ui prefs
    UIData ud;
    fread(&ud, sizeof(ud), 1, f);
    state->ui->uiScale = ud.uiScale;
    state->ui->showGrid = ud.showGrid;
    state->ui->gridSize = ud.gridSize;
    state->ui->gridSpacing = ud.gridSpacing;
    state->ui->transformMode = (TransformMode)ud.transformMode;
    state->ui->drawMode = (DrawMode)ud.drawMode;
    state->ui->showTimeline = ud.showTimeline;
    state->ui->showConsole = ud.showConsole;
    state->ui->showHierarchy = ud.showHierarchy;
    state->ui->showProperties = ud.showProperties;
    state->ui->showAddObject = ud.showAddObject;
    state->ui->showCamera = ud.showCamera;

    // script path
    {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) == 1 && len > 0 && len < sizeof(state->scripting->console.scriptPath)) {
            fread(state->scripting->console.scriptPath, 1, len, f);
            state->scripting->console.scriptPath[len] = '\0';
            script_load_file(state->scripting, state->scripting->console.scriptPath);
        }
    }

    fclose(f);
    return true;
}

// ---- Text format save/load ----

static void write_vec3(FILE *f, const char *key, Vector3 v) {
    fprintf(f, "%s = %g %g %g\n", key, v.x, v.y, v.z);
}

static void write_color(FILE *f, const char *key, Color c) {
    fprintf(f, "%s = %d %d %d %d\n", key, c.r, c.g, c.b, c.a);
}

static void write_float3(FILE *f, const char *key, const float *v) {
    fprintf(f, "%s = %g %g %g\n", key, v[0], v[1], v[2]);
}

bool save_text(const char *path, const EditorState *state) {
    FILE *f = fopen(path, "w");
    if (!f) return false;

    Scene *s = state->scene;

    fprintf(f, "[scene]\n");
    fprintf(f, "object_count = %d\n", s->objectCount);
    fprintf(f, "next_id = %u\n", s->nextId);
    fprintf(f, "selected_count = %d\n", s->selectedCount);
    for (int i = 0; i < s->selectedCount; i++)
        fprintf(f, "selected = %u\n", s->selectedIds[i]);
    fprintf(f, "\n");

    for (int i = 0; i < s->objectCount; i++) {
        const SceneObject *obj = &s->objects[i];
        fprintf(f, "[object.%d]\n", i);
        fprintf(f, "id = %u\n", obj->id);
        fprintf(f, "name = %s\n", obj->name);
        fprintf(f, "type = %d\n", (int)obj->type);
        fprintf(f, "active = %d\n", obj->active ? 1 : 0);
        fprintf(f, "visible = %d\n", obj->visible ? 1 : 0);
        fprintf(f, "parent = %d\n", obj->parentIndex);

        write_vec3(f, "pos", obj->transform.position);
        write_vec3(f, "rot", obj->transform.rotation);
        write_vec3(f, "scale", obj->transform.scale);

        write_color(f, "color", obj->material.color);
        fprintf(f, "has_texture = %d\n", obj->material.hasTexture ? 1 : 0);
        if (obj->material.hasTexture)
            fprintf(f, "texture_path = %s\n", obj->material.texturePath);
        fprintf(f, "roughness = %g\n", obj->material.roughness);
        fprintf(f, "metallic = %g\n", obj->material.metallic);

        write_float3(f, "cube_size", obj->cubeSize);
        fprintf(f, "sphere_radius = %g\n", obj->sphereRadius);
        fprintf(f, "cylinder_radius_top = %g\n", obj->cylinderRadiusTop);
        fprintf(f, "cylinder_radius_bottom = %g\n", obj->cylinderRadiusBottom);
        fprintf(f, "cylinder_height = %g\n", obj->cylinderHeight);
        fprintf(f, "sphere_rings = %d\n", obj->sphereRings);
        fprintf(f, "sphere_slices = %d\n", obj->sphereSlices);
        fprintf(f, "torus_radius = %g\n", obj->torusRadius);
        fprintf(f, "torus_size = %g\n", obj->torusSize);
        fprintf(f, "torus_rad_seg = %d\n", obj->torusRadSeg);
        fprintf(f, "torus_sides = %d\n", obj->torusSides);
        fprintf(f, "capsule_radius = %g\n", obj->capsuleRadius);
        fprintf(f, "capsule_height = %g\n", obj->capsuleHeight);
        fprintf(f, "poly_sides = %d\n", obj->polySides);
        fprintf(f, "poly_radius = %g\n", obj->polyRadius);
        fprintf(f, "cone_radius = %g\n", obj->coneRadius);
        fprintf(f, "cone_height = %g\n", obj->coneHeight);
        fprintf(f, "cone_slices = %d\n", obj->coneSlices);

        fprintf(f, "cam_fov = %g\n", obj->camFov);
        fprintf(f, "cam_near = %g\n", obj->camNear);
        fprintf(f, "cam_far = %g\n", obj->camFar);
        fprintf(f, "cam_ortho = %d\n", obj->camOrtho ? 1 : 0);

        fprintf(f, "shader = %d\n", (int)obj->shaderType);
        fprintf(f, "light_type = %d\n", (int)obj->lightType);
        write_color(f, "light_color", obj->lightColor);
        fprintf(f, "light_intensity = %g\n", obj->lightIntensity);

        if (obj->type == OBJ_MODEL_FILE)
            fprintf(f, "model_path = %s\n", obj->modelPath);

        fprintf(f, "script_count = %d\n", obj->scriptCount);
        for (int s = 0; s < obj->scriptCount; s++) {
            fprintf(f, "script_path = %s\n", obj->scriptPaths[s]);
        }

        // physics
        if (obj->usePhysics) {
            fprintf(f, "use_physics = 1\n");
            fprintf(f, "is_static = %d\n", obj->isStatic ? 1 : 0);
            fprintf(f, "mass = %g\n", obj->mass);
            fprintf(f, "velocity = %g %g %g\n", obj->velocity.x, obj->velocity.y, obj->velocity.z);
            fprintf(f, "use_gravity = %d\n", obj->useGravity ? 1 : 0);
            fprintf(f, "restitution = %g\n", obj->restitution);
            fprintf(f, "phys_friction = %g\n", obj->friction);
        }

        fprintf(f, "keyframe_count = %d\n", obj->keyframeCount);
        for (int k = 0; k < obj->keyframeCount; k++) {
            const Keyframe *kf = &obj->keyframes[k];
            fprintf(f, "keyframe = %d %g %g %g %g %g %g %g %g %g\n",
                kf->frame,
                kf->transform.position.x, kf->transform.position.y, kf->transform.position.z,
                kf->transform.rotation.x, kf->transform.rotation.y, kf->transform.rotation.z,
                kf->transform.scale.x, kf->transform.scale.y, kf->transform.scale.z);
        }
        fprintf(f, "\n");
    }

    // camera
    fprintf(f, "[camera]\n");
    write_vec3(f, "target", state->camera->target);
    fprintf(f, "distance = %g\n", state->camera->distance);
    fprintf(f, "yaw = %g\n", state->camera->yaw);
    fprintf(f, "pitch = %g\n", state->camera->pitch);
    fprintf(f, "zoom_speed = %g\n", state->camera->zoomSpeed);
    fprintf(f, "pan_speed = %g\n", state->camera->panSpeed);
    fprintf(f, "orbit_speed = %g\n", state->camera->orbitSpeed);
    fprintf(f, "move_speed = %g\n", state->camera->moveSpeed);
    fprintf(f, "fov = %g\n", state->camera->fov);
    fprintf(f, "near = %g\n", state->camera->nearPlane);
    fprintf(f, "far = %g\n", state->camera->farPlane);
    fprintf(f, "ortho = %d\n", state->camera->ortho ? 1 : 0);
    fprintf(f, "\n");

    // timeline
    fprintf(f, "[timeline]\n");
    fprintf(f, "current_frame = %d\n", state->timeline->currentFrame);
    fprintf(f, "start_frame = %d\n", state->timeline->startFrame);
    fprintf(f, "end_frame = %d\n", state->timeline->endFrame);
    fprintf(f, "fps = %g\n", state->timeline->fps);
    fprintf(f, "\n");

    // scripting
    fprintf(f, "[scripting]\n");
    fprintf(f, "script_path = %s\n", state->scripting->console.scriptPath);
    fprintf(f, "\n");

    // ui
    fprintf(f, "[ui]\n");
    fprintf(f, "ui_scale = %g\n", state->ui->uiScale);
    fprintf(f, "show_grid = %d\n", state->ui->showGrid ? 1 : 0);
    fprintf(f, "grid_size = %d\n", state->ui->gridSize);
    fprintf(f, "grid_spacing = %g\n", state->ui->gridSpacing);
    fprintf(f, "transform_mode = %d\n", (int)state->ui->transformMode);
    fprintf(f, "draw_mode = %d\n", (int)state->ui->drawMode);
    fprintf(f, "show_timeline = %d\n", state->ui->showTimeline ? 1 : 0);
    fprintf(f, "show_hierarchy = %d\n", state->ui->showHierarchy ? 1 : 0);
    fprintf(f, "show_properties = %d\n", state->ui->showProperties ? 1 : 0);
    fprintf(f, "show_add_object = %d\n", state->ui->showAddObject ? 1 : 0);
    fprintf(f, "show_camera = %d\n", state->ui->showCamera ? 1 : 0);
    fprintf(f, "show_console = %d\n", state->ui->showConsole ? 1 : 0);
    fprintf(f, "\n");

    fclose(f);
    return true;
}

// ---- Text format loader ----

static bool parse_vec3(const char *val, Vector3 *v) {
    return sscanf(val, "%f %f %f", &v->x, &v->y, &v->z) == 3;
}

static bool parse_color(const char *val, Color *c) {
    int r, g, b, a;
    if (sscanf(val, "%d %d %d %d", &r, &g, &b, &a) == 4) {
        *c = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
        return true;
    }
    return false;
}

static bool parse_float3(const char *val, float *out) {
    return sscanf(val, "%f %f %f", &out[0], &out[1], &out[2]) == 3;
}

bool load_text(const char *path, EditorState *state) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    Scene *s = state->scene;
    // clear existing
    for (int i = 0; i < s->objectCount; i++) {
        if (s->objects[i].modelLoaded) UnloadModel(s->objects[i].model);
        if (s->objects[i].material.hasTexture) UnloadTexture(s->objects[i].material.texture);
    }
    scene_init(s);

    char line[1024];
    char section[64] = {};
    int objIdx = -1;
    int selIdx = 0;
    int kfIdx = 0;

    while (fgets(line, sizeof(line), f)) {
        // strip newline
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len == 0) continue;

        // section header
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = 0;
                strncpy(section, line + 1, sizeof(section) - 1);

                // check for object.N
                if (strncmp(section, "object.", 7) == 0) {
                    objIdx = atoi(section + 7);
                    kfIdx = 0;
                    if (objIdx >= 0 && objIdx < MAX_OBJECTS) {
                        s->objects[objIdx] = object_default("", OBJ_NONE);
                    }
                }
            }
            continue;
        }

        // key = value
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line;
        char *val = eq + 1;
        // trim
        while (*key == ' ') key++;
        int kl = (int)strlen(key);
        while (kl > 0 && key[kl-1] == ' ') key[--kl] = 0;
        while (*val == ' ') val++;

        if (strcmp(section, "scene") == 0) {
            if (strcmp(key, "object_count") == 0) s->objectCount = atoi(val);
            else if (strcmp(key, "next_id") == 0) s->nextId = (uint32_t)strtoul(val, NULL, 10);
            else if (strcmp(key, "selected_count") == 0) { s->selectedCount = atoi(val); selIdx = 0; }
            else if (strcmp(key, "selected") == 0 && selIdx < MAX_SELECTED) {
                s->selectedIds[selIdx++] = (uint32_t)strtoul(val, NULL, 10);
            }
        }
        else if (strncmp(section, "object.", 7) == 0 && objIdx >= 0 && objIdx < MAX_OBJECTS) {
            SceneObject *obj = &s->objects[objIdx];
            if (strcmp(key, "id") == 0) obj->id = (uint32_t)strtoul(val, NULL, 10);
            else if (strcmp(key, "name") == 0) strncpy(obj->name, val, MAX_NAME_LEN);
            else if (strcmp(key, "type") == 0) obj->type = (ObjectType)atoi(val);
            else if (strcmp(key, "active") == 0) obj->active = atoi(val) != 0;
            else if (strcmp(key, "visible") == 0) obj->visible = atoi(val) != 0;
            else if (strcmp(key, "parent") == 0) obj->parentIndex = atoi(val);
            else if (strcmp(key, "pos") == 0) parse_vec3(val, &obj->transform.position);
            else if (strcmp(key, "rot") == 0) parse_vec3(val, &obj->transform.rotation);
            else if (strcmp(key, "scale") == 0) parse_vec3(val, &obj->transform.scale);
            else if (strcmp(key, "color") == 0) parse_color(val, &obj->material.color);
            else if (strcmp(key, "has_texture") == 0) obj->material.hasTexture = atoi(val) != 0;
            else if (strcmp(key, "texture_path") == 0) strncpy(obj->material.texturePath, val, 256);
            else if (strcmp(key, "roughness") == 0) obj->material.roughness = (float)atof(val);
            else if (strcmp(key, "metallic") == 0) obj->material.metallic = (float)atof(val);
            else if (strcmp(key, "cube_size") == 0) parse_float3(val, obj->cubeSize);
            else if (strcmp(key, "sphere_radius") == 0) obj->sphereRadius = (float)atof(val);
            else if (strcmp(key, "cylinder_radius_top") == 0) obj->cylinderRadiusTop = (float)atof(val);
            else if (strcmp(key, "cylinder_radius_bottom") == 0) obj->cylinderRadiusBottom = (float)atof(val);
            else if (strcmp(key, "cylinder_height") == 0) obj->cylinderHeight = (float)atof(val);
            else if (strcmp(key, "sphere_rings") == 0) obj->sphereRings = atoi(val);
            else if (strcmp(key, "sphere_slices") == 0) obj->sphereSlices = atoi(val);
            else if (strcmp(key, "torus_radius") == 0) obj->torusRadius = (float)atof(val);
            else if (strcmp(key, "torus_size") == 0) obj->torusSize = (float)atof(val);
            else if (strcmp(key, "torus_rad_seg") == 0) obj->torusRadSeg = atoi(val);
            else if (strcmp(key, "torus_sides") == 0) obj->torusSides = atoi(val);
            else if (strcmp(key, "capsule_radius") == 0) obj->capsuleRadius = (float)atof(val);
            else if (strcmp(key, "capsule_height") == 0) obj->capsuleHeight = (float)atof(val);
            else if (strcmp(key, "poly_sides") == 0) obj->polySides = atoi(val);
            else if (strcmp(key, "poly_radius") == 0) obj->polyRadius = (float)atof(val);
            else if (strcmp(key, "cone_radius") == 0) obj->coneRadius = (float)atof(val);
            else if (strcmp(key, "cone_height") == 0) obj->coneHeight = (float)atof(val);
            else if (strcmp(key, "cone_slices") == 0) obj->coneSlices = atoi(val);
            else if (strcmp(key, "cam_fov") == 0) obj->camFov = (float)atof(val);
            else if (strcmp(key, "cam_near") == 0) obj->camNear = (float)atof(val);
            else if (strcmp(key, "cam_far") == 0) obj->camFar = (float)atof(val);
            else if (strcmp(key, "cam_ortho") == 0) obj->camOrtho = atoi(val) != 0;
            else if (strcmp(key, "shader") == 0) obj->shaderType = (ShaderType)atoi(val);
            else if (strcmp(key, "light_type") == 0) obj->lightType = (LightType)atoi(val);
            else if (strcmp(key, "light_color") == 0) parse_color(val, &obj->lightColor);
            else if (strcmp(key, "light_intensity") == 0) obj->lightIntensity = (float)atof(val);
            else if (strcmp(key, "model_path") == 0) strncpy(obj->modelPath, val, 256);
            else if (strcmp(key, "script_count") == 0) obj->scriptCount = atoi(val);
            else if (strcmp(key, "script_path") == 0 && obj->scriptCount < MAX_SCRIPTS) {
                // append to next available slot
                int si = 0;
                for (si = 0; si < MAX_SCRIPTS; si++) { if (obj->scriptPaths[si][0] == '\0') break; }
                if (si < MAX_SCRIPTS) strncpy(obj->scriptPaths[si], val, 256);
            }
            else if (strcmp(key, "use_physics") == 0) obj->usePhysics = atoi(val) != 0;
            else if (strcmp(key, "is_static") == 0) obj->isStatic = atoi(val) != 0;
            else if (strcmp(key, "mass") == 0) obj->mass = (float)atof(val);
            else if (strcmp(key, "velocity") == 0) parse_vec3(val, &obj->velocity);
            else if (strcmp(key, "use_gravity") == 0) obj->useGravity = atoi(val) != 0;
            else if (strcmp(key, "restitution") == 0) obj->restitution = (float)atof(val);
            else if (strcmp(key, "phys_friction") == 0) obj->friction = (float)atof(val);
            else if (strcmp(key, "keyframe_count") == 0) { obj->keyframeCount = atoi(val); kfIdx = 0; }
            else if (strcmp(key, "keyframe") == 0 && kfIdx < MAX_KEYFRAMES) {
                Keyframe *kf = &obj->keyframes[kfIdx++];
                sscanf(val, "%d %f %f %f %f %f %f %f %f %f",
                    &kf->frame,
                    &kf->transform.position.x, &kf->transform.position.y, &kf->transform.position.z,
                    &kf->transform.rotation.x, &kf->transform.rotation.y, &kf->transform.rotation.z,
                    &kf->transform.scale.x, &kf->transform.scale.y, &kf->transform.scale.z);
            }
        }
        else if (strcmp(section, "camera") == 0) {
            if (strcmp(key, "target") == 0) parse_vec3(val, &state->camera->target);
            else if (strcmp(key, "distance") == 0) state->camera->distance = (float)atof(val);
            else if (strcmp(key, "yaw") == 0) state->camera->yaw = (float)atof(val);
            else if (strcmp(key, "pitch") == 0) state->camera->pitch = (float)atof(val);
            else if (strcmp(key, "zoom_speed") == 0) state->camera->zoomSpeed = (float)atof(val);
            else if (strcmp(key, "pan_speed") == 0) state->camera->panSpeed = (float)atof(val);
            else if (strcmp(key, "orbit_speed") == 0) state->camera->orbitSpeed = (float)atof(val);
            else if (strcmp(key, "move_speed") == 0) state->camera->moveSpeed = (float)atof(val);
            else if (strcmp(key, "fov") == 0) state->camera->fov = (float)atof(val);
            else if (strcmp(key, "near") == 0) state->camera->nearPlane = (float)atof(val);
            else if (strcmp(key, "far") == 0) state->camera->farPlane = (float)atof(val);
            else if (strcmp(key, "ortho") == 0) state->camera->ortho = atoi(val) != 0;
        }
        else if (strcmp(section, "timeline") == 0) {
            if (strcmp(key, "current_frame") == 0) state->timeline->currentFrame = atoi(val);
            else if (strcmp(key, "start_frame") == 0) state->timeline->startFrame = atoi(val);
            else if (strcmp(key, "end_frame") == 0) state->timeline->endFrame = atoi(val);
            else if (strcmp(key, "fps") == 0) state->timeline->fps = (float)atof(val);
        }
        else if (strcmp(section, "scripting") == 0) {
            if (strcmp(key, "script_path") == 0) {
                snprintf(state->scripting->console.scriptPath, sizeof(state->scripting->console.scriptPath), "%s", val);
                if (val[0]) {
                    script_load_file(state->scripting, val);
                }
            }
        }
        else if (strcmp(section, "ui") == 0) {
            if (strcmp(key, "ui_scale") == 0) state->ui->uiScale = (float)atof(val);
            else if (strcmp(key, "show_grid") == 0) state->ui->showGrid = atoi(val) != 0;
            else if (strcmp(key, "grid_size") == 0) state->ui->gridSize = atoi(val);
            else if (strcmp(key, "grid_spacing") == 0) state->ui->gridSpacing = (float)atof(val);
            else if (strcmp(key, "transform_mode") == 0) state->ui->transformMode = (TransformMode)atoi(val);
            else if (strcmp(key, "draw_mode") == 0) state->ui->drawMode = (DrawMode)atoi(val);
            else if (strcmp(key, "show_timeline") == 0) state->ui->showTimeline = atoi(val) != 0;
            else if (strcmp(key, "show_hierarchy") == 0) state->ui->showHierarchy = atoi(val) != 0;
            else if (strcmp(key, "show_properties") == 0) state->ui->showProperties = atoi(val) != 0;
            else if (strcmp(key, "show_add_object") == 0) state->ui->showAddObject = atoi(val) != 0;
            else if (strcmp(key, "show_camera") == 0) state->ui->showCamera = atoi(val) != 0;
            else if (strcmp(key, "show_console") == 0) state->ui->showConsole = atoi(val) != 0;
        }
    }

    fclose(f);

    // post-load: reload textures and models
    for (int i = 0; i < s->objectCount; i++) {
        SceneObject *obj = &s->objects[i];
        if (obj->material.hasTexture && obj->material.texturePath[0]) {
            obj->material.hasTexture = false;
            object_set_texture(obj, obj->material.texturePath);
        }
        if (obj->type == OBJ_MODEL_FILE && obj->modelPath[0]) {
            obj->model = LoadModel(obj->modelPath);
            obj->modelLoaded = true;
        }
    }

    state->timeline->state = PLAYBACK_STOPPED;
    editor_camera_sync(state->camera);

    return true;
}

