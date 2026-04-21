#ifndef CORE_CAMERA_H
#define CORE_CAMERA_H

#include "types.h"

struct EditorCamera {
    Camera3D cam;
    Vector3 target;
    float distance;
    float yaw;              // radians
    float pitch;            // radians
    float zoomSpeed;
    float panSpeed;
    float orbitSpeed;
    float moveSpeed;        // WASD speed
    float fov;
    float nearPlane;
    float farPlane;
    bool ortho;
};

void editor_camera_init(EditorCamera *ec);
void editor_camera_update(EditorCamera *ec, bool inputAllowed,
                          float vpX = 0, float vpY = 0, float vpW = 0, float vpH = 0);
void editor_camera_sync(EditorCamera *ec);
void editor_camera_focus(EditorCamera *ec, Vector3 point, float dist);

#endif // CORE_CAMERA_H
