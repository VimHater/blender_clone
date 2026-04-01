#ifndef CORE_SHADOW_H
#define CORE_SHADOW_H

#include "types.h"

struct ShadowMap {
    RenderTexture2D depthTarget;
    int resolution;
    Camera3D lightCam;
    Shader depthShader;
    Shader shadowShader;
    bool initialized;
};

void shadowmap_init(ShadowMap *sm, int resolution);
void shadowmap_begin(ShadowMap *sm, Vector3 lightDir);
void shadowmap_end(ShadowMap *sm);
void shadowmap_unload(ShadowMap *sm);

#endif // CORE_SHADOW_H
