// level1_analysis_rl.c
#include "level1_analysis_rl.h"
#include "app_data_rl.h"
#include "raylib.h"
#include "raygui.h"
#include "sqlite_utils_rl.h" 
#include "calculations_rl.h"

#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 
#include <math.h>    
#include <stdbool.h> 

// --- Прототипы внутренних статических функций для каждой вкладки ---
static void DrawLevel1Tab_InitialData(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataPlusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataMinusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_Conclusion(AppData_Raylib *data, Rectangle tab_content_bounds);

static TableData CreateL1PlusECombinedTable(AppData_Raylib *data, double value_E);
static TableData CreateL1MinusECombinedTable(AppData_Raylib *data, double value_E);

static bool EnsureLevel1BaseCalculations(AppData_Raylib *data);
static TableData CreateL1CombinedInitialTable(AppData_Raylib *data);
static bool EnsureLevel1SmoothingCalculations(AppData_Raylib *data);
static bool EnsureLevel1PlusECalculations(AppData_Raylib *data);
static bool EnsureLevel1MinusECalculations(AppData_Raylib *data);
// void ResetLevel1Data(AppData_Raylib *data);

static FinalAccuracyTableRow* PrepareL1FinalAccuracyTableData(AppData_Raylib *data, int *out_row_count);


bool HIDE_ROWID_COLUMN_FOR_VALUES_TABLE = true;
bool HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE = true;


static TableData CreateL1SmoothingTable(double **smoothing_results, int num_smooth_rows,
                                        const double* coeffs, int num_coeffs,
                                        double *original_data, int original_data_count,
                                        const char* original_data_col_name,
                                        int original_data_offset_for_smoothing);

void DrawMultiSeriesLineGraph_L1(
    AppData_Raylib *data,
    Rectangle plot_bounds,
    GraphSeriesData_L1 *series_array,
    int num_series,
    GraphDisplayState *graph_state,
    const char* x_axis_label_text,
    const char* y_axis_label_text
);

static void DrawL1FinalAccuracyTable(AppData_Raylib *data, 
                                     FinalAccuracyTableRow *accuracy_table_data, 
                                     int row_count, 
                                     Rectangle panel_bounds, 
                                     Vector2 *scroll, 
                                     int *selectedRow);



static const double SMOOTHING_COEFFS[] = {0.1, 0.4, 0.7, 0.9};
#define NUM_SMOOTHING_COEFFS (sizeof(SMOOTHING_COEFFS) / sizeof(SMOOTHING_COEFFS[0]))

static TableData CreateL1CombinedInitialTable(AppData_Raylib *data) {
    TableData combined_table = {0}; 

    if (!data || !data->table_data_main.table || data->table_data_main.row_count == 0 ||
        !data->mu_data || data->mu_data_count != data->table_data_main.row_count ||
        !data->alpha_data || data->alpha_count != data->table_data_main.row_count) {
        TraceLog(LOG_WARNING, "L1_COMB_TABLE: Недостаточно данных для создания объединенной таблицы.");
        return combined_table; 
    }

    int base_data_cols = data->table_data_main.column_count; 
    int total_data_cols_combined = base_data_cols + 2;       
    int total_pragma_cols_combined = total_data_cols_combined + 1; 

    combined_table.row_count = data->table_data_main.row_count;
    combined_table.column_count = total_data_cols_combined; 
    
    combined_table.num_allocated_column_names = total_pragma_cols_combined;
    combined_table.column_names = (char**)calloc(combined_table.num_allocated_column_names, sizeof(char*)); 
    if (!combined_table.column_names) {
        TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка calloc для column_names.");
        return combined_table; 
    }

    if (data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names[0]) {
        combined_table.column_names[0] = strdup(data->table_data_main.column_names[0]); 
    } else {
        combined_table.column_names[0] = strdup("Эпоха");
    }
    if (!combined_table.column_names[0]) goto cleanup_colnames_combined;

    for (int i = 0; i < base_data_cols; ++i) {
        if ((i + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[i + 1]) {
            combined_table.column_names[i + 1] = strdup(data->table_data_main.column_names[i + 1]);
        } else {
            combined_table.column_names[i + 1] = strdup(TextFormat("X%d", i + 1)); 
        }
        if (!combined_table.column_names[i + 1]) goto cleanup_colnames_combined;
    }
    combined_table.column_names[base_data_cols + 1] = strdup("Mu"); //μ (Мю)
    if (!combined_table.column_names[base_data_cols + 1]) goto cleanup_colnames_combined;
    combined_table.column_names[base_data_cols + 2] = strdup("Alpha"); //α (Альфа)
    if (!combined_table.column_names[base_data_cols + 2]) goto cleanup_colnames_combined;

    combined_table.epochs = (int*)calloc(combined_table.row_count, sizeof(int));
    if (!combined_table.epochs) {
        TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка calloc для epochs.");
        goto cleanup_colnames_combined; 
    }
    for (int i = 0; i < combined_table.row_count; ++i) {
        combined_table.epochs[i] = data->table_data_main.epochs[i];
    }

    combined_table.table = (double**)calloc(combined_table.row_count, sizeof(double*));
    if (!combined_table.table) {
        TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка calloc для table.");
        goto cleanup_epochs_combined; 
    }
    for (int i = 0; i < combined_table.row_count; ++i) {
        combined_table.table[i] = (double*)calloc(combined_table.column_count, sizeof(double));
        if (!combined_table.table[i]) {
            TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка calloc для table[%d].", i);
            for (int k = 0; k < i; ++k) if(combined_table.table[k]) free(combined_table.table[k]); 
            free(combined_table.table); combined_table.table = NULL;
            goto cleanup_epochs_combined;
        }
        for (int j = 0; j < base_data_cols; ++j) {
            if (data->table_data_main.table[i]) { 
                 combined_table.table[i][j] = data->table_data_main.table[i][j];
            } else {
                 combined_table.table[i][j] = NAN; 
            }
        }
        combined_table.table[i][base_data_cols] = data->mu_data[i];
        combined_table.table[i][base_data_cols + 1] = data->alpha_data[i];
    }
    TraceLog(LOG_INFO, "L1_COMB_TABLE: Временная объединенная таблица создана.");
    return combined_table;

cleanup_epochs_combined:
    if (combined_table.epochs) free(combined_table.epochs);
    combined_table.epochs = NULL;
cleanup_colnames_combined:
    if (combined_table.column_names) {
        for (int i = 0; i < combined_table.num_allocated_column_names; ++i) {
            if (combined_table.column_names[i]) free(combined_table.column_names[i]);
        }
        free(combined_table.column_names);
        combined_table.column_names = NULL;
    }
    combined_table.row_count = 0;
    combined_table.column_count = 0;
    combined_table.num_allocated_column_names = 0;
    TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка при создании временной таблицы, очистка.");
    return combined_table;
}


static void DrawSimpleLineSeries(Rectangle bounds, double *y_values, int num_points, Color color,
                                 double y_min_overall, double y_max_overall) {
    if (!y_values || num_points < 2) return;

    float point_x_step = bounds.width / (float)(num_points - 1);
    
    Vector2 prev_point_screen = {0};

    for (int i = 0; i < num_points; ++i) {
        float screen_x = bounds.x + i * point_x_step;
        
        // Масштабирование Y
        double y_val = y_values[i];
        float screen_y = bounds.y + bounds.height; // Низ графика
        if (y_max_overall > y_min_overall) { // Защита от деления на ноль
            screen_y = bounds.y + bounds.height - ((y_val - y_min_overall) / (y_max_overall - y_min_overall)) * bounds.height;
        } else if (y_val == y_min_overall) { // Если все значения одинаковы
             screen_y = bounds.y + bounds.height / 2.0f;
        }


        if (i > 0) {
            DrawLineEx(prev_point_screen, (Vector2){screen_x, screen_y}, 1.5f, color);
        }
        prev_point_screen = (Vector2){screen_x, screen_y};
    }
}

// --- Вспомогательная функция для вычисления данных сглаживания ---
static bool EnsureLevel1SmoothingCalculations(AppData_Raylib *data) {
    if (!data) return false;

    // 1. ОСВОБОЖДАЕМ СТАРЫЕ ДАННЫЕ СГЛАЖИВАНИЯ, ЕСЛИ ОНИ ЕСТЬ
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; ++i) {
            if (data->smoothing_data[i]) {
                free(data->smoothing_data[i]);
                data->smoothing_data[i] = NULL;
            }
        }
        free(data->smoothing_data);
        data->smoothing_data = NULL; 
        data->smoothing_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_SMOOTH_CALC: Освобождены старые smoothing_data (в начале функции).");
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; ++i) {
            if (data->smoothing_alpha_data[i]) {
                free(data->smoothing_alpha_data[i]);
                data->smoothing_alpha_data[i] = NULL;
            }
        }
        free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL; 
        data->smoothing_alpha_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_SMOOTH_CALC: Освобождены старые smoothing_alpha_data (в начале функции).");
    }

    // 2. ПРОВЕРЯЕМ НАЛИЧИЕ БАЗОВЫХ ДАННЫХ ДЛЯ НОВЫХ ВЫЧИСЛЕНИЙ
    if (!data->mu_data || data->mu_data_count == 0) {
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Нет mu_data для сглаживания. Новые данные сглаживания не будут вычислены.");
        // Старые уже очищены выше, так что просто выходим
        return false; 
    }
    if (!data->alpha_data || data->alpha_count == 0) { // alpha_count == 0 означает, что и alpha_count > 1 не выполнится
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Нет alpha_data для сглаживания. Новые данные сглаживания alpha не будут вычислены.");
    }

    // 3. ВЫЧИСЛЕНИЕ НОВЫХ ДАННЫХ СГЛАЖИВАНИЯ

    // Сглаживание для mu_data
    TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Вычисление сглаживания для μ (данных: %d)", data->mu_data_count);
    data->smoothing_data = compute_smoothing_rl(data->mu_data, data->mu_data_count,
                                                &data->smoothing_row_count,
                                                (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
    if (!data->smoothing_data) {
        TraceLog(LOG_ERROR, "L1_SMOOTH_CALC: Не удалось вычислить сглаживание для μ.");
        return false; 
    }
    TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Сглаживание μ завершено (строк результата: %d).", data->smoothing_row_count);

    // Сглаживание для alpha_data
    if (data->alpha_data && data->alpha_count > 1) {
        TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Вычисление сглаживания для α (данных: %d, начиная с индекса 1)", data->alpha_count - 1);
        data->smoothing_alpha_data = compute_smoothing_rl(&data->alpha_data[1], data->alpha_count - 1,
                                                          &data->smoothing_alpha_row_count,
                                                          (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_alpha_data) {
            TraceLog(LOG_ERROR, "L1_SMOOTH_CALC: Не удалось вычислить сглаживание для α.");
            return false; 
        }
        TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Сглаживание α завершено (строк результата: %d).", data->smoothing_alpha_row_count);
    } else {
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Недостаточно данных α для сглаживания (alpha_data NULL, или alpha_count <= 1).");
    }
    return true;
}

// --- Вспомогательная функция для создания временной таблицы сглаживания ---
static TableData CreateL1SmoothingTable(double **smoothing_results, int num_smooth_rows,
                                        const double* coeffs, int num_coeffs,
                                        double *original_data, int original_data_count,
                                        const char* original_data_col_name,
                                        int original_data_offset_for_smoothing) {
    TableData smooth_table_display = {0};

    if (!smoothing_results || num_smooth_rows == 0) {
        TraceLog(LOG_WARNING, "L1_SMOOTH_TABLE: Нет данных сглаживания для отображения.");
        return smooth_table_display;
    }

    smooth_table_display.row_count = num_smooth_rows;
    // Колонки данных: num_coeffs для сглаженных значений + 1 для оригинального значения
    smooth_table_display.column_count = num_coeffs + 1;
    // Всего колонок с учетом "M (период)"
    smooth_table_display.num_allocated_column_names = smooth_table_display.column_count + 1;

    // 1. Имена колонок
    smooth_table_display.column_names = (char**)calloc(smooth_table_display.num_allocated_column_names, sizeof(char*));
    if (!smooth_table_display.column_names) goto cleanup_smooth_table;

    smooth_table_display.column_names[0] = strdup("M");
    if (!smooth_table_display.column_names[0]) goto cleanup_smooth_table;

    for (int i = 0; i < num_coeffs; ++i) {
        smooth_table_display.column_names[i + 1] = strdup(TextFormat("A=%.1f", coeffs[i]));
        if (!smooth_table_display.column_names[i + 1]) goto cleanup_smooth_table;
    }
    smooth_table_display.column_names[num_coeffs + 1] = strdup(original_data_col_name);
    if (!smooth_table_display.column_names[num_coeffs + 1]) goto cleanup_smooth_table;

    // 2. Эпохи (здесь это M-периоды)
    smooth_table_display.epochs = (int*)calloc(smooth_table_display.row_count, sizeof(int));
    if (!smooth_table_display.epochs) goto cleanup_smooth_table;
    for (int i = 0; i < smooth_table_display.row_count; ++i) {
        smooth_table_display.epochs[i] = i;
    }

    // 3. Данные таблицы
    smooth_table_display.table = (double**)calloc(smooth_table_display.row_count, sizeof(double*));
    if (!smooth_table_display.table) goto cleanup_smooth_table;

    for (int i = 0; i < smooth_table_display.row_count; ++i) {
        smooth_table_display.table[i] = (double*)calloc(smooth_table_display.column_count, sizeof(double));
        if (!smooth_table_display.table[i]) {
            for (int k = 0; k < i; ++k) free(smooth_table_display.table[k]);
            free(smooth_table_display.table); smooth_table_display.table = NULL;
            goto cleanup_smooth_table;
        }

        // Заполняем сглаженными значениями
        if (smoothing_results[i]) { // Проверка, что строка сглаженных данных существует
            for (int j = 0; j < num_coeffs; ++j) {
                smooth_table_display.table[i][j] = smoothing_results[i][j];
            }
        } else {
             for (int j = 0; j < num_coeffs; ++j) smooth_table_display.table[i][j] = NAN;
        }


        // Заполняем оригинальным значением
        // i - это M-период.
        // num_smooth_rows = (количество_исходных_точек_для_сглаживания) + 1 (для прогноза)
        // Реальные данные есть для M от 0 до (количество_исходных_точек_для_сглаживания - 1)
        int num_original_points_for_smoothing = num_smooth_rows - 1;
        if (i < num_original_points_for_smoothing) { // Если это не прогнозная строка
            int original_data_idx = i + original_data_offset_for_smoothing;
            if (original_data && original_data_idx < original_data_count) {
                smooth_table_display.table[i][num_coeffs] = original_data[original_data_idx];
            } else {
                smooth_table_display.table[i][num_coeffs] = NAN; // Оригинальных данных нет/не совпал индекс
            }
        } else { // Это прогнозная строка
            smooth_table_display.table[i][num_coeffs] = NAN; // Для прогнозной строки нет "реального" значения в этой точке
                                                             // В write_results_to_file_rl вы писали "(прогноз)"
                                                             // Здесь для числовой таблицы ставим NAN
        }
    }
    return smooth_table_display;

cleanup_smooth_table:
    // Используем существующую функцию для очистки, если она подходит
    // или реализуем очистку здесь. free_table_data_content_rl ожидает TableData*
    // поэтому сделаем ручную очистку для локальной переменной.
    if (smooth_table_display.column_names) {
        for (int i = 0; i < smooth_table_display.num_allocated_column_names; ++i) {
            if (smooth_table_display.column_names[i]) free(smooth_table_display.column_names[i]);
        }
        free(smooth_table_display.column_names);
    }
    if (smooth_table_display.epochs) free(smooth_table_display.epochs);
    if (smooth_table_display.table) {
        for (int i = 0; i < smooth_table_display.row_count; ++i) {
            if (smooth_table_display.table[i]) free(smooth_table_display.table[i]);
        }
        free(smooth_table_display.table);
    }
    memset(&smooth_table_display, 0, sizeof(TableData)); // Обнуляем всю структуру
    TraceLog(LOG_ERROR, "L1_SMOOTH_TABLE: Ошибка при создании таблицы сглаживания, очистка.");
    return smooth_table_display;
}

void ResetLevel1Data(AppData_Raylib *data) {
    if (!data) return;
    TraceLog(LOG_INFO, "L1_RESET: Сброс всех вычисленных данных Уровня 1.");

    // 1. Освобождаем базовые расчеты (mu_data, alpha_data)
    if (data->mu_data) {
        free(data->mu_data);
        data->mu_data = NULL;
        data->mu_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены mu_data.");
    }
    if (data->alpha_data) {
        free(data->alpha_data);
        data->alpha_data = NULL;
        data->alpha_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены alpha_data.");
    }

    // 2. Освобождаем сглаживания для базовых mu/alpha
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; ++i) {
            if (data->smoothing_data[i]) free(data->smoothing_data[i]);
        }
        free(data->smoothing_data);
        data->smoothing_data = NULL;
        data->smoothing_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_data (для mu).");
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; ++i) {
            if (data->smoothing_alpha_data[i]) free(data->smoothing_alpha_data[i]);
        }
        free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL;
        data->smoothing_alpha_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_alpha_data.");
    }

    // 3. Освобождаем данные +E (mu_prime, alpha_prime) и их сглаживания
    if (data->mu_prime_data) {
        free(data->mu_prime_data);
        data->mu_prime_data = NULL;
        data->mu_prime_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены mu_prime_data.");
    }
    if (data->alpha_prime_data) {
        free(data->alpha_prime_data);
        data->alpha_prime_data = NULL;
        data->alpha_prime_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены alpha_prime_data.");
    }
    if (data->smoothing_mu_prime_data) {
        for (int i = 0; i < data->smoothing_mu_prime_row_count; ++i) {
            if (data->smoothing_mu_prime_data[i]) free(data->smoothing_mu_prime_data[i]);
        }
        free(data->smoothing_mu_prime_data);
        data->smoothing_mu_prime_data = NULL;
        data->smoothing_mu_prime_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_mu_prime_data.");
    }
    if (data->smoothing_alpha_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_prime_row_count; ++i) {
            if (data->smoothing_alpha_prime_data[i]) free(data->smoothing_alpha_prime_data[i]);
        }
        free(data->smoothing_alpha_prime_data);
        data->smoothing_alpha_prime_data = NULL;
        data->smoothing_alpha_prime_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_alpha_prime_data.");
    }

    // 4. Освобождаем данные -E (mu_double_prime, alpha_double_prime) и их сглаживания
    if (data->mu_double_prime_data) {
        free(data->mu_double_prime_data);
        data->mu_double_prime_data = NULL;
        data->mu_double_prime_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены mu_double_prime_data.");
    }
    if (data->alpha_double_prime_data) {
        free(data->alpha_double_prime_data);
        data->alpha_double_prime_data = NULL;
        data->alpha_double_prime_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены alpha_double_prime_data.");
    }
    if (data->smoothing_mu_double_prime_data) {
        for (int i = 0; i < data->smoothing_mu_double_prime_row_count; ++i) {
            if (data->smoothing_mu_double_prime_data[i]) free(data->smoothing_mu_double_prime_data[i]);
        }
        free(data->smoothing_mu_double_prime_data);
        data->smoothing_mu_double_prime_data = NULL;
        data->smoothing_mu_double_prime_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_mu_double_prime_data.");
    }
    if (data->smoothing_alpha_double_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; ++i) {
            if (data->smoothing_alpha_double_prime_data[i]) free(data->smoothing_alpha_double_prime_data[i]);
        }
        free(data->smoothing_alpha_double_prime_data);
        data->smoothing_alpha_double_prime_data = NULL;
        data->smoothing_alpha_double_prime_row_count = 0;
        TraceLog(LOG_DEBUG, "L1_RESET: Освобождены smoothing_alpha_double_prime_data.");
    }

    // 6. Сброс состояния графиков Уровня 1
    // (Предполагается, что у вас есть enum GraphIdentifier и массив graph_states)
    data->graph_states[GRAPH_ID_L1_MU].is_active = false;
    data->graph_states[GRAPH_ID_L1_ALPHA].is_active = false;
    data->graph_states[GRAPH_ID_L1_MU_PRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_ALPHA_PRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_MU_DPRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_ALPHA_DPRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_CONCLUSION_ALPHA_MU].is_active = false;
    // Сброс offset и scale тоже может быть полезен, чтобы при следующем входе график пересчитался полностью
    for (int i = 0; i < GRAPH_ID_COUNT; i++) { // Или только для графиков Уровня 1
        if (i >= GRAPH_ID_L1_MU && i <= GRAPH_ID_L1_CONCLUSION_ALPHA_MU) { // Пример диапазона для L1 графиков
            data->graph_states[i].offset = (Vector2){0.0f, 0.0f};
            data->graph_states[i].scale = (Vector2){0.0f, 0.0f}; // 0.0f для scale заставит его пересчитаться
        }
    }
    TraceLog(LOG_DEBUG, "L1_RESET: Состояния графиков Уровня 1 сброшены.");
}

// --- Вспомогательная функция для вычисления основных mu и alpha ---
static bool EnsureLevel1BaseCalculations(AppData_Raylib *data) {
    if (!data) {
        TraceLog(LOG_ERROR, "L1_CALC: AppData is NULL in EnsureLevel1BaseCalculations.");
        return false;
    }

    // Если нет исходных данных (table_data_main), то все вычисленные данные Уровня 1 должны быть очищены.
    if (!data->db_opened || data->table_data_main.row_count == 0) {
        TraceLog(LOG_WARNING, "L1_CALC: Нет данных в table_data_main для вычислений Уровня 1. Запускается полная очистка данных Уровня 1.");
        ResetLevel1Data(data); // Используем новую функцию для полной очистки
        return false; // Нечего вычислять
    }

    // Если мы дошли сюда, значит table_data_main существует.
    // Теперь выполняем каскадную очистку всех производных данных,
    // так как мы собираемся пересчитать базовые mu_data и alpha_data.
    // Это гарантирует, что не останется "старых" производных данных с новыми базовыми.
    
    TraceLog(LOG_DEBUG, "L1_CALC: Начало каскадной очистки производных данных перед пересчетом базовых Mu/Alpha.");

    // 1. Освобождаем старые mu_data и alpha_data (они будут пересчитаны)
    if (data->mu_data) {
        free(data->mu_data);
        data->mu_data = NULL;
        data->mu_data_count = 0;
    }
    if (data->alpha_data) {
        free(data->alpha_data);
        data->alpha_data = NULL;
        data->alpha_count = 0;
    }

    // 2. Освобождаем сглаживания для базовых mu/alpha, так как базовые будут новыми
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; ++i) {
            if (data->smoothing_data[i]) free(data->smoothing_data[i]);
        }
        free(data->smoothing_data);
        data->smoothing_data = NULL;
        data->smoothing_row_count = 0;
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; ++i) {
            if (data->smoothing_alpha_data[i]) free(data->smoothing_alpha_data[i]);
        }
        free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL;
        data->smoothing_alpha_row_count = 0;
    }

    // 3. Освобождаем данные +E (mu_prime, alpha_prime) и их сглаживания,
    //    так как они зависят от table_data_main, которая могла измениться.
    if (data->mu_prime_data) {
        free(data->mu_prime_data);
        data->mu_prime_data = NULL;
        data->mu_prime_data_count = 0;
    }
    if (data->alpha_prime_data) {
        free(data->alpha_prime_data);
        data->alpha_prime_data = NULL;
        data->alpha_prime_data_count = 0;
    }
    if (data->smoothing_mu_prime_data) {
        for (int i = 0; i < data->smoothing_mu_prime_row_count; ++i) {
            if (data->smoothing_mu_prime_data[i]) free(data->smoothing_mu_prime_data[i]);
        }
        free(data->smoothing_mu_prime_data);
        data->smoothing_mu_prime_data = NULL;
        data->smoothing_mu_prime_row_count = 0;
    }
    if (data->smoothing_alpha_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_prime_row_count; ++i) {
            if (data->smoothing_alpha_prime_data[i]) free(data->smoothing_alpha_prime_data[i]);
        }
        free(data->smoothing_alpha_prime_data);
        data->smoothing_alpha_prime_data = NULL;
        data->smoothing_alpha_prime_row_count = 0;
    }

    // 4. Освобождаем данные -E (mu_double_prime, alpha_double_prime) и их сглаживания
    if (data->mu_double_prime_data) {
        free(data->mu_double_prime_data);
        data->mu_double_prime_data = NULL;
        data->mu_double_prime_data_count = 0;
    }
    if (data->alpha_double_prime_data) {
        free(data->alpha_double_prime_data);
        data->alpha_double_prime_data = NULL;
        data->alpha_double_prime_data_count = 0;
    }
    if (data->smoothing_mu_double_prime_data) {
        for (int i = 0; i < data->smoothing_mu_double_prime_row_count; ++i) {
            if (data->smoothing_mu_double_prime_data[i]) free(data->smoothing_mu_double_prime_data[i]);
        }
        free(data->smoothing_mu_double_prime_data);
        data->smoothing_mu_double_prime_data = NULL;
        data->smoothing_mu_double_prime_row_count = 0;
    }
    if (data->smoothing_alpha_double_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; ++i) {
            if (data->smoothing_alpha_double_prime_data[i]) free(data->smoothing_alpha_double_prime_data[i]);
        }
        free(data->smoothing_alpha_double_prime_data);
        data->smoothing_alpha_double_prime_data = NULL;
        data->smoothing_alpha_double_prime_row_count = 0;
    }


    TraceLog(LOG_DEBUG, "L1_CALC: Каскадная очистка производных данных завершена.");

    // Теперь вычисляем новые mu_data и alpha_data
    TraceLog(LOG_INFO, "L1_CALC: Вычисление μ и α для table_data_main (строк: %d, колонок данных: %d)",
             data->table_data_main.row_count, data->table_data_main.column_count);

    data->mu_data = compute_mu_rl(data->table_data_main.table,
                                  data->table_data_main.row_count,
                                  data->table_data_main.column_count);
    if (!data->mu_data) {
        TraceLog(LOG_ERROR, "L1_CALC: Не удалось вычислить μ.");
        // Важно: после ошибки здесь, все данные Уровня 1 должны быть в очищенном состоянии.
        // ResetLevel1Data() уже была вызвана выше, если table_data_main была пуста.
        // Если table_data_main есть, но compute_mu_rl не удалось, то mu_data останется NULL.
        // Остальные производные данные уже очищены.
        return false;
    }
    data->mu_data_count = data->table_data_main.row_count;
    TraceLog(LOG_INFO, "L1_CALC: μ вычислены (%d значений).", data->mu_data_count);

    data->alpha_data = compute_alpha_rl(data->table_data_main.table,
                                    data->table_data_main.row_count,
                                    data->table_data_main.column_count,
                                    data->mu_data,
                                    data->current_main_table_name);
    if (!data->alpha_data) {
        TraceLog(LOG_ERROR, "L1_CALC: Не удалось вычислить α.");
        // Если alpha не удалось, mu_data уже вычислены. Их нужно освободить,
        // так как состояние "только mu, без alpha" может быть невалидным для других частей.
        free(data->mu_data);
        data->mu_data = NULL;
        data->mu_data_count = 0;
        // Остальные производные данные уже очищены.
        return false;
    }
    data->alpha_count = data->table_data_main.row_count;
    TraceLog(LOG_INFO, "L1_CALC: α вычислены (%d значений).", data->alpha_count);

    // После успешного вычисления базовых mu и alpha,
    // состояния связанных графиков нужно сбросить, чтобы они пересчитали свои диапазоны.
    data->graph_states[GRAPH_ID_L1_MU].is_active = false;
    data->graph_states[GRAPH_ID_L1_ALPHA].is_active = false;
    data->graph_states[GRAPH_ID_L1_MU_PRIME].is_active = false; // Так как mu_prime_data были очищены
    data->graph_states[GRAPH_ID_L1_ALPHA_PRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_MU_DPRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_ALPHA_DPRIME].is_active = false;
    data->graph_states[GRAPH_ID_L1_CONCLUSION_ALPHA_MU].is_active = false; // График заключения тоже зависит от них
    TraceLog(LOG_DEBUG, "L1_CALC: Состояния графиков Уровня 1 сброшены после пересчета базовых Mu/Alpha.");

    return true;
}

// --- Реализация главной функции Уровня 1 ---
void DrawLevel1View_Entry(AppData_Raylib *data) {
    if (!data) return;

    // Размеры и константы для UI Уровня 1
    const float padding = 10.0f;
    const float top_offset = TOP_NAV_BAR_HEIGHT + padding; // TOP_NAV_BAR_HEIGHT из main_rl.c, нужно либо передавать, либо сделать глобальной константой
    const float button_height = 30.0f;
    const float back_button_width = 100.0f;
    const float tab_panel_width = 180.0f; // Ширина панели с кнопками вкладок

    // 1. Кнопка "Назад"
    if (GuiButton((Rectangle){padding, top_offset, back_button_width, button_height}, "< Назад")) {
        ResetLevel1Data(data);
        data->current_view = APP_VIEW_MAIN;
        // Здесь можно добавить логику сброса состояния Уровня 1, если это необходимо
        // Например, сбросить data->l1_active_tab на значение по умолчанию,
        // или освободить память, специфичную только для Уровня 1.
        TraceLog(LOG_INFO, "L1: Возврат в Main View.");
        return;
    }

    // 2. Панель навигации по вкладкам (вертикальная слева)
    Rectangle tab_panel_bounds = {
        padding,
        top_offset + button_height + padding,
        tab_panel_width,
        (float)data->screen_height - (top_offset + button_height + padding) - padding
    };
    // GuiDrawRectangle(tab_panel_bounds, 0, GetColor(GuiGetStyle(DEFAULT, LINE_COLOR)), GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR))); // Опционально, для визуального выделения

    float current_tab_y = tab_panel_bounds.y + padding;
    const char* tab_names[] = {"Основные данные", "+E", "-E", "Заключение"};
    Level1ActiveTab tabs_enum_values[] = {L1_TAB_INITIAL_DATA, L1_TAB_DATA_PLUS_E, L1_TAB_DATA_MINUS_E, L1_TAB_CONCLUSION};
    int num_tabs = sizeof(tab_names) / sizeof(tab_names[0]);

    for (int i = 0; i < num_tabs; ++i) {
        Rectangle tab_button_rect = {tab_panel_bounds.x + padding, current_tab_y, tab_panel_bounds.width - 2 * padding, button_height};
        // Выделяем активную вкладку
        if (data->l1_active_tab == tabs_enum_values[i]) {
            // Можно изменить стиль для активной кнопки, например, GuiLock() и GuiSetState(STATE_PRESSED) перед GuiButton, потом GuiUnlock()
            // Или просто отрисовать рамку/фон по-другому
            // Для простоты пока оставим стандартный вид, но запомним, что это можно улучшить
        }
        if (GuiButton(tab_button_rect, tab_names[i])) {
            data->l1_active_tab = tabs_enum_values[i];
            TraceLog(LOG_INFO, "L1: Активирована вкладка: %s", tab_names[i]);
            // При смене вкладки можно сбрасывать прокрутку для таблиц/графиков этой вкладки, если нужно
            // data->l1_tables_scroll[data->l1_active_tab][0] = (Vector2){0,0}; // Пример
        }
        current_tab_y += button_height + padding / 2.0f;
    }

    // 3. Основная область для контента вкладки
    Rectangle content_area_bounds = {
        tab_panel_bounds.x + tab_panel_bounds.width + padding,
        top_offset, // Начинаем с того же уровня, что и кнопка "Назад" для максимального использования пространства
        (float)data->screen_width - (tab_panel_bounds.x + tab_panel_bounds.width + padding) - padding,
        (float)data->screen_height - top_offset - padding
    };
    // GuiDrawRectangle(content_area_bounds, 0, GetColor(GuiGetStyle(DEFAULT, LINE_COLOR)), WHITE); // Опционально, для отладки границ

    // 4. Логика отображения в зависимости от активной вкладки
    switch (data->l1_active_tab) {
        case L1_TAB_INITIAL_DATA:
            DrawLevel1Tab_InitialData(data, content_area_bounds);
            break;
        case L1_TAB_DATA_PLUS_E:
            DrawLevel1Tab_DataPlusE(data, content_area_bounds);
            break;
        case L1_TAB_DATA_MINUS_E:
            DrawLevel1Tab_DataMinusE(data, content_area_bounds);
            break;
        case L1_TAB_CONCLUSION:
            DrawLevel1Tab_Conclusion(data, content_area_bounds);
            break;
        default:
            DrawText("Неизвестная вкладка Уровня 1", (int)content_area_bounds.x + 20, (int)content_area_bounds.y + 20, 20, RED);
            break;
    }
}

// --- Реализации функций для каждой вкладки (пока заглушки) ---

static void DrawLevel1Tab_InitialData(AppData_Raylib *data, Rectangle tab_content_bounds) {
    if (!data) {
        DrawText("AppData is NULL!", (int)tab_content_bounds.x + 10, (int)tab_content_bounds.y + 10, 18, RED);
        return;
    }

    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 400, 20}, "Исходные данные, Mu, Alpha и их сглаживание");

    float current_y_offset = tab_content_bounds.y + 40; 
    float content_padding = 10.0f;

    if (!data->db_opened || data->table_data_main.row_count == 0) {
        DrawText("Нет исходных данных. Откройте файл БД.",
                 (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, DARKGRAY);
        return;
    }

    // 1. Убедимся, что базовые Mu и Alpha рассчитаны
    if (!data->mu_data || !data->alpha_data ||
        data->mu_data_count != data->table_data_main.row_count ||
        data->alpha_count != data->table_data_main.row_count) {
        TraceLog(LOG_DEBUG, "L1_InitialData: Mu/Alpha не актуальны или отсутствуют, вызываем EnsureLevel1BaseCalculations.");
        if (!EnsureLevel1BaseCalculations(data)) { // Эта функция должна очистить и все зависимые данные (сглаживания, prime, dprime)
            DrawText("Ошибка при вычислении Mu и Alpha. Проверьте логи.",
                     (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, RED);
            return;
        }
        if (!data->mu_data || !data->alpha_data) { // Повторная проверка после вычисления
             DrawText("Данные Mu и Alpha все еще не доступны после попытки вычисления.",
                     (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, ORANGE);
            return;
        }
    }

    // 2. Убедимся, что данные сглаживания для Mu и Alpha рассчитаны
    // EnsureLevel1SmoothingCalculations должна вызываться ПОСЛЕ EnsureLevel1BaseCalculations,
    // так как она зависит от mu_data и alpha_data.
    // Проверяем, нужно ли пересчитывать сглаживание
    bool needs_smoothing_recalc = false;
    if (!data->smoothing_data || (data->mu_data_count > 0 && data->smoothing_row_count != (data->mu_data_count + 1))) {
        needs_smoothing_recalc = true;
    }
    if (!data->smoothing_alpha_data || 
        (data->alpha_count > 1 && data->smoothing_alpha_row_count != (data->alpha_count - 1 + 1)) || // alpha_count-1 точек сглаживаются, +1 для прогноза
        (data->alpha_count <= 1 && data->smoothing_alpha_row_count != 0) ) { // Если alpha_count <=1, сглаживаемых точек 0, строк результата 0 (или 1, если прогноз для 0 точек дает 1 строку)
                                                                            // compute_smoothing_rl для 0 точек вернет 1 строку (прогноз)
                                                                            // Если alpha_count=1 (только alpha[0]=0), то alpha_count-1 = 0 точек. smoothing_alpha_row_count должен быть 1.
                                                                            // Если alpha_count=0, то alpha_count-1 = -1 (некорректно).
         if (data->alpha_count == 1 && data->smoothing_alpha_row_count != 1) needs_smoothing_recalc = true;
         else if (data->alpha_count < 1 && data->smoothing_alpha_row_count != 0) needs_smoothing_recalc = true; // Для 0 исходных точек, compute_smoothing_rl вернет 0 строк если нет коэф.
                                                                                                                // или 1 строку если есть коэф. (прогноз)
                                                                                                                // В EnsureLevel1SmoothingCalculations есть проверка data_count <=0
    }


    if (needs_smoothing_recalc) {
        TraceLog(LOG_DEBUG, "L1_InitialData: Данные сглаживания не актуальны или отсутствуют, вызываем EnsureLevel1SmoothingCalculations.");
        if (!EnsureLevel1SmoothingCalculations(data)) { // Эта функция должна очистить старые data->smoothing_data и data->smoothing_alpha_data
            TraceLog(LOG_WARNING, "L1_InitialData: Ошибка при вычислении данных сглаживания (может быть частичной).");
            // Не выходим, так как основные Mu/Alpha могут быть, а сглаживание просто не отобразится.
        }
    }

    // --- Отрисовка UI ---
    float available_width_content = tab_content_bounds.width - 2 * content_padding;
    float available_height_content = tab_content_bounds.height - (current_y_offset - tab_content_bounds.y) - content_padding;

    // 3. Комбинированная таблица (Исходные данные + Mu + Alpha)
    float combined_table_height = available_height_content * 0.40f;
    if (combined_table_height < 150 && available_height_content > 150) combined_table_height = 150;
    else if (combined_table_height > available_height_content) combined_table_height = available_height_content;
    if (combined_table_height < 0) combined_table_height = 0;

    Rectangle combined_table_panel_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, combined_table_height
    };
    TableData l1_combined_table = CreateL1CombinedInitialTable(data); // Эта функция создает временную таблицу
    if (l1_combined_table.row_count > 0 && l1_combined_table.column_names) {
        int temp_selected_row_combined = -1;
        DrawDataTable_RL(data, &l1_combined_table, "Исходные данные, Mu и Alpha", 
                         combined_table_panel_bounds,
                         &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][0], 
                         &temp_selected_row_combined, false);
    } else {
        DrawText("Не удалось сформировать объединенную таблицу.",
                 (int)combined_table_panel_bounds.x + 5, (int)combined_table_panel_bounds.y + 5, 18, RED);
    }
    free_table_data_content_rl(&l1_combined_table); // <--- Освобождаем временную таблицу
    current_y_offset += combined_table_panel_bounds.height + content_padding;


    // 4. Секции сглаживания (таблицы и графики)
    float remaining_height_for_smoothing_area = tab_content_bounds.y + tab_content_bounds.height - current_y_offset - content_padding;
    if (remaining_height_for_smoothing_area < 150) { /* ... */ return; }
    float smoothing_section_width = available_width_content / 2.0f - content_padding / 2.0f;
    if (smoothing_section_width < 100) smoothing_section_width = 100; 

    float table_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.45f; 
    float graph_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.55f - content_padding;

    if (table_smoothing_height_alloc < 80) table_smoothing_height_alloc = 80;
    if (graph_smoothing_height_alloc < 100) graph_smoothing_height_alloc = 100;
    
    if (table_smoothing_height_alloc + graph_smoothing_height_alloc + content_padding > remaining_height_for_smoothing_area) {
        graph_smoothing_height_alloc = remaining_height_for_smoothing_area - table_smoothing_height_alloc - content_padding;
        if (graph_smoothing_height_alloc < 80 && remaining_height_for_smoothing_area > (80 + 80 + content_padding)) { 
            table_smoothing_height_alloc = remaining_height_for_smoothing_area - 80 - content_padding;
            graph_smoothing_height_alloc = 80;
        }
        if (table_smoothing_height_alloc < 80) table_smoothing_height_alloc = fmaxf(0, remaining_height_for_smoothing_area - graph_smoothing_height_alloc - content_padding);
        if (graph_smoothing_height_alloc < 0) graph_smoothing_height_alloc = 0;
        if (table_smoothing_height_alloc < 0) table_smoothing_height_alloc = 0;
    }

    // --- Секция сглаживания для Мю ---
    Rectangle smooth_mu_table_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
    if (data->smoothing_data && data->smoothing_row_count > 0) {
        TableData mu_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_data, data->smoothing_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->mu_data, data->mu_data_count, "Mu (реальное)", 0);
        if (mu_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &mu_smoothing_table_display, NULL, 
                             smooth_mu_table_bounds, &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][1], &temp_sel_row, false);
        }
        free_table_data_content_rl(&mu_smoothing_table_display); // <--- Освобождаем
    } else {
        DrawText("Нет данных для таблицы сглаживания Mu", (int)smooth_mu_table_bounds.x + 5, (int)smooth_mu_table_bounds.y + 5, 16, DARKGRAY);
    }

    Rectangle smooth_mu_graph_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    if (data->mu_data && data->mu_data_count > 0) {
        #define MAX_SERIES_MU_INIT (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 mu_graph_series_arr[MAX_SERIES_MU_INIT];
        int actual_series_count_mu = 0;
        double* temp_allocated_y_for_mu_smooth[NUM_SMOOTHING_COEFFS] = {NULL}; // Для последующего free

        // Серия 1: Исходные Mu
        mu_graph_series_arr[actual_series_count_mu].x_values = NULL;
        mu_graph_series_arr[actual_series_count_mu].y_values = data->mu_data;
        mu_graph_series_arr[actual_series_count_mu].num_points = data->mu_data_count;
        mu_graph_series_arr[actual_series_count_mu].color = BLACK;
        mu_graph_series_arr[actual_series_count_mu].name = "Mu (исх.)";
        actual_series_count_mu++;

        // Серии 2..N: Сглаженные Mu
        Color mu_smooth_palette[] = {RED, GREEN, BLUE, ORANGE}; // Добавь свои цвета
        if (data->smoothing_data && data->smoothing_row_count > 0) {
            static char mu_legend_buffers[NUM_SMOOTHING_COEFFS][20]; // Буферы для имен легенды
            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_mu >= MAX_SERIES_MU_INIT) break;
                
                double* smoothed_y = (double*)malloc(data->smoothing_row_count * sizeof(double));
                if (!smoothed_y) {
                    TraceLog(LOG_ERROR, "L1_InitialData: Malloc failed for Mu smooth series Y (coeff %d)", c);
                    continue;
                }
                temp_allocated_y_for_mu_smooth[c] = smoothed_y; // Сохраняем для free

                for (int r = 0; r < data->smoothing_row_count; ++r) {
                    smoothed_y[r] = (data->smoothing_data[r]) ? data->smoothing_data[r][c] : NAN;
                }
                
                mu_graph_series_arr[actual_series_count_mu].x_values = NULL;
                mu_graph_series_arr[actual_series_count_mu].y_values = smoothed_y;
                mu_graph_series_arr[actual_series_count_mu].num_points = data->smoothing_row_count;
                mu_graph_series_arr[actual_series_count_mu].color = mu_smooth_palette[c % (sizeof(mu_smooth_palette)/sizeof(Color))];
                snprintf(mu_legend_buffers[c], sizeof(mu_legend_buffers[c]), "A=%.1f", SMOOTHING_COEFFS[c]);
                mu_graph_series_arr[actual_series_count_mu].name = mu_legend_buffers[c];
                actual_series_count_mu++;
            }
        }

        if (actual_series_count_mu > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_mu_graph_bounds,
                                        mu_graph_series_arr, actual_series_count_mu,
                                        &data->graph_states[GRAPH_ID_L1_MU],
                                        "M (период)", "Значение Mu");
        }
        
        // Освобождаем память, выделенную для сглаженных серий
        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_for_mu_smooth[c]) {
                free(temp_allocated_y_for_mu_smooth[c]);
            }
        }
    } else {
        DrawText("Нет данных для графика Mu", (int)smooth_mu_graph_bounds.x + 10, (int)smooth_mu_graph_bounds.y + 30, 16, DARKGRAY);
    }

    // --- Секция сглаживания для Альфа --- (аналогично Мю)
    float alpha_section_x_start = tab_content_bounds.x + content_padding + smoothing_section_width + content_padding;
    Rectangle smooth_alpha_table_bounds = {
        alpha_section_x_start, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
     if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
        TableData alpha_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_alpha_data, data->smoothing_alpha_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->alpha_data, data->alpha_count, "Alpha (реальное)", 1); // offset 1 для alpha
        if (alpha_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &alpha_smoothing_table_display, "Сглаживание Alpha", 
                             smooth_alpha_table_bounds, &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][2], &temp_sel_row, false);
        }
        free_table_data_content_rl(&alpha_smoothing_table_display); // <--- Освобождаем
    } else {
        DrawText("Нет данных для таблицы сглаживания Alpha", (int)smooth_alpha_table_bounds.x + 5, (int)smooth_alpha_table_bounds.y + 5, 16, DARKGRAY);
    }

    Rectangle smooth_alpha_graph_bounds = {
        alpha_section_x_start, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    if (data->alpha_data && data->alpha_count > 1) { // alpha_count > 1 для сглаживания (т.к. alpha[0] не сглаживается)
        #define MAX_SERIES_ALPHA_INIT (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 alpha_graph_series_arr[MAX_SERIES_ALPHA_INIT];
        int actual_series_count_alpha = 0;
        double* temp_allocated_y_for_alpha_smooth[NUM_SMOOTHING_COEFFS] = {NULL};

        // Серия 1: Исходные Alpha (начиная с alpha_data[1])
        alpha_graph_series_arr[actual_series_count_alpha].x_values = NULL;
        alpha_graph_series_arr[actual_series_count_alpha].y_values = &data->alpha_data[1]; // Начинаем с индекса 1
        alpha_graph_series_arr[actual_series_count_alpha].num_points = data->alpha_count - 1;
        alpha_graph_series_arr[actual_series_count_alpha].color = BLACK;
        alpha_graph_series_arr[actual_series_count_alpha].name = "Alpha (исх.)";
        actual_series_count_alpha++;

        // Серии 2..N: Сглаженные Alpha
        Color alpha_smooth_palette[] = {RED, GREEN, BLUE, ORANGE, PURPLE, DARKGREEN}; // Расширь палитру при необходимости
        if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
            // Статические буферы для имен легенды. Убедись, что NUM_SMOOTHING_COEFFS не слишком большой.
            // Если NUM_SMOOTHING_COEFFS может быть большим, лучше выделять динамически или увеличить размер.
            static char alpha_legend_buffers[NUM_SMOOTHING_COEFFS][20]; 

            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_alpha >= MAX_SERIES_ALPHA_INIT) {
                    TraceLog(LOG_WARNING, "L1_InitialData: Достигнут лимит серий для Alpha графика (%d)", MAX_SERIES_ALPHA_INIT);
                    break;
                }

                double* smoothed_y = (double*)malloc(data->smoothing_alpha_row_count * sizeof(double));
                if (!smoothed_y) {
                    TraceLog(LOG_ERROR, "L1_InitialData: Malloc failed for Alpha smooth series Y (coeff %d)", c);
                    continue; // Пропустить эту серию, если не удалось выделить память
                }
                temp_allocated_y_for_alpha_smooth[c] = smoothed_y; // Сохраняем для последующего free

                for (int r = 0; r < data->smoothing_alpha_row_count; ++r) {
                    // Проверяем, что data->smoothing_alpha_data[r] не NULL перед доступом к [c]
                    smoothed_y[r] = (data->smoothing_alpha_data[r] && c < NUM_SMOOTHING_COEFFS) ? data->smoothing_alpha_data[r][c] : NAN;
                }
                
                // Заполнение структуры GraphSeriesData_L1 для текущей сглаженной серии Alpha
                alpha_graph_series_arr[actual_series_count_alpha].x_values = NULL; // Используем индексы для оси X (M-периоды)
                alpha_graph_series_arr[actual_series_count_alpha].y_values = smoothed_y; // Указатель на только что заполненные Y-данные
                alpha_graph_series_arr[actual_series_count_alpha].num_points = data->smoothing_alpha_row_count; // Количество точек в этой серии
                alpha_graph_series_arr[actual_series_count_alpha].color = alpha_smooth_palette[c % (sizeof(alpha_smooth_palette)/sizeof(Color))]; // Цвет из палитры
                
                // Формируем имя для легенды
                snprintf(alpha_legend_buffers[c], sizeof(alpha_legend_buffers[c]), "A=%.1f", SMOOTHING_COEFFS[c]);
                alpha_graph_series_arr[actual_series_count_alpha].name = alpha_legend_buffers[c];
                
                actual_series_count_alpha++; // Увеличиваем счетчик фактически добавленных серий
            }
        }
        
        if (actual_series_count_alpha > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_alpha_graph_bounds,
                                        alpha_graph_series_arr, actual_series_count_alpha,
                                        &data->graph_states[GRAPH_ID_L1_ALPHA],
                                        "M (период)", "Значение Alpha");
        }

        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_for_alpha_smooth[c]) {
                free(temp_allocated_y_for_alpha_smooth[c]);
            }
        }
    } else {
        DrawText("Нет данных для графика Alpha", (int)smooth_alpha_graph_bounds.x + 10, (int)smooth_alpha_graph_bounds.y + 30, 16, DARKGRAY);
    }
}


static bool EnsureLevel1PlusECalculations(AppData_Raylib *data) {
    if (!data) {
        TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: AppData is NULL.");
        return false;
    }

    // 1. Освобождаем все предыдущие 'prime' данные, так как мы их будем пересчитывать
    TraceLog(LOG_DEBUG, "L1_PLUS_E_CALC: Очистка предыдущих 'prime' данных.");
    if (data->mu_prime_data) { free(data->mu_prime_data); data->mu_prime_data = NULL; data->mu_prime_data_count = 0; }
    if (data->alpha_prime_data) { free(data->alpha_prime_data); data->alpha_prime_data = NULL; data->alpha_prime_data_count = 0; }
    if (data->smoothing_mu_prime_data) {
        for (int i = 0; i < data->smoothing_mu_prime_row_count; ++i) if (data->smoothing_mu_prime_data[i]) free(data->smoothing_mu_prime_data[i]);
        free(data->smoothing_mu_prime_data);
        data->smoothing_mu_prime_data = NULL; data->smoothing_mu_prime_row_count = 0;
    }
    if (data->smoothing_alpha_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_prime_row_count; ++i) if (data->smoothing_alpha_prime_data[i]) free(data->smoothing_alpha_prime_data[i]);
        free(data->smoothing_alpha_prime_data);
        data->smoothing_alpha_prime_data = NULL; data->smoothing_alpha_prime_row_count = 0;
    }
    // Если вы храните data->error_adjusted_table_data, его тоже нужно очистить здесь:
    // free_table_data_content_rl(&data->error_adjusted_table_data); // Если это TableData
    // Или если это просто double**, то соответствующим образом.

    // 2. Проверяем наличие базовых данных
    if (!data->db_opened || data->table_data_main.row_count == 0) {
        TraceLog(LOG_WARNING, "L1_PLUS_E_CALC: Нет данных в table_data_main для вычислений +E.");
        return false;
    }
    if (data->table_data_values.row_count == 0 || !data->table_data_values.table || !data->table_data_values.table[0]) {
        TraceLog(LOG_WARNING, "L1_PLUS_E_CALC: Таблица 'Значения' пуста или не содержит данных для E.");
        return false;
    }

    // 3. Получение значения E
    double value_E = 0.0;
    int e_column_data_idx = -1; 
    if (data->table_data_values.column_names && data->table_data_values.num_allocated_column_names > 0) {
        for (int i = 0; i < data->table_data_values.num_allocated_column_names; ++i) {
            // Сравниваем имя колонки из PRAGMA (которое соответствует порядку в table[0])
            if (data->table_data_values.column_names[i] && strcmp(data->table_data_values.column_names[i], "E") == 0) {
                e_column_data_idx = i; // i - это и есть индекс для data->table_data_values.table[0]
                break;
            }
        }
    }

    if (e_column_data_idx == -1) {
        TraceLog(LOG_WARNING, "L1_PLUS_E_CALC: Колонка 'E' не найдена в таблице 'Значения'. Используется E=0.0.");
        // value_E остается 0.0
    } else if (e_column_data_idx >= data->table_data_values.column_count) {
        // Эта проверка на случай, если логика определения column_count для "Значения" изменится
        TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: Индекс колонки 'E' (%d) выходит за пределы column_count (%d) таблицы 'Значения'. Используется E=0.0.", 
                 e_column_data_idx, data->table_data_values.column_count);
        // value_E остается 0.0
    } else {
        value_E = data->table_data_values.table[0][e_column_data_idx];
    }
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Используется значение E = %.4f (из колонки с PRAGMA-индексом %d)", value_E, e_column_data_idx);

    // 4. Создание временной таблицы X+E (локально для этой функции)
    TableData table_x_plus_e = {0}; // Важно инициализировать нулями!
    table_x_plus_e.row_count = data->table_data_main.row_count;
    table_x_plus_e.column_count = data->table_data_main.column_count;
    // Эпохи и имена колонок для table_x_plus_e не обязательны, если compute_mu/alpha_rl их не используют напрямую
    // и если мы не собираемся отображать table_x_plus_e напрямую с помощью DrawDataTable_RL.
    // Но если вы решите хранить X+E в data->error_adjusted_table_data (как TableData), то их нужно будет заполнить.

    table_x_plus_e.table = (double**)calloc(table_x_plus_e.row_count, sizeof(double*));
    if (!table_x_plus_e.table) { 
        TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: calloc error for table_x_plus_e.table"); 
        return false; 
    }

    for (int i = 0; i < table_x_plus_e.row_count; ++i) {
        if (table_x_plus_e.column_count > 0) {
            table_x_plus_e.table[i] = (double*)calloc(table_x_plus_e.column_count, sizeof(double));
            if (!table_x_plus_e.table[i]) {
                TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: calloc error for table_x_plus_e.table[%d]", i);
                for(int k=0; k<i; ++k) if(table_x_plus_e.table[k]) free(table_x_plus_e.table[k]);
                free(table_x_plus_e.table);
                return false;
            }
            if (data->table_data_main.table[i]) { 
                for (int j = 0; j < table_x_plus_e.column_count; ++j) {
                    table_x_plus_e.table[i][j] = data->table_data_main.table[i][j] + value_E;
                }
            } else { 
                 for (int j = 0; j < table_x_plus_e.column_count; ++j) table_x_plus_e.table[i][j] = NAN;
                 TraceLog(LOG_WARNING, "L1_PLUS_E_CALC: Исходная строка table_data_main.table[%d] была NULL.", i);
            }
        } else {
            table_x_plus_e.table[i] = NULL;
        }
    }
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Временная таблица X+E создана.");

    // 5. Вычисление μ' и α'
    data->mu_prime_data = compute_mu_rl(table_x_plus_e.table, table_x_plus_e.row_count, table_x_plus_e.column_count);
    if (!data->mu_prime_data) {
        TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: Не удалось вычислить μ'.");
        free_table_data_content_rl(&table_x_plus_e); 
        return false;
    }
    data->mu_prime_data_count = table_x_plus_e.row_count;
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: μ' вычислены (%d значений).", data->mu_prime_data_count);

    data->alpha_prime_data = compute_alpha_rl(table_x_plus_e.table, // Используется временная таблица table_x_plus_e
                                          table_x_plus_e.row_count,
                                          table_x_plus_e.column_count,
                                          data->mu_prime_data,
                                          data->current_main_table_name);
    if (!data->alpha_prime_data) {
        TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: Не удалось вычислить α'.");
        free(data->mu_prime_data); data->mu_prime_data = NULL; data->mu_prime_data_count = 0;
        free_table_data_content_rl(&table_x_plus_e);
        return false;
    }
    data->alpha_prime_data_count = table_x_plus_e.row_count;
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: α' вычислены (%d значений).", data->alpha_prime_data_count);

    // Очищаем временную таблицу X+E, так как она больше не нужна для этих вычислений
    free_table_data_content_rl(&table_x_plus_e); // Эта функция обнулит поля table_x_plus_e
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Временная таблица X+E очищена.");

    // 6. Вычисление сглаживания для μ' и α'
    if (data->mu_prime_data && data->mu_prime_data_count > 0) {
        TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Вычисление сглаживания для μ' (данных: %d)", data->mu_prime_data_count);
        data->smoothing_mu_prime_data = compute_smoothing_rl(data->mu_prime_data, data->mu_prime_data_count,
                                                             &data->smoothing_mu_prime_row_count,
                                                             (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_mu_prime_data) {
            TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: Не удалось вычислить сглаживание для μ'.");
            // Не обязательно возвращать false, если основные μ' и α' посчитаны
        } else {
            TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Сглаживание μ' завершено (строк результата: %d).", data->smoothing_mu_prime_row_count);
        }
    }

    if (data->alpha_prime_data && data->alpha_prime_data_count > 1) {
        TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Вычисление сглаживания для α' (данных: %d, начиная с индекса 1)", data->alpha_prime_data_count - 1);
        data->smoothing_alpha_prime_data = compute_smoothing_rl(&data->alpha_prime_data[1], data->alpha_prime_data_count - 1,
                                                                &data->smoothing_alpha_prime_row_count,
                                                                (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_alpha_prime_data) {
            TraceLog(LOG_ERROR, "L1_PLUS_E_CALC: Не удалось вычислить сглаживание для α'.");
        } else {
            TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Сглаживание α' завершено (строк результата: %d).", data->smoothing_alpha_prime_row_count);
        }
    } else if (data->alpha_prime_data) { // Если alpha_prime_data есть, но точек мало
        TraceLog(LOG_WARNING, "L1_PLUS_E_CALC: Недостаточно данных α' для сглаживания (alpha_prime_data_count <= 1).");
    }
    
    TraceLog(LOG_INFO, "L1_PLUS_E_CALC: Все вычисления для +E завершены.");
    return true;
}


// --- Функция отрисовки для вкладки +E ---
static void DrawLevel1Tab_DataPlusE(AppData_Raylib *data, Rectangle tab_content_bounds) {
    if (!data) {
        DrawText("AppData is NULL in DrawLevel1Tab_DataPlusE!", (int)tab_content_bounds.x + 10, (int)tab_content_bounds.y + 10, 18, RED);
        return;
    }

    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 450, 20}, "Данные +E, Mu', Alpha' и их сглаживание");
    
    float current_y_offset = tab_content_bounds.y + 40;
    float content_padding = 10.0f;

    // 1. Проверка и вычисление данных для +E
    bool prime_data_ready_for_display = false;
    if (!data->mu_prime_data || data->mu_prime_data_count != data->table_data_main.row_count ||
        !data->alpha_prime_data || data->alpha_prime_data_count != data->table_data_main.row_count ||
        // Проверка актуальности данных сглаживания (упрощенная, можно детализировать как в InitialData)
        (data->mu_prime_data_count > 0 && (!data->smoothing_mu_prime_data || data->smoothing_mu_prime_row_count != (data->mu_prime_data_count + 1))) ||
        (data->alpha_prime_data_count > 1 && (!data->smoothing_alpha_prime_data || data->smoothing_alpha_prime_row_count != (data->alpha_prime_data_count)))
        ) {
        TraceLog(LOG_DEBUG, "L1_DataPlusE: Данные для +E не актуальны, вызываем EnsureLevel1PlusECalculations.");
        if (EnsureLevel1PlusECalculations(data)) { // Эта функция должна очистить старые и рассчитать новые
            prime_data_ready_for_display = (data->mu_prime_data && data->alpha_prime_data);
        }
    } else {
        prime_data_ready_for_display = true; 
    }

    if (!prime_data_ready_for_display) {
        DrawText("Ошибка/нет данных для +E. Проверьте значение E и логи.",
                 (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, RED);
        return;
    }
    
    double value_E_for_display = 0.0; // Получаем E для отображения в названии таблицы (если нужно)
    int e_col_idx_display = -1; 
    if (data->table_data_values.row_count > 0 && data->table_data_values.column_names && data->table_data_values.table && data->table_data_values.table[0]) {
        for (int i = 0; i < data->table_data_values.num_allocated_column_names; ++i) {
            if (data->table_data_values.column_names[i] && strcmp(data->table_data_values.column_names[i], "E") == 0) {
                e_col_idx_display = i; break;
            }
        }
        if (e_col_idx_display != -1 && e_col_idx_display < data->table_data_values.column_count) {
            value_E_for_display = data->table_data_values.table[0][e_col_idx_display];
        }
    }

    float available_width_content = tab_content_bounds.width - 2 * content_padding;
    float available_height_content = tab_content_bounds.height - (current_y_offset - tab_content_bounds.y) - content_padding;

    // Комбинированная таблица (X+E, Mu', Alpha')
    float combined_table_height = available_height_content * 0.40f;
    if (combined_table_height < 150 && available_height_content > 150) combined_table_height = 150;
    else if (combined_table_height > available_height_content) combined_table_height = available_height_content;
    if (combined_table_height < 0) combined_table_height = 0;

    Rectangle combined_table_panel_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, combined_table_height
    };
    TableData l1_plus_e_combined_table = CreateL1PlusECombinedTable(data, value_E_for_display);
    if (l1_plus_e_combined_table.row_count > 0 && l1_plus_e_combined_table.column_names) {
        int temp_sel_row = -1;
        DrawDataTable_RL(data, &l1_plus_e_combined_table, "Данные X+E, Mu', Alpha'", 
                         combined_table_panel_bounds,
                         &data->l1_tables_scroll[L1_TAB_DATA_PLUS_E][0], 
                         &temp_sel_row, false);
    } else {
        DrawText("Не удалось сформировать таблицу (X+E, Mu', Alpha').",
                 (int)combined_table_panel_bounds.x + 5, (int)combined_table_panel_bounds.y + 5, 18, RED);
    }
    free_table_data_content_rl(&l1_plus_e_combined_table);
    current_y_offset += combined_table_panel_bounds.height + content_padding;

    // Секции сглаживания
    float remaining_height_for_smoothing_area = tab_content_bounds.y + tab_content_bounds.height - current_y_offset - content_padding;
    if (remaining_height_for_smoothing_area < 150) {
        if (remaining_height_for_smoothing_area > 20) {
             DrawText("Недостаточно места для секций сглаживания (+E).", (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, GRAY);
        }
        return; 
    }
    float smoothing_section_width = available_width_content / 2.0f - content_padding / 2.0f;
    if (smoothing_section_width < 100) smoothing_section_width = 100; 
    float table_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.45f; 
    float graph_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.55f - content_padding;
    
    if (table_smoothing_height_alloc < 80) table_smoothing_height_alloc = 80;
    if (graph_smoothing_height_alloc < 100) graph_smoothing_height_alloc = 100;
    if (table_smoothing_height_alloc + graph_smoothing_height_alloc + content_padding > remaining_height_for_smoothing_area) {
        graph_smoothing_height_alloc = remaining_height_for_smoothing_area - table_smoothing_height_alloc - content_padding;
        if (graph_smoothing_height_alloc < 80 && remaining_height_for_smoothing_area > (80 + 80 + content_padding)) { 
            table_smoothing_height_alloc = remaining_height_for_smoothing_area - 80 - content_padding;
            graph_smoothing_height_alloc = 80;
        }
        if (table_smoothing_height_alloc < 80) table_smoothing_height_alloc = fmaxf(0, remaining_height_for_smoothing_area - graph_smoothing_height_alloc - content_padding);
        if (graph_smoothing_height_alloc < 0) graph_smoothing_height_alloc = 0;
        if (table_smoothing_height_alloc < 0) table_smoothing_height_alloc = 0;
    }

    // --- Секция сглаживания для Mu' ---
    Rectangle smooth_mu_prime_table_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
    if (data->smoothing_mu_prime_data && data->smoothing_mu_prime_row_count > 0) {
        TableData mu_prime_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_mu_prime_data, data->smoothing_mu_prime_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->mu_prime_data, data->mu_prime_data_count, "Mu' (реальное)", 0);
        if (mu_prime_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &mu_prime_smoothing_table_display, NULL,
                             smooth_mu_prime_table_bounds, &data->l1_tables_scroll[L1_TAB_DATA_PLUS_E][1], &temp_sel_row, false);
        }
        free_table_data_content_rl(&mu_prime_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания Mu'", (int)smooth_mu_prime_table_bounds.x + 5, (int)smooth_mu_prime_table_bounds.y + 5, 16, DARKGRAY);
    }

    Rectangle smooth_mu_prime_graph_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    if (data->mu_prime_data && data->mu_prime_data_count > 0) {
        #define MAX_SERIES_MU_PRIME_TAB (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 mu_prime_graph_series_arr[MAX_SERIES_MU_PRIME_TAB];
        int actual_series_count_mu_prime = 0;
        double* temp_allocated_y_mu_prime_smooth[NUM_SMOOTHING_COEFFS] = {NULL}; 

        // Серия 1: Исходные Mu'
        if (actual_series_count_mu_prime < MAX_SERIES_MU_PRIME_TAB) {
            mu_prime_graph_series_arr[actual_series_count_mu_prime].x_values = NULL;
            mu_prime_graph_series_arr[actual_series_count_mu_prime].y_values = data->mu_prime_data;
            mu_prime_graph_series_arr[actual_series_count_mu_prime].num_points = data->mu_prime_data_count;
            mu_prime_graph_series_arr[actual_series_count_mu_prime].color = BLACK;
            mu_prime_graph_series_arr[actual_series_count_mu_prime].name = "Mu' (исх.)";
            actual_series_count_mu_prime++;
        }

        // Серии 2..N: Сглаженные Mu'
        Color mu_prime_palette[] = {RED, GREEN, BLUE, ORANGE}; 
        if (data->smoothing_mu_prime_data && data->smoothing_mu_prime_row_count > 0) {
            static char mu_prime_legend_buffs[NUM_SMOOTHING_COEFFS][20];
            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_mu_prime >= MAX_SERIES_MU_PRIME_TAB) break;
                
                double* smoothed_y = (double*)malloc(data->smoothing_mu_prime_row_count * sizeof(double));
                if (!smoothed_y) {
                    TraceLog(LOG_ERROR, "L1_DataPlusE: Malloc failed for Mu' smooth series Y (coeff_idx %d, needed %d bytes)", 
                             c, (int)(data->smoothing_mu_prime_row_count * sizeof(double)));
                    // temp_allocated_y_mu_prime_smooth[c] останется NULL, free для него не вызовется
                    continue; // Пропустить эту серию, если не удалось выделить память
                }
                temp_allocated_y_mu_prime_smooth[c] = smoothed_y; 

                for (int r = 0; r < data->smoothing_mu_prime_row_count; ++r) {
                    smoothed_y[r] = (data->smoothing_mu_prime_data[r]) ? data->smoothing_mu_prime_data[r][c] : NAN;
                }
                
                mu_prime_graph_series_arr[actual_series_count_mu_prime].x_values = NULL;
                mu_prime_graph_series_arr[actual_series_count_mu_prime].y_values = smoothed_y;
                mu_prime_graph_series_arr[actual_series_count_mu_prime].num_points = data->smoothing_mu_prime_row_count;
                mu_prime_graph_series_arr[actual_series_count_mu_prime].color = mu_prime_palette[c % 4];
                snprintf(mu_prime_legend_buffs[c], 20, "A=%.1f", SMOOTHING_COEFFS[c]);
                mu_prime_graph_series_arr[actual_series_count_mu_prime].name = mu_prime_legend_buffs[c];
                actual_series_count_mu_prime++;
            }
        }

        if (actual_series_count_mu_prime > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_mu_prime_graph_bounds,
                                        mu_prime_graph_series_arr, actual_series_count_mu_prime,
                                        &data->graph_states[GRAPH_ID_L1_MU_PRIME],
                                        "M (период)", "Значение Mu'");
        }
        
        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_mu_prime_smooth[c]) free(temp_allocated_y_mu_prime_smooth[c]);
        }
    } else {
        DrawText("Нет данных для графика Mu'", (int)smooth_mu_prime_graph_bounds.x + 10, (int)smooth_mu_prime_graph_bounds.y + 30, 16, DARKGRAY);
    }

    // --- Секция сглаживания для Alpha' ---
    float alpha_prime_section_x_start = tab_content_bounds.x + content_padding + smoothing_section_width + content_padding;
    Rectangle smooth_alpha_prime_table_bounds = {
        alpha_prime_section_x_start, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
     if (data->smoothing_alpha_prime_data && data->smoothing_alpha_prime_row_count > 0) {
        TableData alpha_prime_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_alpha_prime_data, data->smoothing_alpha_prime_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->alpha_prime_data, data->alpha_prime_data_count, "Alpha' (реальное)", 1); 
        if (alpha_prime_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &alpha_prime_smoothing_table_display, "Сглаживание Alpha'",
                             smooth_alpha_prime_table_bounds, &data->l1_tables_scroll[L1_TAB_DATA_PLUS_E][2], &temp_sel_row, false);
        }
        free_table_data_content_rl(&alpha_prime_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания Alpha'", (int)smooth_alpha_prime_table_bounds.x + 5, (int)smooth_alpha_prime_table_bounds.y + 5, 16, DARKGRAY);
    }
    
    Rectangle smooth_alpha_prime_graph_bounds = {
        alpha_prime_section_x_start, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    if (data->alpha_prime_data && data->alpha_prime_data_count > 1) {
        #define MAX_SERIES_ALPHA_PRIME_TAB (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 alpha_prime_graph_series_arr[MAX_SERIES_ALPHA_PRIME_TAB];
        int actual_series_count_alpha_prime = 0;
        double* temp_allocated_y_alpha_prime_smooth[NUM_SMOOTHING_COEFFS] = {NULL};

        // Серия 1: Исходные Alpha'
        if (actual_series_count_alpha_prime < MAX_SERIES_ALPHA_PRIME_TAB) {
            alpha_prime_graph_series_arr[actual_series_count_alpha_prime].x_values = NULL;
            alpha_prime_graph_series_arr[actual_series_count_alpha_prime].y_values = &data->alpha_prime_data[1];
            alpha_prime_graph_series_arr[actual_series_count_alpha_prime].num_points = data->alpha_prime_data_count - 1;
            alpha_prime_graph_series_arr[actual_series_count_alpha_prime].color = BLACK;
            alpha_prime_graph_series_arr[actual_series_count_alpha_prime].name = "Alpha' (исх.)";
            actual_series_count_alpha_prime++;
        }

        // Серии 2..N: Сглаженные Alpha'
        Color alpha_prime_palette[] = {RED, GREEN, BLUE, ORANGE};
        if (data->smoothing_alpha_prime_data && data->smoothing_alpha_prime_row_count > 0) {
            static char alpha_prime_legend_buffs[NUM_SMOOTHING_COEFFS][20];
            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_alpha_prime >= MAX_SERIES_ALPHA_PRIME_TAB) break;

                double* smoothed_y = (double*)malloc(data->smoothing_alpha_prime_row_count * sizeof(double));
                if (!smoothed_y) {
                    TraceLog(LOG_ERROR, "L1_DataPlusE: Malloc failed for Alpha' smooth series Y (coeff_idx %d, needed %d bytes)",
                             c, (int)(data->smoothing_alpha_prime_row_count * sizeof(double)));
                    // temp_allocated_y_alpha_prime_smooth[c] останется NULL
                    continue; // Пропустить эту серию
                }
                temp_allocated_y_alpha_prime_smooth[c] = smoothed_y;

                for (int r = 0; r < data->smoothing_alpha_prime_row_count; ++r) {
                    smoothed_y[r] = (data->smoothing_alpha_prime_data[r]) ? data->smoothing_alpha_prime_data[r][c] : NAN;
                }
                
                alpha_prime_graph_series_arr[actual_series_count_alpha_prime].x_values = NULL;
                alpha_prime_graph_series_arr[actual_series_count_alpha_prime].y_values = smoothed_y;
                alpha_prime_graph_series_arr[actual_series_count_alpha_prime].num_points = data->smoothing_alpha_prime_row_count;
                alpha_prime_graph_series_arr[actual_series_count_alpha_prime].color = alpha_prime_palette[c % 4];
                snprintf(alpha_prime_legend_buffs[c], 20, "A=%.1f", SMOOTHING_COEFFS[c]);
                alpha_prime_graph_series_arr[actual_series_count_alpha_prime].name = alpha_prime_legend_buffs[c];
                actual_series_count_alpha_prime++;
            }
        }

        if (actual_series_count_alpha_prime > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_alpha_prime_graph_bounds,
                                        alpha_prime_graph_series_arr, actual_series_count_alpha_prime,
                                        &data->graph_states[GRAPH_ID_L1_ALPHA_PRIME],
                                        "M (период)", "Значение Alpha'");
        }

        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_alpha_prime_smooth[c]) free(temp_allocated_y_alpha_prime_smooth[c]);
        }
    } else {
        DrawText("Нет данных для графика Alpha'", (int)smooth_alpha_prime_graph_bounds.x + 10, (int)smooth_alpha_prime_graph_bounds.y + 30, 16, DARKGRAY);
    }
}


static TableData CreateL1PlusECombinedTable(AppData_Raylib *data, double value_E) {
    TableData combined_table = {0}; // Инициализируем нулями!

    // Проверки на наличие основных данных
    // FOR -E: Здесь нужно будет проверять data->mu_double_prime_data и data->alpha_double_prime_data
    if (!data || !data->table_data_main.table || data->table_data_main.row_count == 0 ||
        !data->mu_prime_data || data->mu_prime_data_count != data->table_data_main.row_count ||
        !data->alpha_prime_data || data->alpha_prime_data_count != data->table_data_main.row_count) {
        // FOR -E: Изменить сообщение на "... таблицы -E." и "... Mu'', Alpha''."
        TraceLog(LOG_WARNING, "L1_PLUS_E_COMB_TABLE: Недостаточно данных для создания объединенной таблицы +E.");
        return combined_table; // Возвращаем пустую структуру
    }

    int base_rows = data->table_data_main.row_count;
    int base_data_cols = data->table_data_main.column_count;
    // FOR -E: Названия колонок и количество останутся теми же, только значения X будут X-E
    int total_data_cols_combined = base_data_cols + 2; // X_i +/- E, mu', alpha' (или mu'', alpha'')
    int total_pragma_cols_combined = total_data_cols_combined + 1; // +1 для колонки эпохи

    combined_table.row_count = base_rows;
    combined_table.column_count = total_data_cols_combined; 
    combined_table.num_allocated_column_names = total_pragma_cols_combined;

    // 1. Выделение памяти и копирование имен колонок
    combined_table.column_names = (char**)calloc(combined_table.num_allocated_column_names, sizeof(char*));
    if (!combined_table.column_names) {
        // FOR -E: Изменить сообщение в логе на "... таблицы -E."
        TraceLog(LOG_ERROR, "L1_PLUS_E_COMB_TABLE: Ошибка calloc для column_names.");
        return combined_table; 
    }

    // Копируем имя колонки эпохи
    if (data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names[0]) {
        combined_table.column_names[0] = strdup(data->table_data_main.column_names[0]);
    } else {
        combined_table.column_names[0] = strdup("Эпоха"); 
    }
    if (!combined_table.column_names[0]) goto cleanup_plus_e_combined_manual; // Используем новую метку

    // Копируем имена колонок X_i (с пометкой +E или -E)
    for (int i = 0; i < base_data_cols; ++i) {
        const char* base_col_name_ptr = ((i + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[i + 1])
                                   ? data->table_data_main.column_names[i + 1]
                                   : NULL; 
        char temp_base_col_name[64];
        if (base_col_name_ptr) {
            strncpy(temp_base_col_name, base_col_name_ptr, 63);
            temp_base_col_name[63] = '\0';
        } else {
            sprintf(temp_base_col_name, "X%d", i + 1);
        }
        
        // FOR -E: Изменить "(+E)" на "(-E)"
        char final_col_name[128];
        snprintf(final_col_name, sizeof(final_col_name), "%s(+E)", temp_base_col_name);
        combined_table.column_names[i + 1] = strdup(final_col_name);
        
        if (!combined_table.column_names[i + 1]) goto cleanup_plus_e_combined_manual;
    }
    // Добавляем имена для μ' и α'
    // FOR -E: Изменить "Mu'" на "Mu''" и "Alpha'" на "Alpha''"
    combined_table.column_names[base_data_cols + 1] = strdup("Mu'");
    if (!combined_table.column_names[base_data_cols + 1]) goto cleanup_plus_e_combined_manual;
    combined_table.column_names[base_data_cols + 2] = strdup("Alpha'");
    if (!combined_table.column_names[base_data_cols + 2]) goto cleanup_plus_e_combined_manual;


    // 2. Выделение памяти и копирование эпох
    combined_table.epochs = (int*)calloc(base_rows, sizeof(int));
    if (!combined_table.epochs) {
        // FOR -E: Изменить сообщение в логе
        TraceLog(LOG_ERROR, "L1_PLUS_E_COMB_TABLE: Ошибка calloc для epochs.");
        goto cleanup_plus_e_combined_manual; 
    }
    for (int i = 0; i < base_rows; ++i) {
        combined_table.epochs[i] = data->table_data_main.epochs[i];
    }

    // 3. Выделение памяти и заполнение таблицы данными
    combined_table.table = (double**)calloc(base_rows, sizeof(double*));
    if (!combined_table.table) {
        // FOR -E: Изменить сообщение в логе
        TraceLog(LOG_ERROR, "L1_PLUS_E_COMB_TABLE: Ошибка calloc для table.");
        goto cleanup_plus_e_combined_manual; 
    }
    for (int i = 0; i < base_rows; ++i) {
        combined_table.table[i] = (double*)calloc(total_data_cols_combined, sizeof(double));
        if (!combined_table.table[i]) {
            // FOR -E: Изменить сообщение в логе
            TraceLog(LOG_ERROR, "L1_PLUS_E_COMB_TABLE: Ошибка calloc для table[%d].", i);
            for(int k=0; k<i; ++k) if(combined_table.table[k]) free(combined_table.table[k]);
            free(combined_table.table); combined_table.table = NULL; 
            goto cleanup_plus_e_combined_manual;
        }

        // Значения X_i + E
        // FOR -E: Изменить `+ value_E` на `- value_E`
        if (data->table_data_main.table[i]) {
            for (int j = 0; j < base_data_cols; ++j) {
                combined_table.table[i][j] = data->table_data_main.table[i][j] + value_E;
            }
        } else {
            for (int j = 0; j < base_data_cols; ++j) combined_table.table[i][j] = NAN;
        }
        // Mu'
        // FOR -E: Использовать data->mu_double_prime_data[i]
        combined_table.table[i][base_data_cols] = data->mu_prime_data[i];
        // Alpha'
        // FOR -E: Использовать data->alpha_double_prime_data[i]
        combined_table.table[i][base_data_cols + 1] = data->alpha_prime_data[i];
    }

    // FOR -E: Изменить сообщение в логе
    TraceLog(LOG_INFO, "L1_PLUS_E_COMB_TABLE: Временная таблица (X+E, Mu', Alpha') создана (строк: %d, колонок данных: %d).",
             combined_table.row_count, combined_table.column_count);
    return combined_table;

cleanup_plus_e_combined_manual: // Метка для ручной очистки
    // FOR -E: Изменить сообщение в логе
    TraceLog(LOG_ERROR, "L1_PLUS_E_COMB_TABLE: Ошибка при создании таблицы +E, ручная очистка.");
    if (combined_table.column_names) {
        // Используем num_allocated_column_names, так как это фактический размер выделенного массива column_names
        for (int i = 0; i < combined_table.num_allocated_column_names; ++i) { 
            if (combined_table.column_names[i]) free(combined_table.column_names[i]);
        }
        free(combined_table.column_names);
        combined_table.column_names = NULL; // Важно для free_table_data_content_rl, если она будет вызвана для частично созданной таблицы
    }
    if (combined_table.epochs) {
        free(combined_table.epochs);
        combined_table.epochs = NULL;
    }
    if (combined_table.table) { 
        for (int i = 0; i < combined_table.row_count; ++i) { // row_count здесь может быть корректным, если ошибка была в выделении строки
            if (combined_table.table[i]) free(combined_table.table[i]);
        }
        free(combined_table.table);
        combined_table.table = NULL;
    }
    // Сбрасываем счетчики, чтобы показать, что таблица невалидна
    combined_table.row_count = 0;
    combined_table.column_count = 0;
    combined_table.num_allocated_column_names = 0;
    return combined_table;
}

static bool EnsureLevel1MinusECalculations(AppData_Raylib *data) {
    if (!data) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: AppData is NULL.");
        return false;
    }

    // 1. Освобождаем все предыдущие 'double_prime' данные, так как мы их будем пересчитывать
    // Это уже было в вашей версии, оставляем как есть, но убедимся в полноте.
    TraceLog(LOG_DEBUG, "L1_MINUS_E_CALC: Очистка предыдущих 'double_prime' данных (основных и сглаживания).");
    if (data->mu_double_prime_data) { free(data->mu_double_prime_data); data->mu_double_prime_data = NULL; data->mu_double_prime_data_count = 0; }
    if (data->alpha_double_prime_data) { free(data->alpha_double_prime_data); data->alpha_double_prime_data = NULL; data->alpha_double_prime_data_count = 0; }
    
    if (data->smoothing_mu_double_prime_data) {
        for (int i = 0; i < data->smoothing_mu_double_prime_row_count; ++i) if (data->smoothing_mu_double_prime_data[i]) free(data->smoothing_mu_double_prime_data[i]);
        free(data->smoothing_mu_double_prime_data);
        data->smoothing_mu_double_prime_data = NULL; data->smoothing_mu_double_prime_row_count = 0;
    }
    if (data->smoothing_alpha_double_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; ++i) if (data->smoothing_alpha_double_prime_data[i]) free(data->smoothing_alpha_double_prime_data[i]);
        free(data->smoothing_alpha_double_prime_data);
        data->smoothing_alpha_double_prime_data = NULL; data->smoothing_alpha_double_prime_row_count = 0;
    }

    // 2. Проверяем наличие базовых данных
    if (!data->db_opened || data->table_data_main.row_count == 0) {
        TraceLog(LOG_WARNING, "L1_MINUS_E_CALC: Нет данных в table_data_main для вычислений -E.");
        return false;
    }
    if (data->table_data_values.row_count == 0 || !data->table_data_values.table || !data->table_data_values.table[0] || !data->table_data_values.column_names) {
        TraceLog(LOG_WARNING, "L1_MINUS_E_CALC: Таблица 'Значения' пуста, не содержит данных или имен колонок для E.");
        return false;
    }

    // 3. Получение значения E
    double value_E = 0.0;
    int e_column_data_idx = -1;
    for (int i = 0; i < data->table_data_values.num_allocated_column_names; ++i) {
        if (data->table_data_values.column_names[i] && strcmp(data->table_data_values.column_names[i], "E") == 0) {
            e_column_data_idx = i; break;
        }
    }
    if (e_column_data_idx == -1) {
        TraceLog(LOG_WARNING, "L1_MINUS_E_CALC: Колонка 'E' не найдена в таблице 'Значения'. Используется E=0.0.");
    } else if (e_column_data_idx >= data->table_data_values.column_count) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: Индекс колонки 'E' (%d) выходит за пределы column_count (%d) таблицы 'Значения'. Используется E=0.0.",
                 e_column_data_idx, data->table_data_values.column_count);
    } else {
        value_E = data->table_data_values.table[0][e_column_data_idx];
    }
    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Используется значение E = %.4f (из колонки с PRAGMA-индексом %d)", value_E, e_column_data_idx);

    // 4. Создание временной таблицы X-E
    TableData table_x_minus_e = {0};
    table_x_minus_e.row_count = data->table_data_main.row_count;
    table_x_minus_e.column_count = data->table_data_main.column_count;
    table_x_minus_e.table = (double**)calloc(table_x_minus_e.row_count, sizeof(double*));
    if (!table_x_minus_e.table) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: calloc error for table_x_minus_e.table");
        return false;
    }
    for (int i = 0; i < table_x_minus_e.row_count; ++i) {
        if (table_x_minus_e.column_count > 0) {
            table_x_minus_e.table[i] = (double*)calloc(table_x_minus_e.column_count, sizeof(double));
            if (!table_x_minus_e.table[i]) {
                TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: calloc error for table_x_minus_e.table[%d]", i);
                for(int k=0; k<i; ++k) if(table_x_minus_e.table[k]) free(table_x_minus_e.table[k]);
                free(table_x_minus_e.table); table_x_minus_e.table = NULL; // Обнуляем, чтобы free_table_data_content_rl не пыталась освободить снова
                return false;
            }
            if (data->table_data_main.table[i]) {
                for (int j = 0; j < table_x_minus_e.column_count; ++j) {
                    table_x_minus_e.table[i][j] = data->table_data_main.table[i][j] - value_E;
                }
            } else {
                 for (int j = 0; j < table_x_minus_e.column_count; ++j) table_x_minus_e.table[i][j] = NAN;
            }
        } else {
            table_x_minus_e.table[i] = NULL;
        }
    }
    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Временная таблица X-E создана.");

    // 5. Вычисление μ'' и α''
    data->mu_double_prime_data = compute_mu_rl(table_x_minus_e.table, table_x_minus_e.row_count, table_x_minus_e.column_count);
    if (!data->mu_double_prime_data) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: Не удалось вычислить μ''.");
        free_table_data_content_rl(&table_x_minus_e);
        return false;
    }
    data->mu_double_prime_data_count = table_x_minus_e.row_count;
    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: μ'' вычислены (%d значений).", data->mu_double_prime_data_count);

    data->alpha_double_prime_data = compute_alpha_rl(table_x_minus_e.table, // Используется временная таблица table_x_minus_e
                                                 table_x_minus_e.row_count,
                                                 table_x_minus_e.column_count,
                                                 data->mu_double_prime_data,
                                                 data->current_main_table_name);
    if (!data->alpha_double_prime_data) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: Не удалось вычислить α''.");
        free(data->mu_double_prime_data); data->mu_double_prime_data = NULL; data->mu_double_prime_data_count = 0;
        free_table_data_content_rl(&table_x_minus_e);
        return false;
    }
    data->alpha_double_prime_data_count = table_x_minus_e.row_count;
    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: α'' вычислены (%d значений).", data->alpha_double_prime_data_count);

    free_table_data_content_rl(&table_x_minus_e);
    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Временная таблица X-E очищена.");

    // 6. Вычисление сглаживания для μ'' и α''
    // (Очистка data->smoothing_... была в начале функции)
    if (data->mu_double_prime_data && data->mu_double_prime_data_count > 0) {
        TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Вычисление сглаживания для μ'' (данных: %d)", data->mu_double_prime_data_count);
        data->smoothing_mu_double_prime_data = compute_smoothing_rl(
            data->mu_double_prime_data, data->mu_double_prime_data_count,
            &data->smoothing_mu_double_prime_row_count,
            (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_mu_double_prime_data) {
            TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: Не удалось вычислить сглаживание для μ''.");
        } else {
            TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Сглаживание μ'' завершено (строк результата: %d).", data->smoothing_mu_double_prime_row_count);
        }
    }

    if (data->alpha_double_prime_data && data->alpha_double_prime_data_count > 1) {
        TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Вычисление сглаживания для α'' (данных: %d, начиная с индекса 1)", data->alpha_double_prime_data_count - 1);
        data->smoothing_alpha_double_prime_data = compute_smoothing_rl(
            &data->alpha_double_prime_data[1], data->alpha_double_prime_data_count - 1,
            &data->smoothing_alpha_double_prime_row_count,
            (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_alpha_double_prime_data) {
            TraceLog(LOG_ERROR, "L1_MINUS_E_CALC: Не удалось вычислить сглаживание для α''.");
        } else {
            TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Сглаживание α'' завершено (строк результата: %d).", data->smoothing_alpha_double_prime_row_count);
        }
    } else if (data->alpha_double_prime_data) {
        TraceLog(LOG_WARNING, "L1_MINUS_E_CALC: Недостаточно данных α'' для сглаживания (alpha_double_prime_data_count <= 1).");
        // Убедимся, что если smoothing_alpha_double_prime_data было выделено ранее (хотя оно должно быть очищено в начале), оно будет NULL
        if(data->smoothing_alpha_double_prime_data) { // Эта проверка избыточна, если очистка в начале работает
             for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; ++i) if (data->smoothing_alpha_double_prime_data[i]) free(data->smoothing_alpha_double_prime_data[i]);
             free(data->smoothing_alpha_double_prime_data);
             data->smoothing_alpha_double_prime_data = NULL; data->smoothing_alpha_double_prime_row_count = 0;
        }
    }

    TraceLog(LOG_INFO, "L1_MINUS_E_CALC: Все вычисления для -E завершены.");
    return true;
}

static void DrawLevel1Tab_DataMinusE(AppData_Raylib *data, Rectangle tab_content_bounds) {
    if (!data) {
        DrawText("AppData is NULL in DrawLevel1Tab_DataMinusE!", (int)tab_content_bounds.x + 10, (int)tab_content_bounds.y + 10, 18, RED);
        return;
    }

    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 450, 20}, "Данные -E, Mu'', Alpha'' и их сглаживание");

    float current_y_offset = tab_content_bounds.y + 40;
    float content_padding = 10.0f;

    // 1. Проверка и вычисление данных для -E
    bool double_prime_data_ready_for_display = false;
    // Проверяем, нужно ли пересчитывать.
    // Условия неактуальности могут быть сложными, здесь упрощенная проверка.
    // Главное, чтобы EnsureLevel1MinusECalculations вызывалась, если данные действительно нужны и отсутствуют/устарели.
    if (!data->mu_double_prime_data || data->mu_double_prime_data_count != data->table_data_main.row_count ||
        !data->alpha_double_prime_data || data->alpha_double_prime_data_count != data->table_data_main.row_count ||
        // Проверка для данных сглаживания (если они нужны для отображения таблиц)
        (data->mu_double_prime_data_count > 0 && (!data->smoothing_mu_double_prime_data || data->smoothing_mu_double_prime_row_count != (data->mu_double_prime_data_count + 1))) ||
        (data->alpha_double_prime_data_count > 1 && (!data->smoothing_alpha_double_prime_data || data->smoothing_alpha_double_prime_row_count != (data->alpha_double_prime_data_count - 1 + 1)))
       ) {
        TraceLog(LOG_DEBUG, "L1_DataMinusE: Данные для -E (double_prime или их сглаживание) не актуальны или отсутствуют, вызываем EnsureLevel1MinusECalculations.");
        if (EnsureLevel1MinusECalculations(data)) {
            double_prime_data_ready_for_display = (data->mu_double_prime_data && data->alpha_double_prime_data);
        }
    } else {
        double_prime_data_ready_for_display = true;
    }

    if (!double_prime_data_ready_for_display) {
        DrawText("Ошибка/нет данных для -E. Проверьте значение E и логи.",
                 (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, RED);
        return;
    }

    double value_E_for_display = 0.0;
    int e_col_idx_display = -1;
    if (data->table_data_values.row_count > 0 && data->table_data_values.column_names && data->table_data_values.table && data->table_data_values.table[0]) {
        for (int i = 0; i < data->table_data_values.num_allocated_column_names; ++i) {
            if (data->table_data_values.column_names[i] && strcmp(data->table_data_values.column_names[i], "E") == 0) {
                e_col_idx_display = i; break;
            }
        }
        if (e_col_idx_display != -1 && e_col_idx_display < data->table_data_values.column_count) {
            value_E_for_display = data->table_data_values.table[0][e_col_idx_display];
        }
    }

    float available_width_content = tab_content_bounds.width - 2 * content_padding;
    float available_height_content = tab_content_bounds.height - (current_y_offset - tab_content_bounds.y) - content_padding;

    // Комбинированная таблица (X-E, Mu'', Alpha'')
    float combined_table_height = available_height_content * 0.40f;
    if (combined_table_height < 150 && available_height_content > 150) combined_table_height = 150;
    else if (combined_table_height > available_height_content) combined_table_height = available_height_content;
    if (combined_table_height < 0) combined_table_height = 0;

    Rectangle combined_table_panel_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, combined_table_height
    };
    TableData l1_minus_e_combined_table = CreateL1MinusECombinedTable(data, value_E_for_display);
    if (l1_minus_e_combined_table.row_count > 0 && l1_minus_e_combined_table.column_names) {
        int temp_sel_row = -1;
        DrawDataTable_RL(data, &l1_minus_e_combined_table, "Данные X-E, Mu'', Alpha''", // Добавил заголовок для ясности
                         combined_table_panel_bounds,
                         &data->l1_tables_scroll[L1_TAB_DATA_MINUS_E][0],
                         &temp_sel_row, false);
    } else {
        DrawText("Не удалось сформировать таблицу (X-E, Mu'', Alpha'').",
                 (int)combined_table_panel_bounds.x + 5, (int)combined_table_panel_bounds.y + 5, 18, RED);
    }
    free_table_data_content_rl(&l1_minus_e_combined_table);
    current_y_offset += combined_table_panel_bounds.height + content_padding;

    // Секции сглаживания
    float remaining_height_for_smoothing_area = tab_content_bounds.y + tab_content_bounds.height - current_y_offset - content_padding;
    if (remaining_height_for_smoothing_area < 150) {
        if (remaining_height_for_smoothing_area > 20) {
             DrawText("Недостаточно места для секций сглаживания (-E).", (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, GRAY);
        }
        return;
    }
    float smoothing_section_width = available_width_content / 2.0f - content_padding / 2.0f;
    if (smoothing_section_width < 100) smoothing_section_width = 100;
    float table_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.45f;
    float graph_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.55f - content_padding;

    // Коррекция размеров, если выходят за пределы или слишком малы
    if (table_smoothing_height_alloc < 80 && remaining_height_for_smoothing_area > 160 + content_padding) table_smoothing_height_alloc = 80;
    if (graph_smoothing_height_alloc < 100 && remaining_height_for_smoothing_area > table_smoothing_height_alloc + 100 + content_padding) graph_smoothing_height_alloc = 100;
    if (table_smoothing_height_alloc + graph_smoothing_height_alloc + content_padding > remaining_height_for_smoothing_area) {
        graph_smoothing_height_alloc = remaining_height_for_smoothing_area - table_smoothing_height_alloc - content_padding;
        if (graph_smoothing_height_alloc < 0) graph_smoothing_height_alloc = 0;
    }
    if (table_smoothing_height_alloc < 0) table_smoothing_height_alloc = 0;


    // --- Секция сглаживания для Mu'' (Таблица) ---
    Rectangle smooth_mu_double_prime_table_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
    TraceLog(LOG_DEBUG, "DrawL1MinusE: Перед таблицей сглаживания Mu'': smoothing_ptr=%p, rows=%d, mu_dprime_ptr=%p",
             (void*)data->smoothing_mu_double_prime_data, data->smoothing_mu_double_prime_row_count, (void*)data->mu_double_prime_data);

    if (data->smoothing_mu_double_prime_data && data->smoothing_mu_double_prime_row_count > 0 && data->mu_double_prime_data) {
        TableData mu_dprime_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_mu_double_prime_data, data->smoothing_mu_double_prime_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->mu_double_prime_data, data->mu_double_prime_data_count,
            "Mu'' (реальное)", 0);
        if (mu_dprime_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &mu_dprime_smoothing_table_display, "Сглаживание Mu''", // Добавил заголовок
                             smooth_mu_double_prime_table_bounds, &data->l1_tables_scroll[L1_TAB_DATA_MINUS_E][1], &temp_sel_row, false);
        } else {
            DrawText("Не удалось создать таблицу сглаживания Mu''", (int)smooth_mu_double_prime_table_bounds.x + 5, (int)smooth_mu_double_prime_table_bounds.y + 5, 16, DARKGRAY);
        }
        free_table_data_content_rl(&mu_dprime_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания Mu''", (int)smooth_mu_double_prime_table_bounds.x + 5, (int)smooth_mu_double_prime_table_bounds.y + 5, 16, DARKGRAY);
    }

    // --- Секция сглаживания для Mu'' (График) ---
    Rectangle smooth_mu_dprime_graph_bounds = {
        tab_content_bounds.x + content_padding,
        current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width,
        graph_smoothing_height_alloc
    };
    // (Код для графика Mu'' остается как в вашем предыдущем варианте, он должен работать, если данные есть)
    if (data->mu_double_prime_data && data->mu_double_prime_data_count > 0) {
        #define MAX_SERIES_MU_DPRIME_TAB (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 mu_dprime_graph_series_arr[MAX_SERIES_MU_DPRIME_TAB];
        int actual_series_count_mu_dprime = 0;
        double* temp_allocated_y_mu_dprime_smooth[NUM_SMOOTHING_COEFFS] = {NULL};
        if (actual_series_count_mu_dprime < MAX_SERIES_MU_DPRIME_TAB) {
            mu_dprime_graph_series_arr[actual_series_count_mu_dprime].x_values = NULL;
            mu_dprime_graph_series_arr[actual_series_count_mu_dprime].y_values = data->mu_double_prime_data;
            mu_dprime_graph_series_arr[actual_series_count_mu_dprime].num_points = data->mu_double_prime_data_count;
            mu_dprime_graph_series_arr[actual_series_count_mu_dprime].color = BLACK;
            mu_dprime_graph_series_arr[actual_series_count_mu_dprime].name = "Mu'' (исх.)";
            actual_series_count_mu_dprime++;
        }
        Color mu_dprime_palette[] = {RED, GREEN, BLUE, ORANGE, PURPLE, BROWN};
        if (data->smoothing_mu_double_prime_data && data->smoothing_mu_double_prime_row_count > 0) {
            static char mu_dprime_legend_buffs[NUM_SMOOTHING_COEFFS][20];
            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_mu_dprime >= MAX_SERIES_MU_DPRIME_TAB) break;
                double* smoothed_y = (double*)malloc(data->smoothing_mu_double_prime_row_count * sizeof(double));
                if (!smoothed_y) { continue; }
                temp_allocated_y_mu_dprime_smooth[c] = smoothed_y;
                for (int r = 0; r < data->smoothing_mu_double_prime_row_count; ++r) {
                    smoothed_y[r] = (data->smoothing_mu_double_prime_data[r] && c < NUM_SMOOTHING_COEFFS)
                                    ? data->smoothing_mu_double_prime_data[r][c] : NAN;
                }
                mu_dprime_graph_series_arr[actual_series_count_mu_dprime].x_values = NULL;
                mu_dprime_graph_series_arr[actual_series_count_mu_dprime].y_values = smoothed_y;
                mu_dprime_graph_series_arr[actual_series_count_mu_dprime].num_points = data->smoothing_mu_double_prime_row_count;
                mu_dprime_graph_series_arr[actual_series_count_mu_dprime].color = mu_dprime_palette[c % (sizeof(mu_dprime_palette)/sizeof(Color))];
                snprintf(mu_dprime_legend_buffs[c], sizeof(mu_dprime_legend_buffs[c]), "A=%.1f", SMOOTHING_COEFFS[c]);
                mu_dprime_graph_series_arr[actual_series_count_mu_dprime].name = mu_dprime_legend_buffs[c];
                actual_series_count_mu_dprime++;
            }
        }
        if (actual_series_count_mu_dprime > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_mu_dprime_graph_bounds,
                                        mu_dprime_graph_series_arr, actual_series_count_mu_dprime,
                                        &data->graph_states[GRAPH_ID_L1_MU_DPRIME], "M (период)", "Значение Mu''");
        }
        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_mu_dprime_smooth[c]) { free(temp_allocated_y_mu_dprime_smooth[c]); temp_allocated_y_mu_dprime_smooth[c] = NULL; }
        }
    } else {
        DrawText("Нет данных для графика Mu''", (int)smooth_mu_dprime_graph_bounds.x + 10, (int)smooth_mu_dprime_graph_bounds.y + 30, 16, DARKGRAY);
    }


    // --- Секция сглаживания для Alpha'' (Таблица) ---
    float alpha_section_x_start = tab_content_bounds.x + content_padding + smoothing_section_width + content_padding;
    Rectangle smooth_alpha_dprime_table_bounds = {
        alpha_section_x_start, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
    TraceLog(LOG_DEBUG, "DrawL1MinusE: Перед таблицей сглаживания Alpha'': smoothing_ptr=%p, rows=%d, alpha_dprime_ptr=%p, alpha_dprime_count=%d",
         (void*)data->smoothing_alpha_double_prime_data, data->smoothing_alpha_double_prime_row_count,
         (void*)data->alpha_double_prime_data, data->alpha_double_prime_data_count);

    if (data->smoothing_alpha_double_prime_data && data->smoothing_alpha_double_prime_row_count > 0 &&
        data->alpha_double_prime_data && data->alpha_double_prime_data_count > 0) { // alpha_double_prime_data_count > 0 (а не >1) для передачи в CreateL1SmoothingTable
        TableData alpha_dprime_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_alpha_double_prime_data, data->smoothing_alpha_double_prime_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->alpha_double_prime_data, data->alpha_double_prime_data_count,
            "Alpha'' (реальное)", 1); // offset 1 для alpha
        if (alpha_dprime_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &alpha_dprime_smoothing_table_display, "Сглаживание Alpha''", // Добавил заголовок
                             smooth_alpha_dprime_table_bounds, &data->l1_tables_scroll[L1_TAB_DATA_MINUS_E][2], &temp_sel_row, false);
        } else {
            DrawText("Не удалось создать таблицу сглаживания Alpha''", (int)smooth_alpha_dprime_table_bounds.x + 5, (int)smooth_alpha_dprime_table_bounds.y + 5, 16, DARKGRAY);
        }
        free_table_data_content_rl(&alpha_dprime_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания Alpha''", (int)smooth_alpha_dprime_table_bounds.x + 5, (int)smooth_alpha_dprime_table_bounds.y + 5, 16, DARKGRAY);
        if (data->smoothing_alpha_double_prime_data == NULL) TraceLog(LOG_WARNING, "DrawL1MinusE: smoothing_alpha_double_prime_data is NULL");
        if (data->smoothing_alpha_double_prime_row_count <= 0) TraceLog(LOG_WARNING, "DrawL1MinusE: smoothing_alpha_double_prime_row_count is %d", data->smoothing_alpha_double_prime_row_count);
        if (data->alpha_double_prime_data == NULL) TraceLog(LOG_WARNING, "DrawL1MinusE: alpha_double_prime_data is NULL");
        if (data->alpha_double_prime_data_count <= 0) TraceLog(LOG_WARNING, "DrawL1MinusE: alpha_double_prime_data_count is %d", data->alpha_double_prime_data_count);
    }

    // --- Секция сглаживания для Alpha'' (График) ---
    Rectangle smooth_alpha_dprime_graph_bounds = {
        alpha_section_x_start,
        current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width,
        graph_smoothing_height_alloc
    };
    // (Код для графика Alpha'' остается как в вашем предыдущем варианте)
    if (data->alpha_double_prime_data && data->alpha_double_prime_data_count > 1) {
        #define MAX_SERIES_ALPHA_DPRIME_TAB (1 + NUM_SMOOTHING_COEFFS)
        GraphSeriesData_L1 alpha_dprime_graph_series_arr[MAX_SERIES_ALPHA_DPRIME_TAB];
        int actual_series_count_alpha_dprime = 0;
        double* temp_allocated_y_alpha_dprime_smooth[NUM_SMOOTHING_COEFFS] = {NULL};
        if (actual_series_count_alpha_dprime < MAX_SERIES_ALPHA_DPRIME_TAB) {
            alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].x_values = NULL;
            alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].y_values = &data->alpha_double_prime_data[1];
            alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].num_points = data->alpha_double_prime_data_count - 1;
            alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].color = BLACK;
            alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].name = "Alpha'' (исх.)";
            actual_series_count_alpha_dprime++;
        }
        Color alpha_dprime_palette[] = {RED, GREEN, BLUE, ORANGE, PURPLE, BROWN};
        if (data->smoothing_alpha_double_prime_data && data->smoothing_alpha_double_prime_row_count > 0) {
            static char alpha_dprime_legend_buffs[NUM_SMOOTHING_COEFFS][20];
            for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
                if (actual_series_count_alpha_dprime >= MAX_SERIES_ALPHA_DPRIME_TAB) break;
                double* smoothed_y = (double*)malloc(data->smoothing_alpha_double_prime_row_count * sizeof(double));
                if (!smoothed_y) { continue; }
                temp_allocated_y_alpha_dprime_smooth[c] = smoothed_y;
                for (int r = 0; r < data->smoothing_alpha_double_prime_row_count; ++r) {
                    smoothed_y[r] = (data->smoothing_alpha_double_prime_data[r] && c < NUM_SMOOTHING_COEFFS)
                                    ? data->smoothing_alpha_double_prime_data[r][c] : NAN;
                }
                alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].x_values = NULL;
                alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].y_values = smoothed_y;
                alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].num_points = data->smoothing_alpha_double_prime_row_count;
                alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].color = alpha_dprime_palette[c % (sizeof(alpha_dprime_palette)/sizeof(Color))];
                snprintf(alpha_dprime_legend_buffs[c], sizeof(alpha_dprime_legend_buffs[c]), "A=%.1f", SMOOTHING_COEFFS[c]);
                alpha_dprime_graph_series_arr[actual_series_count_alpha_dprime].name = alpha_dprime_legend_buffs[c];
                actual_series_count_alpha_dprime++;
            }
        }
        if (actual_series_count_alpha_dprime > 0) {
            DrawMultiSeriesLineGraph_L1(data, smooth_alpha_dprime_graph_bounds,
                                        alpha_dprime_graph_series_arr, actual_series_count_alpha_dprime,
                                        &data->graph_states[GRAPH_ID_L1_ALPHA_DPRIME], "M (период)", "Значение Alpha''");
        }
        for (int c = 0; c < NUM_SMOOTHING_COEFFS; ++c) {
            if (temp_allocated_y_alpha_dprime_smooth[c]) { free(temp_allocated_y_alpha_dprime_smooth[c]); temp_allocated_y_alpha_dprime_smooth[c] = NULL; }
        }
    } else {
        DrawText("Нет данных для графика Alpha''", (int)smooth_alpha_dprime_graph_bounds.x + 10, (int)smooth_alpha_dprime_graph_bounds.y + 30, 16, DARKGRAY);
    }
}

static TableData CreateL1MinusECombinedTable(AppData_Raylib *data, double value_E) {
    TableData combined_table = {0}; // Инициализируем нулями!

    // Проверки на наличие основных данных
    if (!data || !data->table_data_main.table || data->table_data_main.row_count == 0 ||
        !data->mu_double_prime_data || data->mu_double_prime_data_count != data->table_data_main.row_count ||
        !data->alpha_double_prime_data || data->alpha_double_prime_data_count != data->table_data_main.row_count) {
        TraceLog(LOG_WARNING, "L1_MINUS_E_COMB_TABLE: Недостаточно данных для создания объединенной таблицы -E.");
        return combined_table; // Возвращаем пустую структуру
    }

    int base_rows = data->table_data_main.row_count;
    int base_data_cols = data->table_data_main.column_count;
    int total_data_cols_combined = base_data_cols + 2;
    int total_pragma_cols_combined = total_data_cols_combined + 1;

    combined_table.row_count = base_rows;
    combined_table.column_count = total_data_cols_combined; 
    combined_table.num_allocated_column_names = total_pragma_cols_combined;

    // 1. Выделение памяти и копирование имен колонок
    combined_table.column_names = (char**)calloc(combined_table.num_allocated_column_names, sizeof(char*));
    if (!combined_table.column_names) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_COMB_TABLE: Ошибка calloc для column_names.");
        return combined_table; 
    }

    // Копируем имя колонки эпохи
    if (data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names[0]) {
        combined_table.column_names[0] = strdup(data->table_data_main.column_names[0]);
    } else {
        combined_table.column_names[0] = strdup("Эпоха"); 
    }
    if (!combined_table.column_names[0]) goto cleanup_minus_e_combined_manual; // Используем новую метку

    // Копируем имена колонок X_i
    for (int i = 0; i < base_data_cols; ++i) {
        const char* base_col_name_ptr = ((i + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[i + 1])
                                   ? data->table_data_main.column_names[i + 1]
                                   : NULL; 
        char temp_base_col_name[64];
        if (base_col_name_ptr) {
            strncpy(temp_base_col_name, base_col_name_ptr, 63);
            temp_base_col_name[63] = '\0';
        } else {
            sprintf(temp_base_col_name, "X%d", i + 1);
        }
        
        char final_col_name[128];
        snprintf(final_col_name, sizeof(final_col_name), "%s(-E)", temp_base_col_name);
        combined_table.column_names[i + 1] = strdup(final_col_name);
        
        if (!combined_table.column_names[i + 1]) goto cleanup_minus_e_combined_manual;
    }
    // Добавляем имена для μ'' и α''
    combined_table.column_names[base_data_cols + 1] = strdup("Mu''");
    if (!combined_table.column_names[base_data_cols + 1]) goto cleanup_minus_e_combined_manual;
    combined_table.column_names[base_data_cols + 2] = strdup("Alpha''");
    if (!combined_table.column_names[base_data_cols + 2]) goto cleanup_minus_e_combined_manual;


    // 2. Выделение памяти и копирование эпох
    combined_table.epochs = (int*)calloc(base_rows, sizeof(int));
    if (!combined_table.epochs) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_COMB_TABLE: Ошибка calloc для epochs.");
        goto cleanup_minus_e_combined_manual; 
    }
    for (int i = 0; i < base_rows; ++i) {
        combined_table.epochs[i] = data->table_data_main.epochs[i];
    }

    // 3. Выделение памяти и заполнение таблицы данными
    combined_table.table = (double**)calloc(base_rows, sizeof(double*));
    if (!combined_table.table) {
        TraceLog(LOG_ERROR, "L1_MINUS_E_COMB_TABLE: Ошибка calloc для table.");
        goto cleanup_minus_e_combined_manual; 
    }
    for (int i = 0; i < base_rows; ++i) {
        combined_table.table[i] = (double*)calloc(total_data_cols_combined, sizeof(double));
        if (!combined_table.table[i]) {
            TraceLog(LOG_ERROR, "L1_MINUS_E_COMB_TABLE: Ошибка calloc для table[%d].", i);
            for(int k=0; k<i; ++k) if(combined_table.table[k]) free(combined_table.table[k]);
            free(combined_table.table); combined_table.table = NULL; 
            goto cleanup_minus_e_combined_manual;
        }

        // Значения X_i + E
        if (data->table_data_main.table[i]) {
            for (int j = 0; j < base_data_cols; ++j) {
                combined_table.table[i][j] = data->table_data_main.table[i][j] - value_E;
            }
        } else {
            for (int j = 0; j < base_data_cols; ++j) combined_table.table[i][j] = NAN;
        }
        // Mu''
        combined_table.table[i][base_data_cols] = data->mu_double_prime_data[i];
        // Alpha''
        combined_table.table[i][base_data_cols + 1] = data->alpha_double_prime_data[i];
    }

    TraceLog(LOG_INFO, "L1_MINUS_E_COMB_TABLE: Временная таблица (X-E, Mu', Alpha') создана (строк: %d, колонок данных: %d).",
             combined_table.row_count, combined_table.column_count);
    return combined_table;

cleanup_minus_e_combined_manual: // Метка для ручной очистки
    TraceLog(LOG_ERROR, "L1_MINUS_E_COMB_TABLE: Ошибка при создании таблицы -E, ручная очистка.");
    if (combined_table.column_names) {
        // Используем num_allocated_column_names, так как это фактический размер выделенного массива column_names
        for (int i = 0; i < combined_table.num_allocated_column_names; ++i) { 
            if (combined_table.column_names[i]) free(combined_table.column_names[i]);
        }
        free(combined_table.column_names);
        combined_table.column_names = NULL; // Важно для free_table_data_content_rl, если она будет вызвана для частично созданной таблицы
    }
    if (combined_table.epochs) {
        free(combined_table.epochs);
        combined_table.epochs = NULL;
    }
    if (combined_table.table) { 
        for (int i = 0; i < combined_table.row_count; ++i) { // row_count здесь может быть корректным, если ошибка была в выделении строки
            if (combined_table.table[i]) free(combined_table.table[i]);
        }
        free(combined_table.table);
        combined_table.table = NULL;
    }
    // Сбрасываем счетчики, чтобы показать, что таблица невалидна
    combined_table.row_count = 0;
    combined_table.column_count = 0;
    combined_table.num_allocated_column_names = 0;
    return combined_table;
}

static void DrawLevel1Tab_Conclusion(AppData_Raylib *data, Rectangle tab_content_bounds) {
    if (!data) {
        DrawText("AppData is NULL in DrawLevel1Tab_Conclusion!", (int)tab_content_bounds.x + 10, (int)tab_content_bounds.y + 10, 18, RED);
        return;
    }

    // Заголовок вкладки
    GuiLabel((Rectangle){tab_content_bounds.x + PADDING, tab_content_bounds.y + PADDING, 450, 20}, 
             "Вкладка: Заключение"); // Общий заголовок вкладки

    float current_y_offset = tab_content_bounds.y + PADDING + 20 + PADDING; // Отступ после заголовка вкладки
    float content_padding = PADDING; // Используем PADDING из app_data_rl.h или определенный здесь

    // 1. Убедимся, что все необходимые данные рассчитаны.
    // Эти вызовы Ensure... должны быть достаточно "умными", чтобы не пересчитывать без необходимости.
    // Они подготавливают data->mu_data, data->alpha_data, data->smoothing_..., data->..._prime_..., data->..._double_prime_...
    // Порядок вызовов важен, так как некоторые Ensure... могут зависеть от результатов предыдущих.

    if (!data->db_opened || data->table_data_main.row_count == 0) {
        DrawText("Нет исходных данных. Откройте файл БД и выберите таблицу.",
                 (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, DARKGRAY);
        return;
    }

    // Базовые расчеты Mu и Alpha
    if (!data->mu_data || !data->alpha_data ||
        data->mu_data_count != data->table_data_main.row_count ||
        data->alpha_count != data->table_data_main.row_count) {
        TraceLog(LOG_DEBUG, "L1_Conclusion: Базовые Mu/Alpha не актуальны или отсутствуют, вызываем EnsureLevel1BaseCalculations.");
        if (!EnsureLevel1BaseCalculations(data)) { /* Обработка ошибки, если Ensure... вернула false */ }
    }

    // Сглаживание для базовых Mu/Alpha (нужно для Mz в таблице Заключения)
    bool initial_smoothing_needed = false;
    if (data->mu_data && (!data->smoothing_data || (data->mu_data_count > 0 && data->smoothing_row_count != (data->mu_data_count + 1)))) {
        initial_smoothing_needed = true;
    }
    // Для Alpha сглаживание не используется напрямую в таблице Заключения, но может быть нужно для графика или других вкладок
    if (data->alpha_data && data->alpha_count > 1 && (!data->smoothing_alpha_data || data->smoothing_alpha_row_count != (data->alpha_count -1 + 1))) {
        initial_smoothing_needed = true;
    }
    if (initial_smoothing_needed) {
        TraceLog(LOG_DEBUG, "L1_Conclusion: Данные сглаживания для базовых Mu/Alpha не актуальны, вызываем EnsureLevel1SmoothingCalculations.");
        if (!EnsureLevel1SmoothingCalculations(data)) { /* Обработка ошибки */ }
    }

    // Данные +E (нужны для Mz+E в таблице Заключения и для графика)
    if (data->mu_data && (!data->mu_prime_data || !data->alpha_prime_data ||
         data->mu_prime_data_count != data->table_data_main.row_count ||
         data->alpha_prime_data_count != data->table_data_main.row_count ||
         (data->mu_prime_data_count > 0 && (!data->smoothing_mu_prime_data || data->smoothing_mu_prime_row_count != (data->mu_prime_data_count + 1))) )) {
        TraceLog(LOG_DEBUG, "L1_Conclusion: Данные +E (или их сглаживание) не актуальны, вызываем EnsureLevel1PlusECalculations.");
        if (!EnsureLevel1PlusECalculations(data)) { /* Обработка ошибки */ }
    }

    // Данные -E (нужны для Mz-E в таблице Заключения и для графика)
    if (data->mu_data && (!data->mu_double_prime_data || !data->alpha_double_prime_data ||
         data->mu_double_prime_data_count != data->table_data_main.row_count ||
         data->alpha_double_prime_data_count != data->table_data_main.row_count ||
         (data->mu_double_prime_data_count > 0 && (!data->smoothing_mu_double_prime_data || data->smoothing_mu_double_prime_row_count != (data->mu_double_prime_data_count + 1))) )) {
        TraceLog(LOG_DEBUG, "L1_Conclusion: Данные -E (или их сглаживание) не актуальны, вызываем EnsureLevel1MinusECalculations.");
        if (!EnsureLevel1MinusECalculations(data)) { /* Обработка ошибки */ }
    }

    // Флаги для проверки готовности данных (для графика и таблицы)
    bool base_ok = (data->mu_data && data->alpha_data && data->mu_data_count > 0 && data->mu_data_count == data->alpha_count);
    bool plus_e_ok = (data->mu_prime_data && data->alpha_prime_data && data->mu_prime_data_count > 0 && data->mu_prime_data_count == data->alpha_prime_data_count);
    bool minus_e_ok = (data->mu_double_prime_data && data->alpha_double_prime_data && data->mu_double_prime_data_count > 0 && data->mu_double_prime_data_count == data->alpha_double_prime_data_count);
    
    bool can_draw_conclusion_graph = base_ok || plus_e_ok || minus_e_ok;
    
    // Для таблицы заключения нужны все три набора сглаженных данных (Mz, Mz+E, Mz-E)
    bool smoothing_for_table_ok =
        (data->mu_data_count == 0 || (data->smoothing_data && data->smoothing_row_count == (data->mu_data_count + 1))) &&
        (data->mu_prime_data_count == 0 || (data->smoothing_mu_prime_data && data->smoothing_mu_prime_row_count == (data->mu_prime_data_count + 1))) &&
        (data->mu_double_prime_data_count == 0 || (data->smoothing_mu_double_prime_data && data->smoothing_mu_double_prime_row_count == (data->mu_double_prime_data_count + 1)));
    bool can_draw_conclusion_table = base_ok && plus_e_ok && minus_e_ok && smoothing_for_table_ok;


    float available_width_content = tab_content_bounds.width - 2 * content_padding;
    float available_height_content = tab_content_bounds.height - (current_y_offset - (tab_content_bounds.y + PADDING)) - content_padding;


    // --- График Сравнения Alpha(Mu) ---
    float conclusion_graph_height = available_height_content * 0.45f; // Уменьшаем долю для графика, чтобы таблица поместилась
    if (conclusion_graph_height < 150 && available_height_content > 300) conclusion_graph_height = 150;
    else if (conclusion_graph_height > available_height_content - 150) conclusion_graph_height = available_height_content - 150;
    if (conclusion_graph_height < 0) conclusion_graph_height = 0;

    Rectangle conclusion_graph_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, conclusion_graph_height
    };

    if (can_draw_conclusion_graph) {
        // ... (код для подготовки серий и вызова DrawMultiSeriesLineGraph_L1, как был раньше) ...
        // Этот код не менялся и должен быть здесь
        #ifndef MAX_SERIES_CONCLUSION_GRAPH
        #define MAX_SERIES_CONCLUSION_GRAPH 3
        #endif
        GraphSeriesData_L1 conclusion_series_arr[MAX_SERIES_CONCLUSION_GRAPH];
        int actual_series_count_conc = 0;
        Color conclusion_palette[] = {RED, GREEN, BLUE};

        if (base_ok && actual_series_count_conc < MAX_SERIES_CONCLUSION_GRAPH) {
            conclusion_series_arr[actual_series_count_conc].x_values = data->mu_data;
            conclusion_series_arr[actual_series_count_conc].y_values = data->alpha_data;
            conclusion_series_arr[actual_series_count_conc].num_points = data->mu_data_count;
            conclusion_series_arr[actual_series_count_conc].color = conclusion_palette[0];
            conclusion_series_arr[actual_series_count_conc].name = "Alpha(Mu)";
            actual_series_count_conc++;
        }
        if (plus_e_ok && actual_series_count_conc < MAX_SERIES_CONCLUSION_GRAPH) {
            conclusion_series_arr[actual_series_count_conc].x_values = data->mu_prime_data;
            conclusion_series_arr[actual_series_count_conc].y_values = data->alpha_prime_data;
            conclusion_series_arr[actual_series_count_conc].num_points = data->mu_prime_data_count;
            conclusion_series_arr[actual_series_count_conc].color = conclusion_palette[1];
            conclusion_series_arr[actual_series_count_conc].name = "Alpha'(Mu') +E";
            actual_series_count_conc++;
        }
        if (minus_e_ok && actual_series_count_conc < MAX_SERIES_CONCLUSION_GRAPH) {
            conclusion_series_arr[actual_series_count_conc].x_values = data->mu_double_prime_data;
            conclusion_series_arr[actual_series_count_conc].y_values = data->alpha_double_prime_data;
            conclusion_series_arr[actual_series_count_conc].num_points = data->mu_double_prime_data_count;
            conclusion_series_arr[actual_series_count_conc].color = conclusion_palette[2];
            conclusion_series_arr[actual_series_count_conc].name = "Alpha''(Mu'') -E";
            actual_series_count_conc++;
        }

        if (actual_series_count_conc > 0) {
             DrawMultiSeriesLineGraph_L1(data, conclusion_graph_bounds,
                                        conclusion_series_arr, actual_series_count_conc,
                                        &data->graph_states[GRAPH_ID_L1_CONCLUSION_ALPHA_MU],
                                        "Значение Mu (Mz)", "Значение Alpha");
        } else {
            DrawText("Нет серий данных для графика сравнения Alpha(Mu).", (int)conclusion_graph_bounds.x + 10, (int)conclusion_graph_bounds.y + 10, 16, DARKGRAY);
        }
    } else {
         DrawText("Данные для графика Alpha(Mu) не готовы.", (int)conclusion_graph_bounds.x + 10, (int)conclusion_graph_bounds.y + 10, 16, DARKGRAY);
    }
    current_y_offset += conclusion_graph_bounds.height + content_padding;

    // --- Итоговая таблица погрешности ---
    // Заголовок для таблицы
    GuiLabel((Rectangle){tab_content_bounds.x + content_padding, current_y_offset, available_width_content, 20}, 
             "Итоговая таблица погрешности");
    current_y_offset += 20 + content_padding / 2.0f;

    float conclusion_table_height = tab_content_bounds.y + tab_content_bounds.height - current_y_offset - content_padding;
    if (conclusion_table_height < 100 && available_height_content > conclusion_graph_height + 100 + content_padding + 20) conclusion_table_height = 100;
    if (conclusion_table_height < 0) conclusion_table_height = 0;

    Rectangle area_for_conclusion_table = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, conclusion_table_height
    };

    if (can_draw_conclusion_table) {
        int conclusion_table_row_count = 0;
        // Вызываем нашу новую функцию подготовки данных
        FinalAccuracyTableRow *conclusion_table_data = PrepareL1FinalAccuracyTableData(data, &conclusion_table_row_count);

        if (conclusion_table_data && conclusion_table_row_count > 0) {
            int temp_sel_row_conc = data->l1_conclusion_table_selected_row; // Используем сохраненное состояние
            
            // Вызываем нашу новую функцию отрисовки
            DrawL1FinalAccuracyTable(data,
                                     conclusion_table_data,
                                     conclusion_table_row_count,
                                     area_for_conclusion_table,
                                     &data->l1_tables_scroll[L1_TAB_CONCLUSION][0], // Скролл для этой таблицы
                                     &temp_sel_row_conc);
            
            data->l1_conclusion_table_selected_row = temp_sel_row_conc; // Сохраняем измененное состояние

            // ОСВОБОЖДАЕМ ПАМЯТЬ, выделенную PrepareL1FinalAccuracyTableData
            free(conclusion_table_data);
            conclusion_table_data = NULL; 
        } else {
            DrawText("Не удалось подготовить данные для таблицы Заключения.", 
                     (int)area_for_conclusion_table.x + 5, 
                     (int)area_for_conclusion_table.y + 5, 18, RED);
            if (conclusion_table_data) { // Если строки 0, но указатель не NULL
                free(conclusion_table_data);
                conclusion_table_data = NULL;
            }
        }
    } else {
         DrawText("Данные для Итоговой таблицы погрешности не готовы.", 
                  (int)area_for_conclusion_table.x + 5, 
                  (int)area_for_conclusion_table.y + 5, 18, DARKGRAY);
    }
}

static FinalAccuracyTableRow* PrepareL1FinalAccuracyTableData(AppData_Raylib *data, int *out_row_count) {
    if (!data || !out_row_count) {
        if (out_row_count) *out_row_count = 0;
        TraceLog(LOG_ERROR, "PrepareL1FinalAccuracy: Invalid input parameters (data or out_row_count is NULL).");
        return NULL;
    }
    *out_row_count = 0; // Инициализация на случай раннего выхода

    // 1. Проверка наличия базовых данных mu_data.
    //    Предполагается, что EnsureLevel1BaseCalculations уже была вызвана в DrawLevel1Tab_Conclusion.
    if (!data->mu_data || data->mu_data_count == 0) {
        TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Отсутствуют базовые mu_data. Невозможно подготовить таблицу.");
        return NULL;
    }

    // 2. Определение индекса для коэффициента сглаживания A=0.9
    int idx_A09 = -1;
    if (NUM_SMOOTHING_COEFFS > 0) { // NUM_SMOOTHING_COEFFS определен в этом файле
        for (int i = 0; i < NUM_SMOOTHING_COEFFS; ++i) {
            if (fabs(SMOOTHING_COEFFS[i] - 0.9) < 1e-6) { // SMOOTHING_COEFFS определен в этом файле
                idx_A09 = i;
                break;
            }
        }
    }
    if (idx_A09 == -1) {
        TraceLog(LOG_ERROR, "PrepareL1FinalAccuracy: Коэффициент сглаживания A=0.9 не найден в SMOOTHING_COEFFS!");
        return NULL;
    }

    // 3. Проверка наличия всех необходимых данных сглаживания.
    //    Предполагается, что EnsureLevel1SmoothingCalculations, EnsureLevel1PlusECalculations,
    //    и EnsureLevel1MinusECalculations уже были вызваны в DrawLevel1Tab_Conclusion.
    bool base_smooth_ok = (data->smoothing_data && 
                           data->smoothing_row_count == (data->mu_data_count + 1) && 
                           data->smoothing_data[0] != NULL); // Проверяем первую строку сглаживания
    
    bool prime_smooth_ok = (data->mu_prime_data && // Убедимся, что mu_prime_data вообще есть
                            data->mu_prime_data_count == data->mu_data_count && // Их количество должно совпадать с базовыми mu
                            data->smoothing_mu_prime_data && 
                            data->smoothing_mu_prime_row_count == (data->mu_prime_data_count + 1) && 
                            data->smoothing_mu_prime_data[0] != NULL);

    bool dprime_smooth_ok = (data->mu_double_prime_data && // Аналогично для mu_double_prime_data
                             data->mu_double_prime_data_count == data->mu_data_count &&
                             data->smoothing_mu_double_prime_data && 
                             data->smoothing_mu_double_prime_row_count == (data->mu_double_prime_data_count + 1) && 
                             data->smoothing_mu_double_prime_data[0] != NULL);

    if (!base_smooth_ok || !prime_smooth_ok || !dprime_smooth_ok) {
        TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Отсутствуют или некорректны необходимые данные сглаживания (base_smooth:%d, prime_smooth:%d, dprime_smooth:%d).", 
                 base_smooth_ok, prime_smooth_ok, dprime_smooth_ok);
        return NULL;
    }

    // 4. Определение количества строк и выделение памяти
    int num_data_rows = data->mu_data_count; // Количество строк t0...t(N-1)
    int total_rows = num_data_rows + 1;      // +1 для строки "прогноз"

    FinalAccuracyTableRow *table_rows = (FinalAccuracyTableRow*)calloc(total_rows, sizeof(FinalAccuracyTableRow));
    if (!table_rows) {
        TraceLog(LOG_ERROR, "PrepareL1FinalAccuracy: Ошибка calloc для table_rows (запрошено %d строк).", total_rows);
        return NULL; 
    }

    // 5. Заполнение данных
    double mz_t0 = NAN; // Для хранения Mz на t0 для расчета L

    for (int i = 0; i < total_rows; ++i) {
        FinalAccuracyTableRow *currentRow = &table_rows[i];

        // --- Заполнение cycle_epoch ---
        if (i < num_data_rows) { // Строки t0...t(N-1)
            currentRow->cycle_epoch = i;
        } else { // Строка "прогноз"
            currentRow->cycle_epoch = -1;
        }

        // --- Извлечение Mz+E, Mz, Mz-E ---
        int smooth_data_idx = (i < num_data_rows) ? i : num_data_rows; // Индекс для массивов сглаживания

        // Mz+E (из data->smoothing_mu_prime_data)
        // Дополнительная проверка на существование строки в массиве сглаживания
        if (smooth_data_idx < data->smoothing_mu_prime_row_count && data->smoothing_mu_prime_data[smooth_data_idx] != NULL) {
            currentRow->mz_plus_e = data->smoothing_mu_prime_data[smooth_data_idx][idx_A09];
        } else {
            currentRow->mz_plus_e = NAN; 
            TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Отсутствуют данные сглаживания Mz+E для строки %d (smooth_idx %d)", i, smooth_data_idx);
        }

        // Mz (из data->smoothing_data)
        if (smooth_data_idx < data->smoothing_row_count && data->smoothing_data[smooth_data_idx] != NULL) {
            currentRow->mz = data->smoothing_data[smooth_data_idx][idx_A09];
        } else {
            currentRow->mz = NAN;
            TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Отсутствуют данные сглаживания Mz для строки %d (smooth_idx %d)", i, smooth_data_idx);
        }

        // Mz-E (из data->smoothing_mu_double_prime_data)
        if (smooth_data_idx < data->smoothing_mu_double_prime_row_count && data->smoothing_mu_double_prime_data[smooth_data_idx] != NULL) {
            currentRow->mz_minus_e = data->smoothing_mu_double_prime_data[smooth_data_idx][idx_A09];
        } else {
            currentRow->mz_minus_e = NAN;
            TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Отсутствуют данные сглаживания Mz-E для строки %d (smooth_idx %d)", i, smooth_data_idx);
        }

        // Сохраняем Mz для t0 (если это строка t0 и Mz валидно)
        if (i == 0 && !isnan(currentRow->mz)) {
            mz_t0 = currentRow->mz;
        } else if (i == 0 && isnan(currentRow->mz)) {
            // Если Mz для t0 невалидно, L для других строк не сможет быть вычислено корректно.
            // Это должно быть обработано проверками выше, но на всякий случай.
            TraceLog(LOG_WARNING, "PrepareL1FinalAccuracy: Mz для t0 является NAN, L будет некорректным.");
        }


        // --- Расчет R ---
        if (!isnan(currentRow->mz_plus_e) && !isnan(currentRow->mz_minus_e)) {
            currentRow->r_value = fabs(currentRow->mz_plus_e - currentRow->mz_minus_e) / 2.0;
        } else {
            currentRow->r_value = NAN;
        }

        // --- Расчет L ---
        if (i == 0) { // Строка t0
            currentRow->l_value = 0.0;
        } else if (!isnan(currentRow->mz) && !isnan(mz_t0)) { // mz_t0 должно быть валидным
            currentRow->l_value = fabs(currentRow->mz - mz_t0);
        } else {
            currentRow->l_value = NAN;
        }

        // --- Расчет Статуса ---
        // Статус вычисляется, только если R и L являются корректными числами
        if (!isnan(currentRow->r_value) && !isnan(currentRow->l_value)) {
            double epsilon = 1e-9; // Для сравнения double
            if (fabs(currentRow->r_value - currentRow->l_value) < epsilon) {
                currentRow->status = '0';
            } else if (currentRow->r_value > currentRow->l_value) {
                currentRow->status = '+';
            } else { // r_value < l_value
                currentRow->status = '-';
            }
        } else {
            currentRow->status = ' '; // Пробел, если R или L - NAN (статус не определен)
                                      // Или можно выбрать другой символ по умолчанию, например '?'
        }
    }

    *out_row_count = total_rows;
    TraceLog(LOG_INFO, "PrepareL1FinalAccuracy: Данные для таблицы Заключения подготовлены (%d строк).", total_rows);
    return table_rows;
}

static void DrawL1FinalAccuracyTable(AppData_Raylib *data, 
                                     FinalAccuracyTableRow *accuracy_table_data, 
                                     int row_count, 
                                     Rectangle panel_bounds, 
                                     Vector2 *scroll, 
                                     int *selectedRow) {
    if (!data || !scroll || !selectedRow) {
        TraceLog(LOG_ERROR, "DrawL1FinalAccuracyTable: Invalid core pointers (data, scroll, or selectedRow is NULL).");
        return;
    }
    if (!accuracy_table_data || row_count == 0) {
        TraceLog(LOG_INFO, "DrawL1FinalAccuracyTable: Нет данных для отрисовки.");
        // Можно нарисовать сообщение внутри panel_bounds, если это нужно
        // DrawText("Нет данных для таблицы Заключения.", (int)panel_bounds.x + PADDING, (int)panel_bounds.y + PADDING, 18, GRAY);
        return;
    }

    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float font_size_cell = GuiGetStyle(DEFAULT, TEXT_SIZE); // Используем общий размер текста по умолчанию
    float spacing_cell = GuiGetStyle(DEFAULT, TEXT_SPACING);
    float text_padding_x = 5.0f; // Отступ текста от левого/правого края ячейки

    // Ширина колонок
    const float col_width_cycle = TABLE_EPOCH_COL_WIDTH;
    const float col_width_mz_plus_e = TABLE_DATA_COL_WIDTH;
    const float col_width_mz = TABLE_DATA_COL_WIDTH;
    const float col_width_mz_minus_e = TABLE_DATA_COL_WIDTH;
    const float col_width_r = 70.0f;
    const float col_width_l = TABLE_DATA_COL_WIDTH;
    const float col_width_status = 60.0f;

    const char* headers[] = {"Цикл", "Mz+E", "Mz", "Mz-E", "R", "L", "статус"};
    const float col_widths[] = {col_width_cycle, col_width_mz_plus_e, col_width_mz, col_width_mz_minus_e, 
                                col_width_r, col_width_l, col_width_status};
    int num_display_cols = sizeof(headers) / sizeof(headers[0]);

    Rectangle contentRect = {0};
    for (int i = 0; i < num_display_cols; ++i) contentRect.width += col_widths[i];
    contentRect.height = TABLE_HEADER_HEIGHT + (float)row_count * TABLE_ROW_HEIGHT;

    Rectangle viewArea = {0};
    GuiScrollPanel(panel_bounds, NULL, contentRect, scroll, &viewArea);

    BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

    float current_col_offset_x_header = 0;
    Color header_bg_color = GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL));
    Color header_border_color = GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL));
    Color header_text_color = GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));
    int border_width = GuiGetStyle(DEFAULT, BORDER_WIDTH);
    float font_size_header = GuiGetStyle(DEFAULT, TEXT_SIZE);
    float spacing_header = GuiGetStyle(DEFAULT, TEXT_SPACING);

    // --- ОТРИСОВКА ЗАГОЛОВКОВ (используем raylib) ---
    for (int c = 0; c < num_display_cols; c++) {
        Rectangle headerScr = { 
            viewArea.x + current_col_offset_x_header + scroll->x, 
            viewArea.y + scroll->y, 
            col_widths[c], 
            TABLE_HEADER_HEIGHT 
        };
        DrawRectangleRec(headerScr, header_bg_color);
        if (border_width > 0) DrawRectangleLinesEx(headerScr, border_width, header_border_color);
        
        Vector2 header_text_size = MeasureTextEx(current_font, headers[c], font_size_header, spacing_header);
        DrawTextEx(current_font, headers[c], 
                   (Vector2){headerScr.x + (headerScr.width - header_text_size.x)/2.0f, 
                              headerScr.y + (headerScr.height - header_text_size.y)/2.0f}, 
                   font_size_header, spacing_header, header_text_color);
        current_col_offset_x_header += col_widths[c];
    }

    // --- ОТРИСОВКА СТРОК (используем raylib) ---
    for (int r_idx = 0; r_idx < row_count; r_idx++) {
        FinalAccuracyTableRow currentRowData = accuracy_table_data[r_idx];
        float current_col_offset_x_row = 0;
        float screen_y_current_row = viewArea.y + TABLE_HEADER_HEIGHT + (float)r_idx * TABLE_ROW_HEIGHT + scroll->y;

        if (screen_y_current_row + TABLE_ROW_HEIGHT < viewArea.y || screen_y_current_row > viewArea.y + viewArea.height) {
            continue;
        }

        Color row_bg_color = GetColor((*selectedRow == r_idx) ? GuiGetStyle(LISTVIEW, BASE_COLOR_FOCUSED) : GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL));
        if (r_idx % 2 != 0 && *selectedRow != r_idx) {
            row_bg_color = ColorAlphaBlend(row_bg_color, GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_DISABLED)), Fade(WHITE, 0.1f));
        }
        Color row_text_color = GetColor((*selectedRow == r_idx) ? GuiGetStyle(LISTVIEW, TEXT_COLOR_FOCUSED) : GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));
        char buffer[TEXT_FIELD_BUFFER_SIZE];
        Vector2 text_pos;
        Vector2 text_s; // Для MeasureTextEx

        // Функция для отрисовки текста в ячейке (можно вынести отдельно, если нужно больше настроек)
        // alignment: 0 - left, 1 - center, 2 - right
        void DrawCellText(const char* text, Rectangle cell, Color color, int alignment) {
            text_s = MeasureTextEx(current_font, text, font_size_cell, spacing_cell);
            text_pos.y = cell.y + (cell.height - text_s.y) / 2.0f;
            if (alignment == 0) { // left
                text_pos.x = cell.x + text_padding_x;
            } else if (alignment == 1) { // center
                text_pos.x = cell.x + (cell.width - text_s.x) / 2.0f;
            } else { // right
                text_pos.x = cell.x + cell.width - text_s.x - text_padding_x;
            }
            DrawTextEx(current_font, text, text_pos, font_size_cell, spacing_cell, color);
        }

        // 1. Колонка "Цикл"
        Rectangle cellRect = { viewArea.x + current_col_offset_x_row + scroll->x, screen_y_current_row, col_widths[0], TABLE_ROW_HEIGHT };
        DrawRectangleRec(cellRect, row_bg_color);
        if (currentRowData.cycle_epoch == -1) strcpy(buffer, "прогноз");
        else sprintf(buffer, "t%d", currentRowData.cycle_epoch);
        DrawCellText(buffer, cellRect, row_text_color, 1); // Центрируем "Цикл"
        current_col_offset_x_row += col_widths[0];

        // 2. Колонка "Mz+E"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[1];
        DrawRectangleRec(cellRect, row_bg_color);
        if (isnan(currentRowData.mz_plus_e)) strcpy(buffer, "N/A");
        else sprintf(buffer, "%.7f", currentRowData.mz_plus_e);
        DrawCellText(buffer, cellRect, row_text_color, 2); // Выравнивание по правому краю для чисел
        current_col_offset_x_row += col_widths[1];

        // 3. Колонка "Mz"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[2];
        DrawRectangleRec(cellRect, row_bg_color);
        if (isnan(currentRowData.mz)) strcpy(buffer, "N/A");
        else sprintf(buffer, "%.4f", currentRowData.mz);
        DrawCellText(buffer, cellRect, row_text_color, 2);
        current_col_offset_x_row += col_widths[2];

        // 4. Колонка "Mz-E"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[3];
        DrawRectangleRec(cellRect, row_bg_color);
        if (isnan(currentRowData.mz_minus_e)) strcpy(buffer, "N/A");
        else sprintf(buffer, "%.4f", currentRowData.mz_minus_e);
        DrawCellText(buffer, cellRect, row_text_color, 2);
        current_col_offset_x_row += col_widths[3];

        // 5. Колонка "R"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[4];
        DrawRectangleRec(cellRect, row_bg_color);
        if (isnan(currentRowData.r_value)) strcpy(buffer, "N/A");
        else sprintf(buffer, "%.5f", currentRowData.r_value);
        DrawCellText(buffer, cellRect, row_text_color, 2);
        current_col_offset_x_row += col_widths[4];

        // 6. Колонка "L"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[5];
        DrawRectangleRec(cellRect, row_bg_color);
        if (isnan(currentRowData.l_value)) strcpy(buffer, "N/A");
        else {
            if (currentRowData.cycle_epoch == 0 && currentRowData.l_value == 0.0) sprintf(buffer, "0");
            else {
                double abs_val_l = fabs(currentRowData.l_value);
                if (abs_val_l > 0 && (abs_val_l < 0.0001 || abs_val_l >= 100000.0)) sprintf(buffer, "%.2E", currentRowData.l_value);
                else sprintf(buffer, "%.6f", currentRowData.l_value);
            }
        }
        DrawCellText(buffer, cellRect, row_text_color, 2);
        current_col_offset_x_row += col_widths[5];

        // 7. Колонка "статус"
        cellRect.x = viewArea.x + current_col_offset_x_row + scroll->x; cellRect.width = col_widths[6];
        DrawRectangleRec(cellRect, row_bg_color);
        if (currentRowData.status == ' ' || currentRowData.status == '\0') strcpy(buffer, "");
        else sprintf(buffer, "%c", currentRowData.status);
        DrawCellText(buffer, cellRect, row_text_color, 1); // Центрируем статус
        // current_col_offset_x_row += col_widths[6]; // Не нужно для последней колонки

        // Логика выбора строки
        Rectangle row_rect_on_screen = {
            viewArea.x + scroll->x, screen_y_current_row, contentRect.width, TABLE_ROW_HEIGHT
        };
        if (!data->show_main_edit_dialog && !data->show_values_edit_dialog) { // Проверка на активные диалоги
            if (CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointRec(GetMousePosition(), row_rect_on_screen) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                     *selectedRow = r_idx;
                     TraceLog(LOG_INFO, "DrawL1FinalAccuracyTable: Выбрана строка %d (Цикл: %s)", r_idx, 
                              (currentRowData.cycle_epoch == -1) ? "прогноз" : TextFormat("t%d", currentRowData.cycle_epoch) );
                }
            }
        }
    }
    EndScissorMode();
}

void DrawMultiSeriesLineGraph_L1(
    AppData_Raylib *data,
    Rectangle plot_bounds,
    GraphSeriesData_L1 *series_array,
    int num_series,
    GraphDisplayState *graph_state,
    const char* x_axis_label_text,
    const char* y_axis_label_text
) {
    if (!data || !graph_state) {
        if (plot_bounds.width > 1 && plot_bounds.height > 1) DrawText("Graph Error: Invalid params", (int)plot_bounds.x + 5, (int)plot_bounds.y + 5, 10, RED);
        return;
    }
    if (num_series > 0 && !series_array) {
         DrawText("Graph Error: No series data", (int)plot_bounds.x + 5, (int)plot_bounds.y + 5, 10, RED);
         return;
    }

    // Уменьшенные отступы для увеличения области графика
    float padding_left = 30;
    float padding_bottom = 20;
    float padding_top = 10;
    bool has_legend = false;
    if (num_series > 0) {
        for (int s = 0; s < num_series; ++s) {
            if (series_array[s].name && series_array[s].name[0] != '\0') { has_legend = true; break; }
        }
    }
    float actual_padding_right = has_legend ? 90.0f : 10.0f;

    Rectangle graph_area = {
        plot_bounds.x + padding_left,
        plot_bounds.y + padding_top,
        plot_bounds.width - padding_left - actual_padding_right,
        plot_bounds.height - padding_top - padding_bottom
    };

    if (graph_area.width <= 10 || graph_area.height <= 10) {
        DrawText("Graph area too small", (int)plot_bounds.x + 5, (int)plot_bounds.y + 5, 10, RED);
        return;
    }

    // --- Логика взаимодействия с мышью (панорамирование и масштабирование) ---
    if (CheckCollisionPointRec(GetMousePosition(), graph_area)) {
        float wheel_move = GetMouseWheelMove();
        if (wheel_move != 0) {
            // Только если масштабирование действительно меняет масштаб (защита от scale = 0)
            if (fabsf(graph_state->scale.x) > 1e-7 && fabsf(graph_state->scale.y) > 1e-7) {
                graph_state->is_active = true; // Пользовательское взаимодействие, отключаем авто-масштаб
                Vector2 mouse_pos_in_graph = { GetMousePosition().x - graph_area.x, GetMousePosition().y - graph_area.y };
                double mouse_data_x = graph_state->offset.x + mouse_pos_in_graph.x / graph_state->scale.x;
                double mouse_data_y = graph_state->offset.y + (graph_area.height - mouse_pos_in_graph.y) / graph_state->scale.y;
                float scale_factor = (wheel_move > 0) ? 1.1f : 1.0f / 1.1f;
                graph_state->scale.x *= scale_factor;
                graph_state->scale.y *= scale_factor;
                float min_scale = 0.0001f; // Позволяем большее отдаление
                if (fabsf(graph_state->scale.x) < min_scale) graph_state->scale.x = (graph_state->scale.x >= 0 ? min_scale : -min_scale);
                if (fabsf(graph_state->scale.y) < min_scale) graph_state->scale.y = (graph_state->scale.y >= 0 ? min_scale : -min_scale);
                graph_state->offset.x = (float)(mouse_data_x - mouse_pos_in_graph.x / graph_state->scale.x);
                graph_state->offset.y = (float)(mouse_data_y - (graph_area.height - mouse_pos_in_graph.y) / graph_state->scale.y);
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            graph_state->is_panning = true;
            graph_state->pan_start_mouse_pos = GetMousePosition();
            graph_state->pan_start_offset = graph_state->offset;
            graph_state->is_active = true; // Пользовательское взаимодействие
        }
    }

    if (graph_state->is_panning) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            if (fabsf(graph_state->scale.x) > 1e-7 && fabsf(graph_state->scale.y) > 1e-7) { // Защита от деления на ноль
                graph_state->offset.x = graph_state->pan_start_offset.x - (GetMousePosition().x - graph_state->pan_start_mouse_pos.x) / graph_state->scale.x;
                graph_state->offset.y = graph_state->pan_start_offset.y + (GetMousePosition().y - graph_state->pan_start_mouse_pos.y) / graph_state->scale.y; // Y инвертирован
            }
        } else {
            graph_state->is_panning = false;
        }
    }
    // --- Конец логики взаимодействия с мышью ---

    if (!graph_state->is_active && num_series > 0) {
        double overall_x_min = 0, overall_x_max = 0, overall_y_min = 0, overall_y_max = 0;
        bool first_valid_point_overall = false;
        int max_points_for_indexed_x = 0;
        for (int s = 0; s < num_series; ++s) {
            if (!series_array[s].y_values || series_array[s].num_points == 0) continue;
            if (!series_array[s].x_values && series_array[s].num_points > max_points_for_indexed_x) {
                max_points_for_indexed_x = series_array[s].num_points;
            }
            for (int i = 0; i < series_array[s].num_points; ++i) {
                double current_x_val = series_array[s].x_values ? series_array[s].x_values[i] : (double)i;
                double current_y_val = series_array[s].y_values[i];
                if (isnan(current_x_val) || isnan(current_y_val) || isinf(current_x_val) || isinf(current_y_val)) continue;
                if (!first_valid_point_overall) {
                    overall_x_min = current_x_val; overall_x_max = current_x_val;
                    overall_y_min = current_y_val; overall_y_max = current_y_val;
                    first_valid_point_overall = true;
                } else {
                    if (current_x_val < overall_x_min) overall_x_min = current_x_val;
                    if (current_x_val > overall_x_max) overall_x_max = current_x_val;
                    if (current_y_val < overall_y_min) overall_y_min = current_y_val;
                    if (current_y_val > overall_y_max) overall_y_max = current_y_val;
                }
            }
        }
        if (!first_valid_point_overall) {
            overall_x_min = 0;
            overall_x_max = (max_points_for_indexed_x > 1) ? (double)max_points_for_indexed_x - 1.0 : 1.0;
            if (max_points_for_indexed_x == 1) overall_x_max = 0; // Если одна точка по индексу 0
            overall_y_min = 0; overall_y_max = 1.0;
        }

        // Специальная обработка для графика Заключения (ось Y с 0)
        if (graph_state == &data->graph_states[GRAPH_ID_L1_CONCLUSION_ALPHA_MU]) {
            if (overall_y_min > 0.0) overall_y_min = 0.0;
            if (fabs(overall_y_max) > 1e-9) overall_y_max *= 1.05f; // 5% отступ сверху
            else if (fabs(overall_y_max) < 1e-9 && fabs(overall_y_min) < 1e-9 && first_valid_point_overall) {
                 // Если все точки на нуле или очень близки к нулю, дадим небольшой диапазон
                 overall_y_max = (overall_y_max == 0.0) ? 0.00001 : overall_y_max * (overall_y_max > 0 ? 1.5 : 0.5) + 0.00001;
                 if (overall_y_min == 0.0 && overall_y_max < 1e-9) overall_y_max = 0.00001; // Гарантируем положительный диапазон
            }
        }

        graph_state->offset.x = (float)overall_x_min;
        double x_range = overall_x_max - overall_x_min;
        if (fabs(x_range) < 1e-9) { // Если диапазон очень мал или 0
            double padding = (fabs(overall_x_min) > 1e-9) ? fabs(overall_x_min * 0.1) : 0.5; // 10% или 0.5
            if (padding < 1e-9) padding = 0.5;
            graph_state->offset.x = (float)(overall_x_min - padding); x_range = 2 * padding;
        }
        graph_state->scale.x = (graph_area.width > 1e-9 && fabs(x_range) > 1e-9) ? graph_area.width / (float)x_range : 1.0f;

        graph_state->offset.y = (float)overall_y_min;
        double y_range = overall_y_max - overall_y_min;
        if (fabs(y_range) < 1e-9) {
            double padding = (fabs(overall_y_min) > 1e-9) ? fabs(overall_y_min * 0.1) : 0.5;
            if (padding < 1e-9) padding = 0.5;
            graph_state->offset.y = (float)(overall_y_min - padding); y_range = 2 * padding;
        }
        graph_state->scale.y = (graph_area.height > 1e-9 && fabs(y_range) > 1e-9) ? graph_area.height / (float)y_range : 1.0f;
        graph_state->is_active = true; // Помечаем, что состояние инициализировано
    } else if (!graph_state->is_active && num_series == 0) { // Пустой график
        graph_state->offset = (Vector2){0.0f, 0.0f};
        graph_state->scale = (Vector2){1.0f, 1.0f};
        graph_state->is_active = true;
    }

    // Отрисовка фона, рамки, осей
    DrawRectangleRec(plot_bounds, GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
    DrawRectangleRec(graph_area, WHITE);

    // Горизонтальная сетка
    if (graph_state->is_active && fabsf(graph_state->scale.y) > 1e-7) {
        int num_grid_lines_y = 5;
        double data_y_view_min = graph_state->offset.y;
        double data_y_view_max = graph_state->offset.y + graph_area.height / graph_state->scale.y;
        for (int i = 0; i <= num_grid_lines_y; ++i) {
            double data_y_level = data_y_view_min + (data_y_view_max - data_y_view_min) * ((double)i / num_grid_lines_y);
            float screen_y_grid = (graph_area.y + graph_area.height) - (float)((data_y_level - graph_state->offset.y) * graph_state->scale.y);
            if (screen_y_grid >= graph_area.y && screen_y_grid <= graph_area.y + graph_area.height + 0.5f) { // +0.5f для учета толщины линии
                DrawLineEx((Vector2){graph_area.x, screen_y_grid}, (Vector2){graph_area.x + graph_area.width, screen_y_grid}, 1.0f, LIGHTGRAY);
            }
        }
    }
    DrawRectangleLinesEx(graph_area, 1, DARKGRAY); // Рамка после сетки

    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float label_font_size = 12.0f; float label_spacing = 1.0f;
    if (x_axis_label_text && x_axis_label_text[0] != '\0') {
        Vector2 x_label_size = MeasureTextEx(current_font, x_axis_label_text, label_font_size, label_spacing);
        DrawTextEx(current_font, x_axis_label_text,
                   (Vector2){graph_area.x + (graph_area.width - x_label_size.x) / 2.0f,
                              plot_bounds.y + plot_bounds.height - padding_bottom + (padding_bottom - x_label_size.y)/2.0f + 5},
                   label_font_size, label_spacing, DARKGRAY);
    }
    if (y_axis_label_text && y_axis_label_text[0] != '\0') {
        Vector2 y_label_size = MeasureTextEx(current_font, y_axis_label_text, label_font_size, label_spacing);
         DrawTextPro(current_font, y_axis_label_text,
                    (Vector2){plot_bounds.x + (padding_left - y_label_size.y)/2.0f - 5,
                               graph_area.y + (graph_area.height + y_label_size.x) / 2.0f},
                    (Vector2){0,0}, -90, label_font_size, label_spacing, DARKGRAY);
    }

    if (fabsf(graph_state->scale.x) < 1e-7 || fabsf(graph_state->scale.y) < 1e-7) { // Проверка на почти нулевой масштаб
        DrawText("Invalid scale for drawing series", (int)graph_area.x + 10, (int)graph_area.y + 10, 10, RED);
        return;
    }

    BeginScissorMode((int)graph_area.x, (int)graph_area.y, (int)graph_area.width, (int)graph_area.height);
    for (int s = 0; s < num_series; ++s) {
        GraphSeriesData_L1 current_series = series_array[s];
        if (!current_series.y_values || current_series.num_points < 1) continue;
        if (current_series.num_points == 1) {
            double x_val_single = current_series.x_values ? current_series.x_values[0] : 0.0;
            if (isnan(x_val_single) || isnan(current_series.y_values[0]) || isinf(x_val_single) || isinf(current_series.y_values[0])) continue;
            float screen_x_single = graph_area.x + (float)((x_val_single - graph_state->offset.x) * graph_state->scale.x);
            float screen_y_single = (graph_area.y + graph_area.height) - (float)((current_series.y_values[0] - graph_state->offset.y) * graph_state->scale.y);
            if (isnormal(screen_x_single) && isnormal(screen_y_single)) {
                DrawCircleV((Vector2){screen_x_single, screen_y_single}, 4.0f, current_series.color);
                DrawCircleLines(screen_x_single, screen_y_single, 4.0f, BLACK);
            }
        } else {
            Vector2 prev_point_screen = {0};
            bool first_valid_segment_point = false;
            for (int i = 0; i < current_series.num_points; ++i) {
                double current_x_val = current_series.x_values ? current_series.x_values[i] : (double)i;
                double current_y_val = current_series.y_values[i];
                if (isnan(current_x_val) || isnan(current_y_val) || isinf(current_x_val) || isinf(current_y_val)) {
                    first_valid_segment_point = false; continue;
                }
                float screen_x = graph_area.x + (float)((current_x_val - graph_state->offset.x) * graph_state->scale.x);
                float screen_y = (graph_area.y + graph_area.height) - (float)((current_y_val - graph_state->offset.y) * graph_state->scale.y);
                Vector2 current_point_screen = {screen_x, screen_y};

                if (isnormal(current_point_screen.x) && isnormal(current_point_screen.y)) { // Рисуем маркер, если точка валидна
                    DrawCircleV(current_point_screen, 4.0f, current_series.color);
                    DrawCircleLines(current_point_screen.x, current_point_screen.y, 4.0f, BLACK);
                }

                if (first_valid_segment_point) { // first_valid_segment_point означает, что prev_point_screen валидна
                    if (isnormal(current_point_screen.x) && isnormal(current_point_screen.y)) { // И текущая точка валидна
                         DrawLineEx(prev_point_screen, current_point_screen, 2.5f, current_series.color);
                    } else { first_valid_segment_point = false; } // Текущая точка невалидна, разрываем линию
                }
                prev_point_screen = current_point_screen;
                first_valid_segment_point = isnormal(current_point_screen.x) && isnormal(current_point_screen.y);
            }
        }
    }
    EndScissorMode();

    if (has_legend) {
        float legend_x_start = graph_area.x + graph_area.width + 10;
        float legend_y_start = graph_area.y;
        float legend_item_height = 15.0f;
        float legend_color_box_size = 10.0f;
        float legend_text_padding = 5.0f;
        float legend_font_size = 10.0f;
        for (int s = 0; s < num_series; ++s) {
            if (series_array[s].name && series_array[s].name[0] != '\0') {
                float current_legend_y = legend_y_start + s * legend_item_height;
                if (current_legend_y + legend_item_height > plot_bounds.y + plot_bounds.height - padding_bottom) break;
                DrawRectangleRec((Rectangle){legend_x_start, current_legend_y + (legend_item_height - legend_color_box_size)/2.0f, legend_color_box_size, legend_color_box_size}, series_array[s].color);
                DrawTextEx(current_font, series_array[s].name,
                           (Vector2){legend_x_start + legend_color_box_size + legend_text_padding, current_legend_y + (legend_item_height - legend_font_size)/2.0f},
                           legend_font_size, label_spacing, DARKGRAY);
            }
        }
    }
}