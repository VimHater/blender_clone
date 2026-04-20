#include "raycast.h"
#include "scene.h"

RayHitResult raycast_scene(const Scene *s, Ray ray) {
    RayHitResult best;
    best.hit = false;
    best.distance = 1e30f;
    best.objectIndex = -1;

    for (int i = 0; i < s->objectCount; i++) {
        if (!s->objects[i].active || !s->objects[i].visible) continue;
        BoundingBox bb = scene_get_bounds(s, i);
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
