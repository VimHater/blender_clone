#ifndef CORE_SCRIPTING_H
#define CORE_SCRIPTING_H

#include "scene.h"
#include "timeline.h"

#define CONSOLE_LOG_MAX 256
#define CONSOLE_LINE_MAX 256

struct ScriptConsole {
    char lines[CONSOLE_LOG_MAX][CONSOLE_LINE_MAX];
    int lineCount;
    bool scrollToBottom;

    // input
    char inputBuf[1024];

    // script file
    char scriptPath[512];
    bool scriptLoaded;
};

struct ScriptState {
    void *L;    // lua_State*, void* to avoid exposing lua.h in header
    Scene *scene;
    Timeline *timeline;
    ScriptConsole console;
    bool playing;
    double startTime;
};

void script_init(ScriptState *ss, Scene *scene, Timeline *timeline);
void script_shutdown(ScriptState *ss);

// load and execute a script file
bool script_load_file(ScriptState *ss, const char *path);

// execute a string (console input)
bool script_exec(ScriptState *ss, const char *code);

// load per-object scripts into __obj_scripts table (call at play start)
void script_load_object_scripts(ScriptState *ss);

// call global animate(t, dt) + per-object scripts each frame
void script_update(ScriptState *ss, float dt);

// playback control
void script_play(ScriptState *ss);
void script_stop(ScriptState *ss);

// console helpers
void console_clear(ScriptConsole *con);
void console_log(ScriptConsole *con, const char *fmt, ...);

#endif // CORE_SCRIPTING_H
