#include "blender_clone.h"
#include "rlImGui.h"
#include "imgui.h"
#include <cstdio>

int main() {
    const int screenWidth = 1920;
    const int screenHeight = 1080;
    InitWindow(screenWidth, screenHeight, "Blender Clone");
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 2.0f;

    char fontPath[512];
    snprintf(fontPath, sizeof(fontPath), "%s../font/JetBrainsMonoNerdFont-Medium.ttf", GetApplicationDirectory());
    io.Fonts->AddFontFromFileTTF(fontPath, 18.0f);


    Editor editor;
    editor_init(&editor, screenWidth, screenHeight);

    while (!WindowShouldClose()) {
        editor_update(&editor);
        editor_draw(&editor);
    }

    editor_shutdown(&editor);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
