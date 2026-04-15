#include "shadow.h"
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

    sm->depthShader = LoadShaderFromMemory(VS_DEPTH, FS_DEPTH);
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

void shadowmap_begin(ShadowMap *sm, Vector3 lightDir, Vector3 sceneCenter, float sceneRadius) {
    if (!sm->initialized) return;

    // compute light view
    Vector3 lightPos = Vector3Add(sceneCenter,
        Vector3Scale(Vector3Negate(lightDir), sceneRadius * 2.0f));
    Vector3 up = (fabsf(lightDir.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Matrix lightView = MatrixLookAt(lightPos, sceneCenter, up);

    float extent = sceneRadius * 1.5f;
    Matrix lightProj = MatrixOrtho(-extent, extent, -extent, extent, 0.1f, sceneRadius * 5.0f);

    sm->lightSpaceMatrix = MatrixMultiply(lightView, lightProj);

    // build a Camera3D for the light
    Camera3D lightCam = {0};
    lightCam.position = lightPos;
    lightCam.target = sceneCenter;
    lightCam.up = up;
    lightCam.fovy = extent * 2.0f; // used as ortho height
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(sm->rt);
    ClearBackground(WHITE);
    BeginMode3D(lightCam);

    // override projection with our exact ortho matrix
    rlSetMatrixProjection(lightProj);
}

void shadowmap_begin_point(ShadowMap *sm, Vector3 lightPos, Vector3 sceneCenter, float sceneRadius) {
    if (!sm->initialized) return;

    Vector3 dir = Vector3Subtract(sceneCenter, lightPos);
    float len = Vector3Length(dir);
    if (len < 0.001f) return;

    Vector3 up = (fabsf(dir.y / len) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Matrix lightView = MatrixLookAt(lightPos, sceneCenter, up);

    // use orthographic projection centered on the scene so the entire scene
    // is covered regardless of light position — no hard frustum cutoff
    float extent = sceneRadius * 1.5f;
    float dist = len + sceneRadius * 2.0f;
    Matrix lightProj = MatrixOrtho(-extent, extent, -extent, extent, 0.1f, dist);

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
