#include "scripting.h"
#include "scene.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// ---- Console ----

void console_clear(ScriptConsole *con) {
    con->lineCount = 0;
}

void console_log(ScriptConsole *con, const char *fmt, ...) {
    if (con->lineCount >= CONSOLE_LOG_MAX) {
        // shift up
        for (int i = 0; i < CONSOLE_LOG_MAX - 1; i++) {
            memcpy(con->lines[i], con->lines[i + 1], CONSOLE_LINE_MAX);
        }
        con->lineCount = CONSOLE_LOG_MAX - 1;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(con->lines[con->lineCount], CONSOLE_LINE_MAX, fmt, args);
    va_end(args);
    con->lineCount++;
    con->scrollToBottom = true;
}

// ---- Helpers to get ScriptState from Lua ----

static ScriptState *get_ss(lua_State *L) {
    lua_getglobal(L, "__script_state");
    ScriptState *ss = (ScriptState *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ss;
}

static SceneObject *find_object(Scene *s, lua_State *L, int argIdx) {
    // accept name (string) or id (integer)
    if (lua_isinteger(L, argIdx)) {
        int id = (int)lua_tointeger(L, argIdx);
        return scene_get_by_id(s, (uint32_t)id);
    } else if (lua_isstring(L, argIdx)) {
        const char *name = lua_tostring(L, argIdx);
        for (int i = 0; i < s->objectCount; i++) {
            if (strcmp(s->objects[i].name, name) == 0 && s->objects[i].active) {
                return &s->objects[i];
            }
        }
    }
    return NULL;
}

// ---- Lua API functions ----

// print(...) — output to console
static int l_print(lua_State *L) {
    ScriptState *ss = get_ss(L);
    int n = lua_gettop(L);
    char buf[CONSOLE_LINE_MAX] = {0};
    int pos = 0;
    for (int i = 1; i <= n; i++) {
        const char *s = luaL_tolstring(L, i, NULL);
        if (i > 1) pos += snprintf(buf + pos, CONSOLE_LINE_MAX - pos, "\t");
        pos += snprintf(buf + pos, CONSOLE_LINE_MAX - pos, "%s", s);
        lua_pop(L, 1);
    }
    console_log(&ss->console, "%s", buf);
    return 0;
}

// get_position(name_or_id) -> x, y, z
static int l_get_position(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    lua_pushnumber(L, obj->transform.position.x);
    lua_pushnumber(L, obj->transform.position.y);
    lua_pushnumber(L, obj->transform.position.z);
    return 3;
}

// set_position(name_or_id, x, y, z)
static int l_set_position(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->transform.position.x = (float)luaL_checknumber(L, 2);
    obj->transform.position.y = (float)luaL_checknumber(L, 3);
    obj->transform.position.z = (float)luaL_checknumber(L, 4);
    return 0;
}

// get_rotation(name_or_id) -> rx, ry, rz (degrees)
static int l_get_rotation(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    lua_pushnumber(L, obj->transform.rotation.x);
    lua_pushnumber(L, obj->transform.rotation.y);
    lua_pushnumber(L, obj->transform.rotation.z);
    return 3;
}

// set_rotation(name_or_id, rx, ry, rz)
static int l_set_rotation(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->transform.rotation.x = (float)luaL_checknumber(L, 2);
    obj->transform.rotation.y = (float)luaL_checknumber(L, 3);
    obj->transform.rotation.z = (float)luaL_checknumber(L, 4);
    return 0;
}

// get_scale(name_or_id) -> sx, sy, sz
static int l_get_scale(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    lua_pushnumber(L, obj->transform.scale.x);
    lua_pushnumber(L, obj->transform.scale.y);
    lua_pushnumber(L, obj->transform.scale.z);
    return 3;
}

// set_scale(name_or_id, sx, sy, sz)
static int l_set_scale(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->transform.scale.x = (float)luaL_checknumber(L, 2);
    obj->transform.scale.y = (float)luaL_checknumber(L, 3);
    obj->transform.scale.z = (float)luaL_checknumber(L, 4);
    return 0;
}

// set_color(name_or_id, r, g, b) — 0-255
static int l_set_color(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->material.color.r = (unsigned char)luaL_checkinteger(L, 2);
    obj->material.color.g = (unsigned char)luaL_checkinteger(L, 3);
    obj->material.color.b = (unsigned char)luaL_checkinteger(L, 4);
    return 0;
}

// get_color(name_or_id) -> r, g, b
static int l_get_color(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    lua_pushinteger(L, obj->material.color.r);
    lua_pushinteger(L, obj->material.color.g);
    lua_pushinteger(L, obj->material.color.b);
    return 3;
}

// set_visible(name_or_id, bool)
static int l_set_visible(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->visible = lua_toboolean(L, 2);
    return 0;
}

// list_objects() -> table of names
static int l_list_objects(lua_State *L) {
    ScriptState *ss = get_ss(L);
    lua_newtable(L);
    int idx = 1;
    for (int i = 0; i < ss->scene->objectCount; i++) {
        if (!ss->scene->objects[i].active) continue;
        lua_pushstring(L, ss->scene->objects[i].name);
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// get_object_count() -> int
static int l_get_object_count(lua_State *L) {
    ScriptState *ss = get_ss(L);
    int count = 0;
    for (int i = 0; i < ss->scene->objectCount; i++) {
        if (ss->scene->objects[i].active) count++;
    }
    lua_pushinteger(L, count);
    return 1;
}

// ---- Physics API ----

// set_velocity(name, vx, vy, vz)
static int l_set_velocity(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->velocity.x = (float)luaL_checknumber(L, 2);
    obj->velocity.y = (float)luaL_checknumber(L, 3);
    obj->velocity.z = (float)luaL_checknumber(L, 4);
    return 0;
}

// get_velocity(name) -> vx, vy, vz
static int l_get_velocity(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    lua_pushnumber(L, obj->velocity.x);
    lua_pushnumber(L, obj->velocity.y);
    lua_pushnumber(L, obj->velocity.z);
    return 3;
}

// add_force(name, fx, fy, fz) — impulse: changes velocity by force/mass
static int l_add_force(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    float invMass = (obj->mass > 0) ? 1.0f / obj->mass : 1.0f;
    obj->velocity.x += (float)luaL_checknumber(L, 2) * invMass;
    obj->velocity.y += (float)luaL_checknumber(L, 3) * invMass;
    obj->velocity.z += (float)luaL_checknumber(L, 4) * invMass;
    return 0;
}

// set_gravity(name, enabled)
static int l_set_gravity(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->useGravity = lua_toboolean(L, 2);
    return 0;
}

// set_physics(name, enabled)
static int l_set_physics(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->usePhysics = lua_toboolean(L, 2);
    return 0;
}

// set_static(name, enabled)
static int l_set_static(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->isStatic = lua_toboolean(L, 2);
    return 0;
}

// set_mass(name, mass)
static int l_set_mass(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->mass = (float)luaL_checknumber(L, 2);
    if (obj->mass < 0.001f) obj->mass = 0.001f;
    return 0;
}

// set_restitution(name, value)
static int l_set_restitution(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *obj = find_object(ss->scene, L, 1);
    if (!obj) return luaL_error(L, "object not found");
    obj->restitution = (float)luaL_checknumber(L, 2);
    return 0;
}

// raycast(ox, oy, oz, dx, dy, dz) -> hit, name, x, y, z, nx, ny, nz
static int l_raycast(lua_State *L) {
    ScriptState *ss = get_ss(L);
    Ray ray;
    ray.position.x = (float)luaL_checknumber(L, 1);
    ray.position.y = (float)luaL_checknumber(L, 2);
    ray.position.z = (float)luaL_checknumber(L, 3);
    ray.direction.x = (float)luaL_checknumber(L, 4);
    ray.direction.y = (float)luaL_checknumber(L, 5);
    ray.direction.z = (float)luaL_checknumber(L, 6);
    // normalize direction
    float len = sqrtf(ray.direction.x*ray.direction.x + ray.direction.y*ray.direction.y + ray.direction.z*ray.direction.z);
    if (len > 0) { ray.direction.x /= len; ray.direction.y /= len; ray.direction.z /= len; }

    float closestDist = 1e9f;
    int closestIdx = -1;
    RayCollision closestHit = {0};

    for (int i = 0; i < ss->scene->objectCount; i++) {
        SceneObject *obj = &ss->scene->objects[i];
        if (!obj->active || !obj->visible) continue;
        if (obj->type == OBJ_CAMERA || obj->type == OBJ_LIGHT) continue;

        BoundingBox bb = scene_get_bounds(ss->scene, i);
        RayCollision hit = GetRayCollisionBox(ray, bb);
        if (hit.hit && hit.distance < closestDist) {
            closestDist = hit.distance;
            closestIdx = i;
            closestHit = hit;
        }
    }

    if (closestIdx < 0) {
        lua_pushboolean(L, false);
        return 1;
    }

    lua_pushboolean(L, true);
    lua_pushstring(L, ss->scene->objects[closestIdx].name);
    lua_pushnumber(L, closestHit.point.x);
    lua_pushnumber(L, closestHit.point.y);
    lua_pushnumber(L, closestHit.point.z);
    lua_pushnumber(L, closestHit.normal.x);
    lua_pushnumber(L, closestHit.normal.y);
    lua_pushnumber(L, closestHit.normal.z);
    return 8;
}

// check_collision(name1, name2) -> bool
static int l_check_collision(lua_State *L) {
    ScriptState *ss = get_ss(L);
    SceneObject *a = find_object(ss->scene, L, 1);
    SceneObject *b = find_object(ss->scene, L, 2);
    if (!a || !b) { lua_pushboolean(L, false); return 1; }

    int idxA = (int)(a - ss->scene->objects);
    int idxB = (int)(b - ss->scene->objects);
    BoundingBox bbA = scene_get_bounds(ss->scene, idxA);
    BoundingBox bbB = scene_get_bounds(ss->scene, idxB);
    lua_pushboolean(L, CheckCollisionBoxes(bbA, bbB));
    return 1;
}

// ---- Registration ----

static const luaL_Reg api_funcs[] = {
    {"get_position",    l_get_position},
    {"set_position",    l_set_position},
    {"get_rotation",    l_get_rotation},
    {"set_rotation",    l_set_rotation},
    {"get_scale",       l_get_scale},
    {"set_scale",       l_set_scale},
    {"set_color",       l_set_color},
    {"get_color",       l_get_color},
    {"set_visible",     l_set_visible},
    {"list_objects",    l_list_objects},
    {"get_object_count", l_get_object_count},
    {"set_velocity",    l_set_velocity},
    {"get_velocity",    l_get_velocity},
    {"add_force",       l_add_force},
    {"set_gravity",     l_set_gravity},
    {"set_physics",     l_set_physics},
    {"set_static",      l_set_static},
    {"set_mass",        l_set_mass},
    {"set_restitution", l_set_restitution},
    {"raycast",         l_raycast},
    {"check_collision", l_check_collision},
    {NULL, NULL}
};

// ---- Error handler for pcall ----

static int lua_error_handler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg, 1);
    return 1;
}

// ---- Public API ----

void script_init(ScriptState *ss, Scene *scene, Timeline *timeline) {
    memset(ss, 0, sizeof(*ss));
    ss->scene = scene;
    ss->timeline = timeline;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    ss->L = L;

    // sandbox: remove dangerous functions
    lua_pushnil(L); lua_setglobal(L, "os");
    lua_pushnil(L); lua_setglobal(L, "io");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "dofile");

    // store ScriptState pointer in Lua for C API callbacks
    lua_pushlightuserdata(L, ss);
    lua_setglobal(L, "__script_state");

    // register API functions
    for (const luaL_Reg *f = api_funcs; f->name; f++) {
        lua_pushcfunction(L, f->func);
        lua_setglobal(L, f->name);
    }

    // override print
    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");

    console_log(&ss->console, "Lua %s initialized", LUA_VERSION);
    console_log(&ss->console, "Type 'help()' for available functions");

    // register help function
    const char *help_code =
        "function help()\n"
        "  print('--- Animation API ---')\n"
        "  print('set_position(name, x, y, z)')\n"
        "  print('get_position(name) -> x, y, z')\n"
        "  print('set_rotation(name, rx, ry, rz)')\n"
        "  print('get_rotation(name) -> rx, ry, rz')\n"
        "  print('set_scale(name, sx, sy, sz)')\n"
        "  print('get_scale(name) -> sx, sy, sz')\n"
        "  print('set_color(name, r, g, b)')\n"
        "  print('get_color(name) -> r, g, b')\n"
        "  print('set_visible(name, bool)')\n"
        "  print('list_objects() -> table')\n"
        "  print('get_object_count() -> int')\n"
        "  print('')\n"
        "  print('--- Physics ---')\n"
        "  print('set_velocity(name, vx, vy, vz)')\n"
        "  print('get_velocity(name) -> vx, vy, vz')\n"
        "  print('add_force(name, fx, fy, fz)')\n"
        "  print('set_physics(name, bool)')\n"
        "  print('set_static(name, bool)')\n"
        "  print('set_gravity(name, bool)')\n"
        "  print('set_mass(name, mass)')\n"
        "  print('set_restitution(name, value)')\n"
        "  print('raycast(ox,oy,oz, dx,dy,dz) -> hit,name,x,y,z,nx,ny,nz')\n"
        "  print('check_collision(name1, name2) -> bool')\n"
        "  print('')\n"
        "  print('--- Animation ---')\n"
        "  print('Define: function animate(t, dt)')\n"
        "  print('  t  = time since play started')\n"
        "  print('  dt = delta time this frame')\n"
        "end\n";
    luaL_dostring(L, help_code);
}

void script_shutdown(ScriptState *ss) {
    if (ss->L) {
        lua_close((lua_State *)ss->L);
        ss->L = NULL;
    }
}

bool script_load_file(ScriptState *ss, const char *path) {
    lua_State *L = (lua_State *)ss->L;
    if (luaL_loadfile(L, path) != LUA_OK) {
        console_log(&ss->console, "[error] %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    // execute the file (defines functions)
    lua_pushcfunction(L, lua_error_handler);
    lua_insert(L, -2);
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        console_log(&ss->console, "[error] %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1); // error handler
        return false;
    }
    lua_pop(L, 1); // error handler
    snprintf(ss->console.scriptPath, sizeof(ss->console.scriptPath), "%s", path);
    ss->console.scriptLoaded = true;
    console_log(&ss->console, "Loaded: %s", path);
    return true;
}

bool script_exec(ScriptState *ss, const char *code) {
    lua_State *L = (lua_State *)ss->L;
    lua_pushcfunction(L, lua_error_handler);
    int errIdx = lua_gettop(L);

    if (luaL_loadstring(L, code) != LUA_OK) {
        console_log(&ss->console, "[error] %s", lua_tostring(L, -1));
        lua_pop(L, 2); // error + handler
        return false;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, errIdx) != LUA_OK) {
        console_log(&ss->console, "[error] %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return false;
    }

    // print any return values
    int nresults = lua_gettop(L) - errIdx;
    if (nresults > 0) {
        char buf[CONSOLE_LINE_MAX] = {0};
        int pos = 0;
        for (int i = errIdx + 1; i <= lua_gettop(L); i++) {
            const char *s = luaL_tolstring(L, i, NULL);
            if (pos > 0) pos += snprintf(buf + pos, CONSOLE_LINE_MAX - pos, "\t");
            pos += snprintf(buf + pos, CONSOLE_LINE_MAX - pos, "%s", s);
            lua_pop(L, 1); // tolstring result
        }
        console_log(&ss->console, "%s", buf);
    }
    lua_settop(L, errIdx - 1); // clean stack
    return true;
}

void script_load_object_scripts(ScriptState *ss) {
    lua_State *L = (lua_State *)ss->L;

    // create or clear __obj_scripts table
    lua_newtable(L);
    lua_setglobal(L, "__obj_scripts");

    double maxDuration = 0.0;
    bool anyDuration = false;

    for (int i = 0; i < ss->scene->objectCount; i++) {
        SceneObject *obj = &ss->scene->objects[i];
        if (!obj->active || obj->scriptCount == 0) continue;

        // create a table to hold this object's animate functions
        lua_getglobal(L, "__obj_scripts");
        lua_newtable(L); // array of functions for this object

        int funcIdx = 0;
        for (int s = 0; s < obj->scriptCount; s++) {
            if (obj->scriptPaths[s][0] == '\0') continue;

            int top = lua_gettop(L);

            // load and execute the file
            int err = luaL_loadfile(L, obj->scriptPaths[s]);
            if (err != LUA_OK) {
                console_log(&ss->console, "[error] %s: %s", obj->name, lua_tostring(L, -1));
                lua_settop(L, top);
                continue;
            }

            lua_pushcfunction(L, lua_error_handler);
            lua_insert(L, -2);
            err = lua_pcall(L, 0, 0, -2);
            lua_remove(L, top + 1);
            if (err != LUA_OK) {
                console_log(&ss->console, "[error] %s: %s", obj->name, lua_tostring(L, -1));
                lua_settop(L, top);
                continue;
            }

            // capture duration if defined
            lua_getglobal(L, "duration");
            if (lua_isnumber(L, -1)) {
                double dur = lua_tonumber(L, -1);
                if (dur > maxDuration) maxDuration = dur;
                anyDuration = true;
                console_log(&ss->console, "  duration: %.1fs", dur);
            }
            lua_pop(L, 1);

            // clear global duration to avoid leaking to next script
            lua_pushnil(L);
            lua_setglobal(L, "duration");

            // capture animate function
            lua_getglobal(L, "animate");
            if (lua_isfunction(L, -1)) {
                funcIdx++;
                lua_rawseti(L, top, funcIdx); // store in the array table
                console_log(&ss->console, "Loaded script for '%s': %s", obj->name, obj->scriptPaths[s]);
            } else {
                lua_pop(L, 1);
                console_log(&ss->console, "[warn] %s: no animate() defined", obj->scriptPaths[s]);
            }

            // clear global animate to avoid collision with next file
            lua_pushnil(L);
            lua_setglobal(L, "animate");
        }

        // store array in __obj_scripts[obj.id]
        lua_rawseti(L, -2, (lua_Integer)obj->id);
        lua_pop(L, 1); // __obj_scripts
    }

    // set timeline end frame based on max script duration
    if (anyDuration) {
        int endFrame = (int)(maxDuration * ss->timeline->fps);
        if (endFrame < 1) endFrame = 1;
        ss->timeline->endFrame = endFrame;
        console_log(&ss->console, "Timeline set to %d frames (%.1fs)", endFrame, maxDuration);
    } else {
        // no duration defined — unlimited playback
        ss->timeline->endFrame = 999999;
    }
}

void script_update(ScriptState *ss, float dt) {
    if (!ss->playing) return;

    lua_State *L = (lua_State *)ss->L;
    double elapsed = GetTime() - ss->startTime;

    // set globals
    lua_pushnumber(L, elapsed);
    lua_setglobal(L, "time");
    lua_pushnumber(L, dt);
    lua_setglobal(L, "dt");

    // call global animate(t, dt) if it exists (console scripts)
    lua_getglobal(L, "animate");
    if (lua_isfunction(L, -1)) {
        lua_pushcfunction(L, lua_error_handler);
        lua_insert(L, -2);
        lua_pushnumber(L, elapsed);
        lua_pushnumber(L, dt);
        if (lua_pcall(L, 2, 0, -4) != LUA_OK) {
            console_log(&ss->console, "[error] %s", lua_tostring(L, -1));
            lua_pop(L, 1);
            ss->playing = false;
            console_log(&ss->console, "Animation stopped due to error");
        }
        lua_pop(L, 1); // error handler
    } else {
        lua_pop(L, 1);
    }

    if (!ss->playing) return; // global script may have stopped

    // call per-object scripts: set self={name, id}, then call each animate(t, dt)
    lua_getglobal(L, "__obj_scripts");
    if (lua_istable(L, -1)) {
        for (int i = 0; i < ss->scene->objectCount; i++) {
            SceneObject *obj = &ss->scene->objects[i];
            if (!obj->active || obj->scriptCount == 0) continue;

            lua_rawgeti(L, -1, (lua_Integer)obj->id);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

            // set self global: {name=..., id=...}
            lua_newtable(L);
            lua_pushstring(L, obj->name);
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, (lua_Integer)obj->id);
            lua_setfield(L, -2, "id");
            lua_setglobal(L, "self");

            // iterate all animate functions in the array
            int len = (int)lua_rawlen(L, -1);
            for (int f = 1; f <= len; f++) {
                lua_rawgeti(L, -1, f);
                if (lua_isfunction(L, -1)) {
                    lua_pushcfunction(L, lua_error_handler);
                    lua_insert(L, -2);

                    lua_pushnumber(L, elapsed);
                    lua_pushnumber(L, dt);

                    if (lua_pcall(L, 2, 0, -4) != LUA_OK) {
                        console_log(&ss->console, "[error] %s: %s", obj->name, lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1); // error handler
                } else {
                    lua_pop(L, 1);
                }
            }

            lua_pop(L, 1); // func array table
        }
    }
    lua_pop(L, 1); // __obj_scripts
}

void script_play(ScriptState *ss) {
    ss->playing = true;
    ss->startTime = GetTime();
    console_log(&ss->console, "Animation playing");
}

void script_stop(ScriptState *ss) {
    ss->playing = false;
    console_log(&ss->console, "Animation stopped");
}
