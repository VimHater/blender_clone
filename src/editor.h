#ifndef EDITOR_H
#define EDITOR_H

#include <core/scene.h>
#include <core/camera.h>
#include <core/timeline.h>
#include <core/shadow.h>
#include <core/raycast.h>
#include <ui/ui.h>

struct Editor {
    Scene scene;
    EditorCamera camera;
    Timeline timeline;
    EditorUI ui;
    ShadowMap shadowMap;
    bool running;
};

void editor_init(Editor *ed, int screenW, int screenH);
bool editor_should_close(const Editor *ed);
void editor_update(Editor *ed);
void editor_draw(Editor *ed);
void editor_shutdown(Editor *ed);

#endif // EDITOR_H
