#ifndef CORE_SCENE_H
#define CORE_SCENE_H

#include "types.h"

struct Scene {
    SceneObject objects[MAX_OBJECTS];
    int objectCount;
    uint32_t nextId;        // monotonically increasing ID counter

    // selection (multi-select by ID)
    uint32_t selectedIds[MAX_SELECTED];
    int selectedCount;
};

void         scene_init(Scene *s);
int          scene_add(Scene *s, const char *name, ObjectType type);
void         scene_remove(Scene *s, int index);
void         scene_draw(const Scene *s);
void         scene_draw_selection(const Scene *s);
void         scene_draw_preview(ObjectType type, Vector3 position);
int          scene_get_children(const Scene *s, int parentIndex, int *outChildren, int maxOut);

// selection helpers
bool         scene_is_selected(const Scene *s, uint32_t id);
void         scene_select(Scene *s, uint32_t id, bool toggle, bool add);
void         scene_select_range(Scene *s, int fromIndex, int toIndex);
void         scene_deselect_all(Scene *s);
SceneObject *scene_first_selected(Scene *s);
int          scene_find_by_id(const Scene *s, uint32_t id);

// naming
void         scene_generate_name(const Scene *s, const char *baseName, char *outName, int outLen);

// lookup
SceneObject *scene_get_by_id(Scene *s, uint32_t id);

// texture helpers
Texture2D load_bitmap(const char *path);
void      object_set_texture(SceneObject *obj, const char *path);
void      object_clear_texture(SceneObject *obj);

#endif // CORE_SCENE_H
