#include "raycast.h"
#include <algorithm>

static BoundingBox object_bounds(const SceneObject *obj) {
    Vector3 p = obj->transform.position;
    Vector3 s = obj->transform.scale;
    float hw, hh, hd, r;
    BoundingBox bb;

    switch (obj->type) {
        case OBJ_CUBE:
            hw = obj->cubeSize[0] * s.x * 0.5f;
            hh = obj->cubeSize[1] * s.y * 0.5f;
            hd = obj->cubeSize[2] * s.z * 0.5f;
            bb.min = Vector3{p.x - hw, p.y - hh, p.z - hd};
            bb.max = Vector3{p.x + hw, p.y + hh, p.z + hd};
            break;
        case OBJ_SPHERE:
            r = obj->sphereRadius * std::max({s.x, s.y, s.z});
            bb.min = Vector3{p.x - r, p.y - r, p.z - r};
            bb.max = Vector3{p.x + r, p.y + r, p.z + r};
            break;
        case OBJ_PLANE:
            hw = obj->cubeSize[0] * s.x * 0.5f;
            hd = obj->cubeSize[2] * s.z * 0.5f;
            bb.min = Vector3{p.x - hw, p.y - 0.01f, p.z - hd};
            bb.max = Vector3{p.x + hw, p.y + 0.01f, p.z + hd};
            break;
        case OBJ_CYLINDER:
            r = std::max(obj->cylinderRadiusTop, obj->cylinderRadiusBottom) * std::max(s.x, s.z);
            hh = obj->cylinderHeight * s.y * 0.5f;
            bb.min = Vector3{p.x - r, p.y - hh, p.z - r};
            bb.max = Vector3{p.x + r, p.y + hh, p.z + r};
            break;
        case OBJ_CONE:
            r = obj->coneRadius * std::max(s.x, s.z);
            hh = obj->coneHeight * s.y * 0.5f;
            bb.min = Vector3{p.x - r, p.y - hh, p.z - r};
            bb.max = Vector3{p.x + r, p.y + hh, p.z + r};
            break;
        case OBJ_TORUS:
        case OBJ_KNOT:
            r = (obj->torusSize + obj->torusRadius) * std::max({s.x, s.y, s.z});
            bb.min = Vector3{p.x - r, p.y - r, p.z - r};
            bb.max = Vector3{p.x + r, p.y + r, p.z + r};
            break;
        case OBJ_CAPSULE:
            r = obj->capsuleRadius * std::max(s.x, s.z);
            hh = (obj->capsuleHeight * 0.5f + obj->capsuleRadius) * s.y;
            bb.min = Vector3{p.x - r, p.y - hh, p.z - r};
            bb.max = Vector3{p.x + r, p.y + hh, p.z + r};
            break;
        case OBJ_POLY:
            r = obj->polyRadius * std::max(s.x, s.z);
            bb.min = Vector3{p.x - r, p.y - 0.01f, p.z - r};
            bb.max = Vector3{p.x + r, p.y + 0.01f, p.z + r};
            break;
        case OBJ_CAMERA:
            bb.min = Vector3{p.x - 0.3f, p.y - 0.3f, p.z - 0.8f};
            bb.max = Vector3{p.x + 0.3f, p.y + 0.3f, p.z + 0.3f};
            break;
        case OBJ_TEAPOT:
        case OBJ_MODEL_FILE:
            if (obj->modelLoaded) {
                BoundingBox mbb = GetModelBoundingBox(obj->model);
                // scale the model bbox
                bb.min = Vector3{p.x + mbb.min.x * s.x, p.y + mbb.min.y * s.y, p.z + mbb.min.z * s.z};
                bb.max = Vector3{p.x + mbb.max.x * s.x, p.y + mbb.max.y * s.y, p.z + mbb.max.z * s.z};
            } else {
                bb.min = Vector3{p.x - s.x, p.y - s.y, p.z - s.z};
                bb.max = Vector3{p.x + s.x, p.y + s.y, p.z + s.z};
            }
            break;
        default:
            bb.min = Vector3{p.x - s.x, p.y - s.y, p.z - s.z};
            bb.max = Vector3{p.x + s.x, p.y + s.y, p.z + s.z};
            break;
    }
    return bb;
}

RayHitResult raycast_scene(const Scene *s, Ray ray) {
    RayHitResult best;
    best.hit = false;
    best.distance = 1e30f;
    best.objectIndex = -1;

    for (int i = 0; i < s->objectCount; i++) {
        if (!s->objects[i].active || !s->objects[i].visible) continue;
        BoundingBox bb = object_bounds(&s->objects[i]);
        RayCollision col = GetRayCollisionBox(ray, bb);
        if (col.hit && col.distance < best.distance) {
            best.hit = true;
            best.objectIndex = i;
            best.point = col.point;
            best.normal = col.normal;
            best.distance = col.distance;
        }
    }
    return best;
}

RayHitResult raycast_from_mouse(const Scene *s, Camera3D cam, Vector2 mousePos,
                                float vpX, float vpY, float vpW, float vpH,
                                int rtW, int rtH) {
    // map screen mouse to viewport-local UV, then to render texture pixel coords
    float localX = (mousePos.x - vpX) / vpW;
    float localY = (mousePos.y - vpY) / vpH;
    Vector2 rtMouse = { localX * (float)rtW, localY * (float)rtH };
    Ray ray = GetScreenToWorldRayEx(rtMouse, cam, rtW, rtH);
    return raycast_scene(s, ray);
}
