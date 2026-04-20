#ifndef CORE_LIGHTING_H
#define CORE_LIGHTING_H

#include "types.h"
#include "shadow.h"

#define MAX_SHADER_LIGHTS MAX_LIGHTS

struct LightData {
    int type;       // 0=point, 1=directional, 2=spot
    Vector3 position;
    Vector3 direction;
    float color[3];
    float intensity;
    float spotInnerCos;  // cos(innerAngle)
    float spotOuterCos;  // cos(outerAngle)
};

struct LightingState {
    Shader shaders[SHADER_COUNT];
    bool shaderLoaded[SHADER_COUNT];

    // uniform locations for lit shaders (DEFAULT and TOON share the same layout)
    int lightCountLoc[SHADER_COUNT];
    int ambientLoc[SHADER_COUNT];
    int viewPosLoc[SHADER_COUNT];
    int lightLocs[SHADER_COUNT][MAX_SHADER_LIGHTS][7]; // type, pos, dir, color, intensity, spotInnerCos, spotOuterCos

    // shadow uniforms
    int lightSpaceMatrixLoc[SHADER_COUNT];
    int shadowMapLoc[SHADER_COUNT];
    int hasShadowLoc[SHADER_COUNT];
    int shadowLightIndexLoc[SHADER_COUNT];
    int shadowIsPointLoc[SHADER_COUNT];
    int shadowLightPosLoc[SHADER_COUNT];
    int shadowViewDirLoc[SHADER_COUNT];

    float ambientColor[4];
    LightData lights[MAX_SHADER_LIGHTS];
    int lightCount;
};

void lighting_init(LightingState *ls);
void lighting_shutdown(LightingState *ls);
void lighting_collect(LightingState *ls, const struct Scene *s);
void lighting_update_shader(LightingState *ls, Vector3 cameraPos);
void lighting_bind_shadow(LightingState *ls, ShadowMap *sm, int shadowLightIndex = 0, bool isPoint = false, Vector3 lightPos = {0}, Vector3 viewDir = {0});

#endif // CORE_LIGHTING_H
