#include "lighting.h"
#include "scene.h"
#include "shader_embed.h"
#include <rlgl.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// ---- Shader table ----

struct ShaderDef {
    const char *vs;
    const char *fs;
    bool hasLighting; // needs light uniform locations
};

static const ShaderDef SHADER_DEFS[SHADER_COUNT] = {
    { SHADER_SRC_COMMON_VERT, SHADER_SRC_DEFAULT_FRAG,    true  }, // SHADER_DEFAULT
    { SHADER_SRC_COMMON_VERT, SHADER_SRC_UNLIT_FRAG,      false }, // SHADER_UNLIT
    { SHADER_SRC_COMMON_VERT, SHADER_SRC_TOON_FRAG,       true  }, // SHADER_TOON
    { SHADER_SRC_COMMON_VERT, SHADER_SRC_NORMAL_VIS_FRAG, false }, // SHADER_NORMAL_VIS
    { SHADER_SRC_COMMON_VERT, SHADER_SRC_FRESNEL_FRAG,    true  }, // SHADER_FRESNEL
};

static void resolve_light_uniforms(LightingState *ls, int idx) {
    Shader s = ls->shaders[idx];
    ls->lightCountLoc[idx] = GetShaderLocation(s, "lightCount");
    ls->ambientLoc[idx] = GetShaderLocation(s, "ambient");
    ls->viewPosLoc[idx] = GetShaderLocation(s, "viewPos");
    ls->lightSpaceMatrixLoc[idx] = GetShaderLocation(s, "lightSpaceMatrix");
    ls->shadowMapLoc[idx] = GetShaderLocation(s, "shadowMap");
    ls->hasShadowLoc[idx] = GetShaderLocation(s, "hasShadow");
    ls->shadowLightIndexLoc[idx] = GetShaderLocation(s, "shadowLightIndex");
    ls->shadowIsPointLoc[idx] = GetShaderLocation(s, "shadowIsPoint");
    ls->shadowLightPosLoc[idx] = GetShaderLocation(s, "shadowLightPos");
    ls->shadowViewDirLoc[idx] = GetShaderLocation(s, "shadowViewDir");

    char buf[64];
    for (int i = 0; i < MAX_SHADER_LIGHTS; i++) {
        snprintf(buf, sizeof(buf), "lightType[%d]", i);
        ls->lightLocs[idx][i][0] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightPos[%d]", i);
        ls->lightLocs[idx][i][1] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightDir[%d]", i);
        ls->lightLocs[idx][i][2] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightColor[%d]", i);
        ls->lightLocs[idx][i][3] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightIntensity[%d]", i);
        ls->lightLocs[idx][i][4] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightSpotInnerCos[%d]", i);
        ls->lightLocs[idx][i][5] = GetShaderLocation(s, buf);
        snprintf(buf, sizeof(buf), "lightSpotOuterCos[%d]", i);
        ls->lightLocs[idx][i][6] = GetShaderLocation(s, buf);
    }
}

void lighting_init(LightingState *ls) {
    memset(ls, 0, sizeof(*ls));
    ls->ambientColor[0] = 0.45f;
    ls->ambientColor[1] = 0.45f;
    ls->ambientColor[2] = 0.45f;
    ls->ambientColor[3] = 1.0f;

    for (int s = 0; s < SHADER_COUNT; s++) {
        ls->shaders[s] = LoadShaderFromMemory(SHADER_DEFS[s].vs, SHADER_DEFS[s].fs);
        ls->shaderLoaded[s] = (ls->shaders[s].id != 0);

        if (!ls->shaderLoaded[s]) {
            printf("[LIGHTING] Failed to load shader %d\n", s);
            continue;
        }

        // common raylib locs
        ls->shaders[s].locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(ls->shaders[s], "matModel");
        ls->shaders[s].locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(ls->shaders[s], "matNormal");
        ls->shaders[s].locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(ls->shaders[s], "viewPos");

        if (SHADER_DEFS[s].hasLighting) {
            resolve_light_uniforms(ls, s);
        }
    }

    printf("[LIGHTING] All shaders loaded\n");
}

void lighting_shutdown(LightingState *ls) {
    for (int s = 0; s < SHADER_COUNT; s++) {
        if (ls->shaderLoaded[s]) {
            UnloadShader(ls->shaders[s]);
            ls->shaderLoaded[s] = false;
        }
    }
}

void lighting_collect(LightingState *ls, const Scene *s) {
    ls->lightCount = 0;
    for (int i = 0; i < s->objectCount && ls->lightCount < MAX_SHADER_LIGHTS; i++) {
        const SceneObject *obj = &s->objects[i];
        if (!obj->active || !obj->visible || obj->type != OBJ_LIGHT) continue;

        LightData *ld = &ls->lights[ls->lightCount];
        ld->type = (int)obj->lightType;
        ld->position = obj->transform.position;
        Matrix rotMat = MatrixRotateXYZ(Vector3{
            obj->transform.rotation.x * DEG2RAD,
            obj->transform.rotation.y * DEG2RAD,
            obj->transform.rotation.z * DEG2RAD
        });
        Vector3 dir = Vector3Transform(Vector3{0, -1, 0}, rotMat);
        ld->direction = dir;
        ld->color[0] = obj->lightColor.r / 255.0f;
        ld->color[1] = obj->lightColor.g / 255.0f;
        ld->color[2] = obj->lightColor.b / 255.0f;
        ld->intensity = obj->lightIntensity;
        ld->spotInnerCos = cosf(obj->spotInnerAngle * DEG2RAD);
        ld->spotOuterCos = cosf(obj->spotOuterAngle * DEG2RAD);
        ls->lightCount++;
    }
}

void lighting_update_shader(LightingState *ls, Vector3 cameraPos) {
    float cp[3] = { cameraPos.x, cameraPos.y, cameraPos.z };

    for (int s = 0; s < SHADER_COUNT; s++) {
        if (!ls->shaderLoaded[s] || !SHADER_DEFS[s].hasLighting) continue;

        SetShaderValue(ls->shaders[s], ls->viewPosLoc[s], cp, SHADER_UNIFORM_VEC3);
        SetShaderValue(ls->shaders[s], ls->ambientLoc[s], ls->ambientColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(ls->shaders[s], ls->lightCountLoc[s], &ls->lightCount, SHADER_UNIFORM_INT);

        for (int i = 0; i < ls->lightCount; i++) {
            LightData *ld = &ls->lights[i];
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][0], &ld->type, SHADER_UNIFORM_INT);
            float pos[3] = { ld->position.x, ld->position.y, ld->position.z };
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][1], pos, SHADER_UNIFORM_VEC3);
            float dir[3] = { ld->direction.x, ld->direction.y, ld->direction.z };
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][2], dir, SHADER_UNIFORM_VEC3);
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][3], ld->color, SHADER_UNIFORM_VEC3);
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][4], &ld->intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][5], &ld->spotInnerCos, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ls->shaders[s], ls->lightLocs[s][i][6], &ld->spotOuterCos, SHADER_UNIFORM_FLOAT);
        }
    }
}

void lighting_bind_shadow(LightingState *ls, ShadowMap *sm, int shadowLightIndex, bool isPoint, Vector3 lightPos, Vector3 viewDir) {
    bool active = sm && sm->initialized;
    int flag = active ? 1 : 0;
    int pointFlag = isPoint ? 1 : 0;

    for (int s = 0; s < SHADER_COUNT; s++) {
        if (!ls->shaderLoaded[s] || !SHADER_DEFS[s].hasLighting) continue;

        SetShaderValue(ls->shaders[s], ls->hasShadowLoc[s], &flag, SHADER_UNIFORM_INT);
        SetShaderValue(ls->shaders[s], ls->shadowLightIndexLoc[s], &shadowLightIndex, SHADER_UNIFORM_INT);
        SetShaderValue(ls->shaders[s], ls->shadowIsPointLoc[s], &pointFlag, SHADER_UNIFORM_INT);
        float lp[3] = { lightPos.x, lightPos.y, lightPos.z };
        float vd[3] = { viewDir.x, viewDir.y, viewDir.z };
        SetShaderValue(ls->shaders[s], ls->shadowLightPosLoc[s], lp, SHADER_UNIFORM_VEC3);
        SetShaderValue(ls->shaders[s], ls->shadowViewDirLoc[s], vd, SHADER_UNIFORM_VEC3);

        if (active) {
            int slot = 1;
            SetShaderValue(ls->shaders[s], ls->shadowMapLoc[s], &slot, SHADER_UNIFORM_INT);
            SetShaderValueMatrix(ls->shaders[s], ls->lightSpaceMatrixLoc[s], sm->lightSpaceMatrix);
        }
    }

    // bind depth texture to slot 1
    if (active) {
        rlActiveTextureSlot(1);
        rlEnableTexture(sm->depthTexture);
        rlActiveTextureSlot(0);
    }
}
