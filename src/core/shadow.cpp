#include "shadow.h"

void shadowmap_init(ShadowMap *sm, int resolution) {
    sm->resolution = resolution;
    sm->initialized = false;
}

void shadowmap_begin(ShadowMap *sm, Vector3 lightDir) {
    (void)sm; (void)lightDir;
}

void shadowmap_end(ShadowMap *sm) {
    (void)sm;
}

void shadowmap_unload(ShadowMap *sm) {
    if (sm->initialized) {
        UnloadRenderTexture(sm->depthTarget);
        UnloadShader(sm->depthShader);
        UnloadShader(sm->shadowShader);
        sm->initialized = false;
    }
}
