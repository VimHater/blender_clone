# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
cmake -B build && cmake --build build
# Binary: build/melder
# Run from build/ so font path "../font/" resolves correctly
cd build && ./melder
```

No tests or linter configured.

## Project Structure

- `src/main.cpp` — entry point (init, loop, shutdown)
- `src/editor.h/.cpp` — top-level Editor struct: owns all subsystems, undo/redo (circular buffer, max 64), clipboard (copy/cut/paste), play mode with snapshot/restore, save/load dispatch
- `src/core/` — engine modules (no UI dependency)
  - `types.h` — constants, enums, all core structs (SceneObject is a large union-style struct covering all object types including lights, cameras, models, physics, scripts, keyframes)
  - `scene.h/.cpp` — flat array scene graph (parentIndex = -1 for roots), multi-select by ID, gizmo hit-testing, texture/model loading
  - `camera.h/.cpp` — orbit/pan/zoom editor camera
  - `raycast.h/.cpp` — bounding-box raycasting for object picking
  - `timeline.h/.cpp` — keyframe animation with lerp interpolation, playback control
  - `lighting.h/.cpp` — shader-based lighting (Blinn-Phong, Toon, Unlit, Normal-vis, Fresnel), collects OBJ_LIGHT objects into GPU uniforms, shadow integration
  - `shadow.h/.cpp` — shadow map setup (requires GLSL shaders, currently no shader files in repo)
  - `physics.h/.cpp` — simple physics step (gravity, collision, restitution, friction)
  - `scripting.h/.cpp` — Lua 5.5 scripting: per-object scripts, global animate(), console REPL. lua_State exposed as void* to avoid header leak
  - `serializer.h/.cpp` — save/load in text (.scene) and binary (.sceneb) formats
- `src/builtin_objects/` — built-in mesh data (teapot)
- `src/ui/ui.h/.cpp` — all ImGui panels: dockspace, viewport (edit + animation tabs), hierarchy, properties, timeline, console, add-object, camera selector, gizmo interaction, placement mode
- `examples/` — sample .scene and .lua files, copied to build/examples/ at cmake time

## Vendored Dependencies

- `imgui/` — Dear ImGui source
- `rlImGui/` — raylib-ImGui binding
- `raylib-linux/`, `raylib-mingw/`, `raylib-msvc/` — prebuilt raylib per platform
- `lua-5.5.0/` — Lua interpreter (compiled as C static lib)
- `tinyfiledialogs/` — native file dialog library

## Conventions

- C++14, minimal C++ (structs, free functions, simple containers, no STL algorithms/smart pointers)
- raylib for rendering/input/window; Dear ImGui for all UI panels
- Viewport renders to RenderTexture, displayed inside ImGui window via rlImGui
- All new source/header files go in `src/` (core/ for engine, ui/ for panels)
- No external deps beyond what's vendored

## Architecture Notes

- **Scene graph**: flat `SceneObject objects[MAX_OBJECTS]` array. Hierarchy via `parentIndex`. Selection stored as ID array, not index array (IDs are stable across add/remove).
- **Object model**: single SceneObject struct holds all type-specific fields (cube dims, sphere params, light properties, camera FOV, model handle, physics state, keyframes, script paths). Type determined by `ObjectType` enum.
- **Play mode**: editor snapshots all object state before play, restores on stop. Physics and scripting only run during play.
- **UI ↔ Editor communication**: UI sets flags on EditorUI (wantSave, wantLoad, wantPlay, undoPending, wantCopy, etc.), editor reads and acts on them each frame. No callbacks.
- **Undo**: full scene copy pushed to circular buffer before mutations. Undo/redo swaps entire scene state.
- **Lighting pipeline**: LightingState collects OBJ_LIGHT scene objects into LightData array, uploads to shader uniforms. Multiple shader types share same uniform layout.
