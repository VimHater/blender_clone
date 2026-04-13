#ifndef CORE_RAYCAST_H
#define CORE_RAYCAST_H

#include "scene.h"

struct RayHitResult {
    bool hit;
    int objectIndex;
    Vector3 point;
    Vector3 normal;
    float distance;
};

RayHitResult raycast_scene(const Scene *s, Ray ray);
RayHitResult raycast_from_mouse(const Scene *s, Camera3D cam, Vector2 mousePos,
                                float vpX, float vpY, float vpW, float vpH,
                                int rtW, int rtH);

#endif // CORE_RAYCAST_H
