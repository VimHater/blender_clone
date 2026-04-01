#include "raycast.h"
#include <algorithm>

static BoundingBox object_bounds(const SceneObject *obj) {
    Vector3 p = obj->transform.position;
    Vector3 s = obj->transform.scale;
    BoundingBox bb;

    switch (obj->type) {
        case OBJ_CUBE: {
            float hw = obj->cubeSize[0] * s.x * 0.5f;
            float hh = obj->cubeSize[1] * s.y * 0.5f;
            float hd = obj->cubeSize[2] * s.z * 0.5f;
            bb.min = Vector3{p.x - hw, p.y - hh, p.z - hd};
            bb.max = Vector3{p.x + hw, p.y + hh, p.z + hd};
            break;
        }
        case OBJ_SPHERE: {
            float r = obj->sphereRadius * std::max({s.x, s.y, s.z});
            bb.min = Vector3{p.x - r, p.y - r, p.z - r};
            bb.max = Vector3{p.x + r, p.y + r, p.z + r};
            break;
        }
        default: {
            float hw = s.x;
            float hh = s.y;
            float hd = s.z;
            bb.min = Vector3{p.x - hw, p.y - hh, p.z - hd};
            bb.max = Vector3{p.x + hw, p.y + hh, p.z + hd};
            break;
        }
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

RayHitResult raycast_from_mouse(const Scene *s, const EditorCamera *ec, Vector2 mousePos,
                                int vpX, int vpY, int vpW, int vpH) {
    Vector2 normMouse;
    normMouse.x = (mousePos.x - vpX) / (float)vpW * GetScreenWidth();
    normMouse.y = (mousePos.y - vpY) / (float)vpH * GetScreenHeight();
    Ray ray = GetScreenToWorldRay(normMouse, ec->cam);
    return raycast_scene(s, ray);
}
