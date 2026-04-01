#include "editor.h"

int main() {
    Editor editor;
    editor_init(&editor, 1920, 1080);

    while (!editor_should_close(&editor)) {
        editor_update(&editor);
        editor_draw(&editor);
    }

    editor_shutdown(&editor);
    return 0;
}
