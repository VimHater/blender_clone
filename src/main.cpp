#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include <stdint.h>
#include <cstdint>
int main() {
    const int screenWidth = 1920;
    const int screenHeight = 1080;
    InitWindow(screenWidth, screenHeight, "Blender Clone");
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ImGui::GetIO().FontGlobalScale = 2.0f;
    Camera3D camera = { 0 };
    camera.position = Vector3{ 10.0f, 10.0f, 10.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // 3. Setup ImGui and Render Texture
    rlImGuiSetup(true);
    RenderTexture screenTarget = LoadRenderTexture(screenWidth, screenHeight);

    Color cubeColor = WHITE;
    float cubeSize = 2.0f;
    float sphereRadius = 2.0f;
    Color sphereColor = RED;

    while (!WindowShouldClose()) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            UpdateCamera(&camera, CAMERA_ORBITAL);
        }

        BeginTextureMode(screenTarget);
            ClearBackground(DARKGRAY);
            BeginMode3D(camera);
                DrawCube(Vector3{0,0,0}, cubeSize, cubeSize, cubeSize, cubeColor);
                DrawSphere(Vector3{0, 0, 3}, sphereRadius, sphereColor);                                     // Draw sphere
                DrawCubeWires(Vector3{0,0,0}, cubeSize, cubeSize, cubeSize, BLACK);
                DrawGrid(10, 1.0f);
            EndMode3D();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            rlImGuiBegin();

            // Menu Bar
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Exit")) break;
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            // Sidebar
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,10));
            ImGui::Begin("PropertiesCube");
                ImGui::Text("Object: Cube");
                ImGui::Separator();
                ImGui::SliderFloat("Scale", &cubeSize, 0.1f, 5.0f);

                float colCube[3] = { cubeColor.r/255.f, cubeColor.g/255.f, cubeColor.b/255.f };
                if (ImGui::ColorEdit3("Color", colCube)) {
                    cubeColor = Color{ (uint8_t)(colCube[0]*255), (uint8_t)(colCube[1]*255), (uint8_t)(colCube[2]*255), 255 };
                }

            ImGui::End();
            ImGui::PopStyleVar();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,10));
            ImGui::Begin("PropertiesSphere");
                ImGui::Text("Object: Sphere");
                ImGui::Separator();
                ImGui::SliderFloat("Scale", &sphereRadius, 0.1f, 5.0f);

                float colSphere[3] = { cubeColor.r/255.f, cubeColor.g/255.f, cubeColor.b/255.f };
                if (ImGui::ColorEdit3("Color", colSphere)) {
                    cubeColor = Color{ (uint8_t)(colSphere[0]*255), (uint8_t)(colSphere[1]*255), (uint8_t)(colSphere[2]*255), 255 };
                }
            ImGui::End();
            ImGui::PopStyleVar();

            // Viewport
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,10));
            ImGui::Begin("Viewport");
                // Calculate aspect ratio fit
                ImVec2 viewSize = ImGui::GetContentRegionAvail();
                // Draw the texture inside the ImGui window
                rlImGuiImageRenderTextureFit(&screenTarget, true);
            ImGui::End();
            ImGui::PopStyleVar();

            rlImGuiEnd();
        EndDrawing();
    }

    UnloadRenderTexture(screenTarget);
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
