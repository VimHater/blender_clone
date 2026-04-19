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

    bool altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

    // hide/show cursor on alt press/release
    if (altHeld && !IsCursorHidden()) {
        DisableCursor();
    } else if (!altHeld && IsCursorHidden()) {
        EnableCursor();
    }

    // Alt held: FPS-style mouse look
    if (altHeld) {
        Vector2 delta = GetMouseDelta();
        ec->yaw   += delta.x * ec->orbitSpeed;
        ec->pitch += delta.y * ec->orbitSpeed;
        if (ec->pitch > 1.5f) ec->pitch = 1.5f;
        if (ec->pitch < -1.5f) ec->pitch = -1.5f;
    }

    // orbit/pan: middle mouse
    bool orbiting = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    if (orbiting && !altHeld) {
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

    // WASD movement (always active)
    float dt = GetFrameTime();
    float speed = ec->moveSpeed * dt;
    Vector3 forward = Vector3Normalize(Vector3Subtract(ec->cam.target, ec->cam.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, ec->cam.up));
    Vector3 forwardXZ = Vector3Normalize(Vector3{forward.x, 0, forward.z});
    Vector3 rightXZ = Vector3Normalize(Vector3{right.x, 0, right.z});

    if (IsKeyDown(KEY_W)) ec->target = Vector3Add(ec->target, Vector3Scale(forwardXZ, speed));
    if (IsKeyDown(KEY_S)) ec->target = Vector3Add(ec->target, Vector3Scale(forwardXZ, -speed));
    if (IsKeyDown(KEY_D)) ec->target = Vector3Add(ec->target, Vector3Scale(rightXZ, speed));
    if (IsKeyDown(KEY_A)) ec->target = Vector3Add(ec->target, Vector3Scale(rightXZ, -speed));
    if (IsKeyDown(KEY_LEFT_SHIFT)) ec->target.y -= speed;
    if (IsKeyDown(KEY_SPACE)) ec->target.y += speed;

    // gamepad support
    if (IsGamepadAvailable(0)) {
        float deadzone = 0.15f;

        // left stick: move (forward/back, strafe)
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(lx) > deadzone)
            ec->target = Vector3Add(ec->target, Vector3Scale(rightXZ, lx * speed * 2.0f));
        if (fabsf(ly) > deadzone)
            ec->target = Vector3Add(ec->target, Vector3Scale(forwardXZ, -ly * speed * 2.0f));

        // right stick: orbit (look around)
        float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        if (fabsf(rx) > deadzone) ec->yaw += rx * ec->orbitSpeed * 3.0f;
        if (fabsf(ry) > deadzone) {
            ec->pitch += ry * ec->orbitSpeed * 3.0f;
            if (ec->pitch > 1.5f) ec->pitch = 1.5f;
            if (ec->pitch < -1.5f) ec->pitch = -1.5f;
        }

        // triggers: zoom in/out
        float lt = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER);
        float rt = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);
        if (lt > deadzone) {
            ec->distance += lt * ec->zoomSpeed * 0.1f;
            if (ec->distance > 200.0f) ec->distance = 200.0f;
        }
        if (rt > deadzone) {
            ec->distance -= rt * ec->zoomSpeed * 0.1f;
            if (ec->distance < 1.0f) ec->distance = 1.0f;
        }

        // bumpers: move up/down
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1))  ec->target.y -= speed;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) ec->target.y += speed;
    }

    editor_camera_sync(ec);
}

void editor_camera_focus(EditorCamera *ec, Vector3 point, float dist) {
    ec->target = point;
    ec->distance = dist;
    editor_camera_sync(ec);
}
