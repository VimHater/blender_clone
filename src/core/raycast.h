#ifndef CORE_RAYCAST_H
#define CORE_RAYCAST_H

#include "scene.h"
#include "camera.h"

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

#endif // CORE_RAYCAST_H
