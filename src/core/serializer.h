#ifndef CORE_SERIALIZER_H
#define CORE_SERIALIZER_H

#include "types.h"
#include "camera.h"
#include "timeline.h"
#include "../ui/ui.h"

struct Scene;

struct EditorState {
    Scene *scene;
    EditorCamera *camera;
    Timeline *timeline;
    EditorUI *ui;
};

// Save/load in human-readable text format (.scene)
bool save_text(const char *path, const EditorState *state);
bool load_text(const char *path, EditorState *state);

// Save/load in binary format (.sceneb)
bool save_binary(const char *path, const EditorState *state);
bool load_binary(const char *path, EditorState *state);

#endif // CORE_SERIALIZER_H
