#ifndef CORE_SHADOW_H
#define CORE_SHADOW_H

#include "types.h"

struct ShadowMap {
    RenderTexture2D rt;          // raylib render texture (color + depth)
    unsigned int depthTexture;   // separate depth texture we can sample
    int resolution;
    Shader depthShader;
    bool initialized;

    Matrix lightSpaceMatrix;
};

void shadowmap_init(ShadowMap *sm, int resolution);
void shadowmap_begin(ShadowMap *sm, Vector3 lightDir, Vector3 sceneCenter, float sceneRadius);
void shadowmap_begin_point(ShadowMap *sm, Vector3 lightPos, Vector3 sceneCenter, float sceneRadius);
void shadowmap_end(ShadowMap *sm);
void shadowmap_unload(ShadowMap *sm);

#endif // CORE_SHADOW_H
