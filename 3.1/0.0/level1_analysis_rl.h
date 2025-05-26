// level1_analysis_rl.h
#ifndef LEVEL1_ANALYSIS_RL_H
#define LEVEL1_ANALYSIS_RL_H

#include "app_data_rl.h" // Для AppData_Raylib
#include "raylib.h"      // Для Rectangle и других типов Raylib

// Главная функция входа для отрисовки Уровня 1
void DrawLevel1View_Entry(AppData_Raylib *data);

// (Опционально) Функции для инициализации/деинициализации ресурсов Уровня 1, если они нужны
// void InitLevel1Resources(AppData_Raylib *data);
// void ShutdownLevel1Resources(AppData_Raylib *data);

#endif // LEVEL1_ANALYSIS_RL_H