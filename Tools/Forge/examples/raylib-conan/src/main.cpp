#include <raylib.h>

int main()
{
    InitWindow(800, 450, "Forge + raylib");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Forge coordinates. CMake and Conan stay visible.", 80, 210, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
