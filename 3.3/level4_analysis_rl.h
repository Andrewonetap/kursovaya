#ifndef LEVEL4_ANALYSIS_RL_H
#define LEVEL4_ANALYSIS_RL_H

#include "app_data_rl.h" // Для AppData_Raylib и других общих определений
#include "raylib.h"      // Для Rectangle и других типов Raylib (если нужны в прототипах)

// Главная функция входа для отрисовки Уровня 4
void DrawLevel4View_Entry(AppData_Raylib *data);

// Если для Уровня 4 понадобятся свои структуры данных или enum,
// их можно будет объявить здесь.
// Например:
// typedef enum {
//     L4_TAB_OVERVIEW,
//     L4_TAB_DETAILS
// } Level4ActiveTab;
//
// typedef struct {
//     // ... какие-то данные, специфичные для Уровня 4 ...
// } Level4SpecificData;


// Если для Уровня 4 будут свои функции сброса данных, их прототипы тоже здесь:
void ResetLevel4Data(AppData_Raylib *data);

#endif // LEVEL4_ANALYSIS_RL_H