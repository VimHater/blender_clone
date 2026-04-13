#include "editor.h"
#define DEFAULT_SCREEN_W 1920
#define DEFAULT_SCREEN_H 1080

int main() {

    Editor editor;
    editor_init(&editor, DEFAULT_SCREEN_W, DEFAULT_SCREEN_H);

    while (!editor_should_close(&editor)) {
        editor_update(&editor);
        editor_draw(&editor);
    }

    editor_shutdown(&editor);
    return 0;
}
