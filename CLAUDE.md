# Blender Clone

A 3D editor inspired by Blender, built with raylib and Dear ImGui.

## Build

```bash
cmake -B build && cmake --build build
# Binary: build/blender_clone
# Run from build/ so font path "../font/" resolves correctly
```

## Project Structure

- `src/` — all source and header files
  - `main.cpp` — entry point, main loop
  - `blender_clone.h` — core types and function declarations
- `imgui/` — Dear ImGui source (vendored)
- `rlImGui/` — raylib-ImGui binding (vendored)
- `raylib-linux/`, `raylib-mingw/`, `raylib-msvc/` — prebuilt raylib per platform
- `font/` — UI fonts

## Conventions

- C++ 14 standard, minimal C++ usage (prefer structs, free functions, simple containers)
- raylib for rendering, input, window management
- Dear ImGui for all UI panels
- Viewport renders to a RenderTexture, displayed inside an ImGui window via rlImGui
- All new source/header files go in `src/`
- No external dependencies beyond raylib, imgui, rlImGui

## Architecture

UI panels: Viewport, Timeline, Scene Hierarchy, Properties Editor
Core systems: Scene graph, Camera, Raycasting, Animation/Keyframes, Texture mapping

Objects are stored in a flat array with parent indices forming the hierarchy tree.
Each object has a transform (position, rotation, scale), material, and optional mesh/model.
