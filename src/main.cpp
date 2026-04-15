#include <editor.h>
#define DEFAULT_SCREEN_W 1920
#define DEFAULT_SCREEN_H 1080

int main(int argc, char **argv) {
    const char *filePath = (argc > 1) ? argv[1] : NULL;

    Editor *editor = new Editor();
    editor_init(editor, DEFAULT_SCREEN_W, DEFAULT_SCREEN_H);

    if (filePath) {
        editor_load(editor, filePath);
    }

    while (!editor_should_close(editor)) {
        editor_update(editor);
        editor_draw(editor);
    }

    editor_shutdown(editor);
    delete editor;
    return 0;
}
