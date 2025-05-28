// level1_analysis_rl.h
#ifndef LEVEL1_ANALYSIS_RL_H
#define LEVEL1_ANALYSIS_RL_H

#include "app_data_rl.h" // Для AppData_Raylib
#include "raylib.h"      // Для Rectangle и других типов Raylib

// Главная функция входа для отрисовки Уровня 1
void DrawLevel1View_Entry(AppData_Raylib *data);

typedef struct {
    double *x_values;       // X-координаты (может быть NULL, если используются индексы 0,1,2...)
    double *y_values;       // Y-координаты
    int num_points;         // Количество точек в этой серии
    Color color;            // Цвет линии
    const char *name;       // Имя для легенды (может быть NULL)
} GraphSeriesData_L1;

typedef struct {
    int cycle_epoch;    // Значение для колонки "Цикл": 0 для t0, 1 для t1, ..., N-1 для t(N-1), -1 для "прогноз"
    double mz_plus_e;   // Значение для колонки "Mz+E"
    double mz;          // Значение для колонки "Mz"
    double mz_minus_e;  // Значение для колонки "Mz-E"
    double r_value;     // Значение для колонки "R"
    double l_value;     // Значение для колонки "L"
    char status;        // Значение для колонки "статус": '+', '-', или '0'
} FinalAccuracyTableRow;

#endif // LEVEL1_ANALYSIS_RL_H