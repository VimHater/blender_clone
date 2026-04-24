#include "shadow.h"
#include "shader_utils.h"
#include <rlgl.h>
#include <cstdio>
#include <cmath>

// ---- Depth-only shaders ----
static const char *VS_DEPTH =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char *FS_DEPTH =
    "#version 330\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    finalColor = vec4(1.0);\n"
    "}\n";

void shadowmap_init(ShadowMap *sm, int resolution) {
    sm->resolution = resolution;
    sm->initialized = false;
    sm->depthTexture = 0;
    sm->lightSpaceMatrix = MatrixIdentity();

    sm->depthShader = load_shader_from_file("depth.vert", "depth.frag", VS_DEPTH, FS_DEPTH);
    if (sm->depthShader.id == 0) {
        printf("[SHADOW] Failed to load depth shader\n");
        return;
    }

    // use raylib's RenderTexture for safe FBO management
    sm->rt = LoadRenderTexture(resolution, resolution);
    if (sm->rt.id == 0) {
        printf("[SHADOW] Failed to create render texture\n");
        return;
    }

    // create a sampleable depth texture and attach it to the FBO,
    // replacing the default depth renderbuffer
    sm->depthTexture = rlLoadTextureDepth(resolution, resolution, false);
    if (sm->depthTexture == 0) {
        printf("[SHADOW] Failed to create depth texture\n");
        UnloadRenderTexture(sm->rt);
        return;
    }

    // replace the default depth attachment with our sampleable texture
    rlFramebufferAttach(sm->rt.id, sm->depthTexture, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    if (!rlFramebufferComplete(sm->rt.id)) {
        printf("[SHADOW] Framebuffer not complete after depth attach\n");
        rlUnloadTexture(sm->depthTexture);
        UnloadRenderTexture(sm->rt);
        sm->depthTexture = 0;
        return;
    }

    sm->initialized = true;
    printf("[SHADOW] Shadow map (%dx%d) initialized OK\n", resolution, resolution);
}

void shadowmap_begin(ShadowMap *sm, Vector3 lightPos, Vector3 lightDir, Vector3 sceneCenter, float sceneRadius) {
    if (!sm->initialized) return;

    Vector3 target = Vector3Add(lightPos, lightDir);
    Vector3 up = (fabsf(lightDir.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Matrix lightView = MatrixLookAt(lightPos, target, up);

    float extent = sceneRadius * 1.5f;
    float farPlane = sceneRadius * 4.0f;
    Matrix lightProj = MatrixOrtho(-extent, extent, -extent, extent, 0.1f, farPlane);

    sm->lightSpaceMatrix = MatrixMultiply(lightView, lightProj);

    Camera3D lightCam = {0};
    lightCam.position = lightPos;
    lightCam.target = target;
    lightCam.up = up;
    lightCam.fovy = extent * 2.0f;
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(sm->rt);
    ClearBackground(WHITE);
    BeginMode3D(lightCam);
    rlSetMatrixProjection(lightProj);
}

void shadowmap_begin_spot(ShadowMap *sm, Vector3 lightPos, Vector3 lightDir, float outerAngleDeg, float sceneRadius) {
    if (!sm->initialized) return;

    Vector3 target = Vector3Add(lightPos, lightDir);
    Vector3 up = (fabsf(lightDir.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Matrix lightView = MatrixLookAt(lightPos, target, up);

    // perspective projection matching the spot cone
    float fov = outerAngleDeg * 2.0f;
    if (fov > 170.0f) fov = 170.0f;
    float farPlane = sceneRadius * 4.0f;
    Matrix lightProj = MatrixPerspective(fov * DEG2RAD, 1.0f, 0.5f, farPlane);

    sm->lightSpaceMatrix = MatrixMultiply(lightView, lightProj);

    Camera3D lightCam = {0};
    lightCam.position = lightPos;
    lightCam.target = target;
    lightCam.up = up;
    lightCam.fovy = fov;
    lightCam.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(sm->rt);
    ClearBackground(WHITE);
    BeginMode3D(lightCam);
    rlSetMatrixProjection(lightProj);
}

void shadowmap_begin_point(ShadowMap *sm, Vector3 lightPos, Vector3 sceneCenter, float sceneRadius) {
    if (!sm->initialized) return;

    Vector3 dir = Vector3Subtract(sceneCenter, lightPos);
    float len = Vector3Length(dir);
    if (len < 0.001f) return;

    // render from the light's actual position — objects behind the light
    // are clipped by the near plane, preventing false shadows
    Vector3 normDir = Vector3Scale(dir, 1.0f / len);
    Vector3 up = (fabsf(normDir.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Matrix lightView = MatrixLookAt(lightPos, sceneCenter, up);

    float extent = sceneRadius * 1.5f;
    float nearPlane = 0.1f;
    float farPlane = len + sceneRadius * 2.0f;
    Matrix lightProj = MatrixOrtho(-extent, extent, -extent, extent, nearPlane, farPlane);

    sm->lightSpaceMatrix = MatrixMultiply(lightView, lightProj);

    Camera3D lightCam = {0};
    lightCam.position = lightPos;
    lightCam.target = sceneCenter;
    lightCam.up = up;
    lightCam.fovy = extent * 2.0f;
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(sm->rt);
    ClearBackground(WHITE);
    BeginMode3D(lightCam);
    rlSetMatrixProjection(lightProj);
}

void shadowmap_end(ShadowMap *sm) {
    if (!sm->initialized) return;
    EndMode3D();
    EndTextureMode();
}

void shadowmap_unload(ShadowMap *sm) {
    if (sm->initialized) {
        // unload our custom depth texture before the RT
        if (sm->depthTexture) {
            // detach so UnloadRenderTexture doesn't double-free
            rlFramebufferAttach(sm->rt.id, 0, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
            rlUnloadTexture(sm->depthTexture);
            sm->depthTexture = 0;
        }
        UnloadRenderTexture(sm->rt);
        UnloadShader(sm->depthShader);
        sm->initialized = false;
    }
}
