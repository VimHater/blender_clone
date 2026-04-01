#ifndef CORE_SCENE_H
#define CORE_SCENE_H

#include "types.h"

struct Scene {
    SceneObject objects[MAX_OBJECTS];
    int objectCount;
    int selectedIndex;      // -1 = none
};

void         scene_init(Scene *s);
int          scene_add(Scene *s, const char *name, ObjectType type);
void         scene_remove(Scene *s, int index);
SceneObject *scene_selected(Scene *s);
void         scene_draw(const Scene *s);
int          scene_get_children(const Scene *s, int parentIndex, int *outChildren, int maxOut);

// texture helpers
Texture2D load_bitmap(const char *path);
void      object_set_texture(SceneObject *obj, const char *path);
void      object_clear_texture(SceneObject *obj);

#endif // CORE_SCENE_H
