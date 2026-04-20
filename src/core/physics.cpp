#include "physics.h"
#include <cmath>

#define GRAVITY 9.81f

// ---- Collision shapes ----

struct ColliderAABB {
    Vector3 min;
    Vector3 max;
};

static ColliderAABB get_aabb(const SceneObject *obj) {
    Vector3 p = obj->transform.position;
    Vector3 s = obj->transform.scale;
    ColliderAABB box;

    switch (obj->type) {
    case OBJ_CUBE:
        box.min = { p.x - obj->cubeSize[0]*s.x*0.5f, p.y - obj->cubeSize[1]*s.y*0.5f, p.z - obj->cubeSize[2]*s.z*0.5f };
        box.max = { p.x + obj->cubeSize[0]*s.x*0.5f, p.y + obj->cubeSize[1]*s.y*0.5f, p.z + obj->cubeSize[2]*s.z*0.5f };
        break;
    case OBJ_SPHERE:
    case OBJ_TORUS:
    case OBJ_KNOT:
    case OBJ_TEAPOT: {
        float r = obj->sphereRadius * fmaxf(s.x, fmaxf(s.y, s.z));
        box.min = { p.x - r, p.y - r, p.z - r };
        box.max = { p.x + r, p.y + r, p.z + r };
        break;
    }
    case OBJ_CYLINDER: {
        float r = fmaxf(obj->cylinderRadiusTop, obj->cylinderRadiusBottom) * fmaxf(s.x, s.z);
        float h = obj->cylinderHeight * s.y * 0.5f;
        box.min = { p.x - r, p.y - h, p.z - r };
        box.max = { p.x + r, p.y + h, p.z + r };
        break;
    }
    case OBJ_CONE: {
        float r = obj->coneRadius * fmaxf(s.x, s.z);
        float h = obj->coneHeight * s.y * 0.5f;
        box.min = { p.x - r, p.y - h, p.z - r };
        box.max = { p.x + r, p.y + h, p.z + r };
        break;
    }
    case OBJ_CAPSULE: {
        float r = obj->capsuleRadius * fmaxf(s.x, s.z);
        float h = obj->capsuleHeight * s.y * 0.5f;
        box.min = { p.x - r, p.y - h, p.z - r };
        box.max = { p.x + r, p.y + h, p.z + r };
        break;
    }
    case OBJ_PLANE: {
        float hw = obj->cubeSize[0] * s.x * 0.5f;
        float hd = obj->cubeSize[1] * s.z * 0.5f;
        box.min = { p.x - hw, p.y - 0.01f, p.z - hd };
        box.max = { p.x + hw, p.y + 0.01f, p.z + hd };
        break;
    }
    default: {
        // fallback: unit box scaled
        box.min = { p.x - s.x, p.y - s.y, p.z - s.z };
        box.max = { p.x + s.x, p.y + s.y, p.z + s.z };
        break;
    }
    }
    return box;
}

static bool aabb_overlap(const ColliderAABB &a, const ColliderAABB &b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

// returns overlap depth and push normal (from a to b)
static bool aabb_resolve(const ColliderAABB &a, const ColliderAABB &b, Vector3 *normal, float *depth) {
    float dx1 = b.max.x - a.min.x;
    float dx2 = a.max.x - b.min.x;
    float dy1 = b.max.y - a.min.y;
    float dy2 = a.max.y - b.min.y;
    float dz1 = b.max.z - a.min.z;
    float dz2 = a.max.z - b.min.z;

    float overlapX = fminf(dx1, dx2);
    float overlapY = fminf(dy1, dy2);
    float overlapZ = fminf(dz1, dz2);

    if (overlapX <= 0 || overlapY <= 0 || overlapZ <= 0) return false;

    if (overlapX < overlapY && overlapX < overlapZ) {
        *depth = overlapX;
        *normal = { (dx1 < dx2) ? -1.0f : 1.0f, 0, 0 };
    } else if (overlapY < overlapZ) {
        *depth = overlapY;
        *normal = { 0, (dy1 < dy2) ? -1.0f : 1.0f, 0 };
    } else {
        *depth = overlapZ;
        *normal = { 0, 0, (dz1 < dz2) ? -1.0f : 1.0f };
    }
    return true;
}

void physics_step(Scene *scene, float dt) {
    if (dt <= 0) return;

    // 1. apply gravity and integrate velocity
    for (int i = 0; i < scene->objectCount; i++) {
        SceneObject *obj = &scene->objects[i];
        if (!obj->active || !obj->usePhysics || obj->isStatic) continue;

        if (obj->useGravity) {
            obj->velocity.y -= GRAVITY * dt;
        }

        obj->transform.position.x += obj->velocity.x * dt;
        obj->transform.position.y += obj->velocity.y * dt;
        obj->transform.position.z += obj->velocity.z * dt;
    }

    // 2. collision detection and response
    for (int i = 0; i < scene->objectCount; i++) {
        SceneObject *a = &scene->objects[i];
        if (!a->active || !a->usePhysics) continue;

        for (int j = i + 1; j < scene->objectCount; j++) {
            SceneObject *b = &scene->objects[j];
            if (!b->active || !b->usePhysics) continue;
            if (a->isStatic && b->isStatic) continue;

            ColliderAABB boxA = get_aabb(a);
            ColliderAABB boxB = get_aabb(b);

            if (!aabb_overlap(boxA, boxB)) continue;

            Vector3 normal;
            float depth;
            if (!aabb_resolve(boxA, boxB, &normal, &depth)) continue;

            // separate objects
            if (a->isStatic) {
                // only push b
                b->transform.position.x += normal.x * depth;
                b->transform.position.y += normal.y * depth;
                b->transform.position.z += normal.z * depth;
            } else if (b->isStatic) {
                // only push a (reverse normal)
                a->transform.position.x -= normal.x * depth;
                a->transform.position.y -= normal.y * depth;
                a->transform.position.z -= normal.z * depth;
            } else {
                // push both equally
                float half = depth * 0.5f;
                a->transform.position.x -= normal.x * half;
                a->transform.position.y -= normal.y * half;
                a->transform.position.z -= normal.z * half;
                b->transform.position.x += normal.x * half;
                b->transform.position.y += normal.y * half;
                b->transform.position.z += normal.z * half;
            }

            // velocity response along collision normal
            float restitution = fminf(a->restitution, b->restitution);
            float friction = (a->friction + b->friction) * 0.5f;

            // relative velocity along normal (b relative to a)
            float vRelN = (b->velocity.x - a->velocity.x) * normal.x +
                          (b->velocity.y - a->velocity.y) * normal.y +
                          (b->velocity.z - a->velocity.z) * normal.z;

            if (vRelN >= 0) continue; // separating

            float invMassA = a->isStatic ? 0.0f : 1.0f / a->mass;
            float invMassB = b->isStatic ? 0.0f : 1.0f / b->mass;
            float invMassSum = invMassA + invMassB;
            if (invMassSum == 0) continue;

            float impulse = -(1.0f + restitution) * vRelN / invMassSum;

            if (!a->isStatic) {
                a->velocity.x -= impulse * invMassA * normal.x;
                a->velocity.y -= impulse * invMassA * normal.y;
                a->velocity.z -= impulse * invMassA * normal.z;
            }
            if (!b->isStatic) {
                b->velocity.x += impulse * invMassB * normal.x;
                b->velocity.y += impulse * invMassB * normal.y;
                b->velocity.z += impulse * invMassB * normal.z;
            }

            // tangential friction
            Vector3 vRel = { b->velocity.x - a->velocity.x,
                             b->velocity.y - a->velocity.y,
                             b->velocity.z - a->velocity.z };
            float vnDot = vRel.x * normal.x + vRel.y * normal.y + vRel.z * normal.z;
            Vector3 tangent = { vRel.x - vnDot * normal.x,
                                vRel.y - vnDot * normal.y,
                                vRel.z - vnDot * normal.z };
            float tLen = sqrtf(tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z);
            if (tLen > 0.001f) {
                tangent.x /= tLen;
                tangent.y /= tLen;
                tangent.z /= tLen;
                float frictionImpulse = tLen * friction / invMassSum;
                if (frictionImpulse > fabsf(impulse) * friction)
                    frictionImpulse = fabsf(impulse) * friction;
                if (!a->isStatic) {
                    a->velocity.x -= frictionImpulse * invMassA * tangent.x;
                    a->velocity.y -= frictionImpulse * invMassA * tangent.y;
                    a->velocity.z -= frictionImpulse * invMassA * tangent.z;
                }
                if (!b->isStatic) {
                    b->velocity.x += frictionImpulse * invMassB * tangent.x;
                    b->velocity.y += frictionImpulse * invMassB * tangent.y;
                    b->velocity.z += frictionImpulse * invMassB * tangent.z;
                }
            }
        }
    }
}
