#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#include <stddef.h> // Для NULL

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Scroll Panel Test (Inverted Logic)");
    SetTargetFPS(60);

    Vector2 scroll = {0, 0};
    Rectangle panel_bounds = {100, 50, 300, 400}; 
    Rectangle content_rect = {0, 0, 600, 800}; 
    Rectangle view_area = {0};

    Rectangle moving_rect = { 50, 50, 200, 150 };

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawRectangleLinesEx(panel_bounds, 1, DARKGRAY);
        GuiScrollPanel(panel_bounds, NULL, content_rect, &scroll, &view_area);

        BeginScissorMode((int)view_area.x, (int)view_area.y, (int)view_area.width, (int)view_area.height);

            // ГИПОТЕЗА: Рисуем, ПРИБАВЛЯЯ scroll.x и scroll.y к базовой позиции view_area
            // Это будет означать, что scroll.x/y от GuiScrollPanel - это то, насколько
            // верхний левый угол КОНТЕНТА смещен ОТНОСИТЕЛЬНО верхнего левого угла ПАНЕЛИ.
            // Если scroll.x положительный, контент смещен вправо.
            // Если scroll.y положительный, контент смещен вниз.
            // Это НЕ стандартное поведение для "scroll offset", но давай проверим.

            // Рисуем фон контента
            DrawRectangle(
                (int)(view_area.x + scroll.x), // Используем view_area.x как базу
                (int)(view_area.y + scroll.y), 
                (int)content_rect.width, 
                (int)content_rect.height, 
                LIGHTGRAY
            );

            // Рисуем тестовый прямоугольник
            DrawRectangle(
                (int)(view_area.x + moving_rect.x + scroll.x), 
                (int)(view_area.y + moving_rect.y + scroll.y), 
                (int)moving_rect.width, 
                (int)moving_rect.height, 
                RED
            );
            
            DrawText(TextFormat("Scroll: %.0f, %.0f", scroll.x, scroll.y), 
                     (int)(view_area.x + 10 + scroll.x), 
                     (int)(view_area.y + 10 + scroll.y), 20, BLACK);
            DrawText("Content Top-Left", 
                     (int)(view_area.x + 10 + scroll.x), 
                     (int)(view_area.y + 30 + scroll.y), 20, BLUE);
            DrawText("Content Bottom-Right", 
                     (int)(view_area.x + content_rect.width - 200 + scroll.x), 
                     (int)(view_area.y + content_rect.height - 30 + scroll.y), 20, GREEN);

        EndScissorMode();
        
        DrawText(TextFormat("ViewArea: x%.0f y%.0f w%.0f h%.0f", view_area.x, view_area.y, view_area.width, view_area.height), 10, 10, 10, DARKGRAY);
        DrawText(TextFormat("Scroll: x%.0f y%.0f (from panel)", scroll.x, scroll.y), 10, 25, 10, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}