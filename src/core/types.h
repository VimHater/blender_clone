#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <raylib.h>
#include <raymath.h>
#include <stdint.h>

// ---- Constants ----

#define MAX_OBJECTS      256
#define MAX_KEYFRAMES    512
#define MAX_NAME_LEN     64
#define MAX_CHILDREN     64
#define MAX_SELECTED     256

// ---- Enums ----

enum ObjectType {
    OBJ_NONE = 0,
    OBJ_CUBE,
    OBJ_SPHERE,
    OBJ_PLANE,
    OBJ_CYLINDER,
    OBJ_CONE,
    OBJ_TORUS,
    OBJ_KNOT,
    OBJ_CAPSULE,
    OBJ_POLY,
    OBJ_TEAPOT,
    OBJ_CAMERA,
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
    uint32_t id;            // unique identifier, never reused
    char name[MAX_NAME_LEN];
    ObjectType type;
    bool active;
    bool visible;
    int parentIndex;        // -1 = root

    ObjectTransform transform;
    ObjectMaterial material;

    // primitive params (shared across types where applicable)
    float cubeSize[3];      // w, h, d for cubes / plane width+length
    float sphereRadius;
    float cylinderRadiusTop;
    float cylinderRadiusBottom;
    float cylinderHeight;
    int sphereRings;
    int sphereSlices;

    // torus / knot
    float torusRadius;
    float torusSize;
    int torusRadSeg;
    int torusSides;

    // capsule
    float capsuleRadius;
    float capsuleHeight;

    // polygon
    int polySides;
    float polyRadius;

    // cone
    float coneRadius;
    float coneHeight;
    int coneSlices;

    // camera (OBJ_CAMERA)
    float camFov;
    float camNear;
    float camFar;
    bool camOrtho;

    // loaded model (OBJ_MODEL_FILE)
    Model model;
    bool modelLoaded;
    char modelPath[256];

    // animation keyframes
    Keyframe keyframes[MAX_KEYFRAMES];
    int keyframeCount;
};

SceneObject object_default(const char *name, ObjectType type);

#endif // CORE_TYPES_H
