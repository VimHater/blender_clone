g++ main.cpp ./rlImGui/*.cpp ./imgui/*.cpp -o ./bin/main.exe `
    -I./imgui/ `
    -I./raylib-mingw/include/ `
    -L./raylib-mingw/lib/ `
    -lraylib -lopengl32 -lgdi32 -lwinmm
