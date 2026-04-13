#include "lighting.h"
#include "scene.h"
#include <rlgl.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// ---- Shared vertex shader (GLSL 330) ----

static const char *VS_COMMON =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "in vec3 vertexNormal;\n"
    "in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "uniform mat4 matNormal;\n"
    "out vec3 fragPosition;\n"
    "out vec2 fragTexCoord;\n"
    "out vec3 fragNormal;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "    fragColor = vertexColor;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

// ---- Fragment shaders ----

// Default: Blinn-Phong
static const char *FS_DEFAULT =
    "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec4 ambient;\n"
    "#define MAX_LIGHTS 8\n"
    "#define LIGHT_POINT 0\n"
    "#define LIGHT_DIRECTIONAL 1\n"
    "uniform int lightCount;\n"
    "uniform int lightType[MAX_LIGHTS];\n"
    "uniform vec3 lightPos[MAX_LIGHTS];\n"
    "uniform vec3 lightDir[MAX_LIGHTS];\n"
    "uniform vec3 lightColor[MAX_LIGHTS];\n"
    "uniform float lightIntensity[MAX_LIGHTS];\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    vec4 tint = colDiffuse * fragColor;\n"
    "    vec3 baseColor = texelColor.rgb * tint.rgb;\n"
    "    vec3 normal = normalize(fragNormal);\n"
    "    vec3 viewDir = normalize(viewPos - fragPosition);\n"
    "    vec3 totalDiffuse = vec3(0.0);\n"
    "    vec3 totalSpecular = vec3(0.0);\n"
    "    for (int i = 0; i < MAX_LIGHTS; i++) {\n"
    "        if (i >= lightCount) break;\n"
    "        vec3 lightVec;\n"
    "        float attenuation = 1.0;\n"
    "        if (lightType[i] == LIGHT_POINT) {\n"
    "            lightVec = lightPos[i] - fragPosition;\n"
    "            float dist = length(lightVec);\n"
    "            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);\n"
    "            lightVec = normalize(lightVec);\n"
    "        } else {\n"
    "            lightVec = normalize(-lightDir[i]);\n"
    "        }\n"
    "        float NdotL = max(dot(normal, lightVec), 0.0);\n"
    "        float specCo = 0.0;\n"
    "        if (NdotL > 0.0) {\n"
    "            vec3 halfDir = normalize(lightVec + viewDir);\n"
    "            specCo = pow(max(dot(normal, halfDir), 0.0), 32.0);\n"
    "        }\n"
    "        vec3 radiance = lightColor[i] * lightIntensity[i] * attenuation;\n"
    "        totalDiffuse += radiance * NdotL;\n"
    "        totalSpecular += radiance * specCo;\n"
    "    }\n"
    "    vec3 ambientTerm = ambient.rgb * baseColor;\n"
    "    vec3 diffuseTerm = totalDiffuse * baseColor;\n"
    "    vec3 specularTerm = totalSpecular * 0.25;\n"
    "    vec3 result = ambientTerm + diffuseTerm + specularTerm;\n"
    "    result = pow(result, vec3(1.0 / 2.2));\n"
    "    finalColor = vec4(result, texelColor.a * tint.a);\n"
    "}\n";

// Unlit: flat color/texture, no lighting
static const char *FS_UNLIT =
    "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    finalColor = texelColor * colDiffuse * fragColor;\n"
    "}\n";

// Toon: cel shading with discrete light bands
static const char *FS_TOON =
    "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec4 ambient;\n"
    "#define MAX_LIGHTS 8\n"
    "#define LIGHT_POINT 0\n"
    "#define LIGHT_DIRECTIONAL 1\n"
    "uniform int lightCount;\n"
    "uniform int lightType[MAX_LIGHTS];\n"
    "uniform vec3 lightPos[MAX_LIGHTS];\n"
    "uniform vec3 lightDir[MAX_LIGHTS];\n"
    "uniform vec3 lightColor[MAX_LIGHTS];\n"
    "uniform float lightIntensity[MAX_LIGHTS];\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    vec4 tint = colDiffuse * fragColor;\n"
    "    vec3 baseColor = texelColor.rgb * tint.rgb;\n"
    "    vec3 normal = normalize(fragNormal);\n"
    "    float totalLight = 0.0;\n"
    "    for (int i = 0; i < MAX_LIGHTS; i++) {\n"
    "        if (i >= lightCount) break;\n"
    "        vec3 lightVec;\n"
    "        float attenuation = 1.0;\n"
    "        if (lightType[i] == LIGHT_POINT) {\n"
    "            lightVec = lightPos[i] - fragPosition;\n"
    "            float dist = length(lightVec);\n"
    "            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);\n"
    "            lightVec = normalize(lightVec);\n"
    "        } else {\n"
    "            lightVec = normalize(-lightDir[i]);\n"
    "        }\n"
    "        totalLight += max(dot(normal, lightVec), 0.0) * lightIntensity[i] * attenuation;\n"
    "    }\n"
    "    // quantize into 4 bands\n"
    "    float band;\n"
    "    if (totalLight > 0.75) band = 1.0;\n"
    "    else if (totalLight > 0.4) band = 0.7;\n"
    "    else if (totalLight > 0.15) band = 0.4;\n"
    "    else band = 0.2;\n"
    "    vec3 result = baseColor * (ambient.rgb + vec3(band));\n"
    "    result = min(result, vec3(1.0));\n"
    "    finalColor = vec4(result, texelColor.a * tint.a);\n"
    "}\n";

// Normal visualization: maps normal direction to RGB
static const char *FS_NORMAL_VIS =
    "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec3 normal = normalize(fragNormal);\n"
    "    finalColor = vec4(normal * 0.5 + 0.5, 1.0);\n"
    "}\n";

// Fresnel: rim lighting / edge glow
static const char *FS_FRESNEL =
    "#version 330\n"
    "in vec3 fragPosition;\n"
    "in vec2 fragTexCoord;\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec4 ambient;\n"
    "#define MAX_LIGHTS 8\n"
    "#define LIGHT_POINT 0\n"
    "#define LIGHT_DIRECTIONAL 1\n"
    "uniform int lightCount;\n"
    "uniform int lightType[MAX_LIGHTS];\n"
    "uniform vec3 lightPos[MAX_LIGHTS];\n"
    "uniform vec3 lightDir[MAX_LIGHTS];\n"
    "uniform vec3 lightColor[MAX_LIGHTS];\n"
    "uniform float lightIntensity[MAX_LIGHTS];\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    vec4 tint = colDiffuse * fragColor;\n"
    "    vec3 baseColor = texelColor.rgb * tint.rgb;\n"
    "    vec3 normal = normalize(fragNormal);\n"
    "    vec3 viewDir = normalize(viewPos - fragPosition);\n"
    "    // standard diffuse\n"
    "    vec3 totalDiffuse = vec3(0.0);\n"
    "    for (int i = 0; i < MAX_LIGHTS; i++) {\n"
    "        if (i >= lightCount) break;\n"
    "        vec3 lightVec;\n"
    "        float attenuation = 1.0;\n"
    "        if (lightType[i] == LIGHT_POINT) {\n"
    "            lightVec = lightPos[i] - fragPosition;\n"
    "            float dist = length(lightVec);\n"
    "            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);\n"
    "            lightVec = normalize(lightVec);\n"
    "        } else {\n"
    "            lightVec = normalize(-lightDir[i]);\n"
    "        }\n"
    "        float NdotL = max(dot(normal, lightVec), 0.0);\n"
    "        totalDiffuse += lightColor[i] * lightIntensity[i] * attenuation * NdotL;\n"
    "    }\n"
    "    // fresnel rim\n"
    "    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);\n"
    "    vec3 rimColor = vec3(0.6, 0.8, 1.0);\n"
    "    vec3 result = baseColor * (ambient.rgb + totalDiffuse) + rimColor * fresnel * 0.8;\n"
    "    result = pow(result, vec3(1.0 / 2.2));\n"
    "    finalColor = vec4(result, texelColor.a * tint.a);\n"
    "}\n";

// ---- Shader table ----

struct ShaderDef {
    const char *vs;
    const char *fs;
    bool hasLighting; // needs light uniform locations
};

static const ShaderDef SHADER_DEFS[SHADER_COUNT] = {
    { VS_COMMON, FS_DEFAULT,    true  }, // SHADER_DEFAULT
    { VS_COMMON, FS_UNLIT,      false }, // SHADER_UNLIT
    { VS_COMMON, FS_TOON,       true  }, // SHADER_TOON
    { VS_COMMON, FS_NORMAL_VIS, false }, // SHADER_NORMAL_VIS
    { VS_COMMON, FS_FRESNEL,    true  }, // SHADER_FRESNEL
};

static void resolve_light_uniforms(LightingState *ls, int idx) {
    Shader s = ls->shaders[idx];
    ls->lightCountLoc[idx] = GetShaderLocation(s, "lightCount");
    ls->ambientLoc[idx] = GetShaderLocation(s, "ambient");
    ls->viewPosLoc[idx] = GetShaderLocation(s, "viewPos");

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
        }
    }
}
