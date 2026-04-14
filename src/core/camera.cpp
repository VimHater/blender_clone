#include "camera.h"
#include <cmath>
#include "raylib.h"

void editor_camera_init(EditorCamera *ec) {
    ec->target = Vector3{0, 0, 0};
    ec->distance = 15.0f;
    ec->yaw = 0.8f;
    ec->pitch = 0.6f;
    ec->zoomSpeed = 1.5f;
    ec->panSpeed = 0.01f;
    ec->orbitSpeed = 0.003f;
    ec->moveSpeed = 10.0f;
    ec->fov = 45.0f;
    ec->nearPlane = 0.1f;
    ec->farPlane = 1000.0f;
    ec->ortho = false;
    editor_camera_sync(ec);
}

void editor_camera_sync(EditorCamera *ec) {
    float cosP = cosf(ec->pitch);
    ec->cam.position = Vector3{
        ec->target.x + ec->distance * cosP * sinf(ec->yaw),
        ec->target.y + ec->distance * sinf(ec->pitch),
        ec->target.z + ec->distance * cosP * cosf(ec->yaw)
    };
    ec->cam.target = ec->target;
    ec->cam.up = Vector3{0, 1, 0};
    ec->cam.fovy = ec->fov;
    ec->cam.projection = ec->ortho ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
    // raylib doesn't expose near/far on Camera3D directly; we set them via rlgl in BeginMode3D
}

void editor_camera_update(EditorCamera *ec, bool inputAllowed) {
    if (!inputAllowed) return;

    // orbit/pan: middle mouse or hold G + mouse move
    bool orbiting = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsKeyDown(KEY_G);
    if (orbiting) {
        Vector2 delta = GetMouseDelta();
        bool panModifier = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (panModifier) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(ec->cam.target, ec->cam.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, ec->cam.up));
            Vector3 up = Vector3CrossProduct(right, forward);
            ec->target = Vector3Add(ec->target, Vector3Scale(right, -delta.x * ec->panSpeed * ec->distance * 0.1f));
            ec->target = Vector3Add(ec->target, Vector3Scale(up, delta.y * ec->panSpeed * ec->distance * 0.1f));
        } else {
            ec->yaw   += delta.x * ec->orbitSpeed;
            ec->pitch += delta.y * ec->orbitSpeed;
            if (ec->pitch > 1.5f) ec->pitch = 1.5f;
            if (ec->pitch < -1.5f) ec->pitch = -1.5f;
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        ec->distance -= wheel * ec->zoomSpeed;
        if (ec->distance < 1.0f) ec->distance = 1.0f;
        if (ec->distance > 200.0f) ec->distance = 200.0f;
    }

    // WASD movement (moves the target point)
    float dt = GetFrameTime();
    float speed = ec->moveSpeed * dt;
    Vector3 forward = Vector3Normalize(Vector3Subtract(ec->cam.target, ec->cam.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, ec->cam.up));
    // project forward onto XZ plane for horizontal movement
    Vector3 forwardXZ = Vector3Normalize(Vector3{forward.x, 0, forward.z});
    Vector3 rightXZ = Vector3Normalize(Vector3{right.x, 0, right.z});

    if (IsKeyDown(KEY_W)) ec->target = Vector3Add(ec->target, Vector3Scale(forwardXZ, speed));
    if (IsKeyDown(KEY_S)) ec->target = Vector3Add(ec->target, Vector3Scale(forwardXZ, -speed));
    if (IsKeyDown(KEY_D)) ec->target = Vector3Add(ec->target, Vector3Scale(rightXZ, speed));
    if (IsKeyDown(KEY_A)) ec->target = Vector3Add(ec->target, Vector3Scale(rightXZ, -speed));
    if (IsKeyDown(KEY_LEFT_SHIFT)) ec->target.y -= speed;
    if (IsKeyDown(KEY_SPACE)) ec->target.y += speed;

    editor_camera_sync(ec);
}

void editor_camera_focus(EditorCamera *ec, Vector3 point, float dist) {
    ec->target = point;
    ec->distance = dist;
    editor_camera_sync(ec);
}
