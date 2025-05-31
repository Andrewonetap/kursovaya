#ifndef LEVEL4_ANALYSIS_RL_H
#define LEVEL4_ANALYSIS_RL_H

#include "app_data_rl.h" // Для AppData_Raylib и других общих определений
#include "raylib.h"      // Для Rectangle и других типов Raylib (если нужны в прототипах)

#include "level1_analysis_rl.h"

// Главная функция входа для отрисовки Уровня 4
void DrawLevel4View_Entry(AppData_Raylib *data);

void DrawMultiSeriesLineGraph_L4(
    AppData_Raylib *data,
    Rectangle plot_area_bounds, // Общие границы для всего компонента графика
    GraphSeriesData_L1 *series_array, // Используем ту же структуру для данных серий
    int num_series,
    GraphDisplayState *graph_state, // Указатель на data->l4_main_graph_state
    const char* x_axis_label_text,
    const char* y_axis_label_text
);

// Если для Уровня 4 будут свои функции сброса данных, их прототипы тоже здесь:
void ResetLevel4Data(AppData_Raylib *data);

#endif // LEVEL4_ANALYSIS_RL_H