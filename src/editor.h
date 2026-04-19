#ifndef EDITOR_H
#define EDITOR_H

#include <core/scene.h>
#include <core/camera.h>
#include <core/timeline.h>
#include <core/shadow.h>
#include <core/raycast.h>
#include <core/lighting.h>
#include <core/scripting.h>
#include <ui/ui.h>

struct Editor {
    Scene scene;
    EditorCamera camera;
    Timeline timeline;
    ScriptState scripting;
    EditorUI ui;
    ShadowMap shadowMap;
    LightingState lighting;
    bool running;

    // play mode: snapshot edit state, animate on copy
    ObjectSnapshot snapshot[MAX_OBJECTS];
    int snapshotCount;
    bool playMode;
    int lastKeyframeFrame;  // track frame changes for keyframe evaluation
};

void editor_init(Editor *ed, int screenW, int screenH);
bool editor_should_close(const Editor *ed);
void editor_update(Editor *ed);
void editor_draw(Editor *ed);
void editor_shutdown(Editor *ed);
bool editor_save(Editor *ed, const char *path);
bool editor_load(Editor *ed, const char *path);

#endif // EDITOR_H
