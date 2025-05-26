// level1_analysis_rl.c
#include "level1_analysis_rl.h"
#include "app_data_rl.h"
#include "raylib.h"
// #define RAYGUI_IMPLEMENTATION // Убедитесь, что это только в main_rl.c
#include "raygui.h"
#include "sqlite_utils_rl.h" // Для TableData и DrawDataTable_RL
#include "calculations_rl.h" // Для compute_... функций

#include <stdio.h>   // Для printf, fopen, fclose, fprintf, va_list и т.д. (если используется логирование)
#include <stdlib.h>  // Для malloc, calloc, free, strtod, rand, srand
#include <string.h>  // Для memset, memcpy, strcpy, strcat, strlen, strdup
#include <math.h>    // Для NAN, fabs, acos, sqrt и т.д.
#include <stdbool.h> // Для bool (хотя часто включается через другие заголовки Raylib/AppData)

// --- Прототипы внутренних статических функций для каждой вкладки ---
static void DrawLevel1Tab_InitialData(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataPlusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataMinusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_Conclusion(AppData_Raylib *data, Rectangle tab_content_bounds);

static bool EnsureLevel1BaseCalculations(AppData_Raylib *data);
static TableData CreateL1CombinedInitialTable(AppData_Raylib *data);
static bool EnsureLevel1SmoothingCalculations(AppData_Raylib *data);
static TableData CreateL1SmoothingTable(double **smoothing_results, int num_smooth_rows,
                                        const double* coeffs, int num_coeffs,
                                        double *original_data, int original_data_count,
                                        const char* original_data_col_name,
                                        int original_data_offset_for_smoothing);

static void DrawLevel1Tab_InitialData(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataPlusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_DataMinusE(AppData_Raylib *data, Rectangle tab_content_bounds);
static void DrawLevel1Tab_Conclusion(AppData_Raylib *data, Rectangle tab_content_bounds);

static const double SMOOTHING_COEFFS[] = {0.1, 0.4, 0.7, 0.9};
static const int NUM_SMOOTHING_COEFFS = sizeof(SMOOTHING_COEFFS) / sizeof(SMOOTHING_COEFFS[0]);

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
    combined_table.column_names = (char**)calloc(combined_table.num_allocated_column_names, sizeof(char*)); // <--- calloc из stdlib.h
    if (!combined_table.column_names) {
        TraceLog(LOG_ERROR, "L1_COMB_TABLE: Ошибка calloc для column_names.");
        return combined_table; 
    }

    if (data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names[0]) {
        combined_table.column_names[0] = strdup(data->table_data_main.column_names[0]); // <--- strdup из string.h
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
            for (int k = 0; k < i; ++k) if(combined_table.table[k]) free(combined_table.table[k]); // <--- free из stdlib.h
            free(combined_table.table); combined_table.table = NULL;
            goto cleanup_epochs_combined;
        }
        for (int j = 0; j < base_data_cols; ++j) {
            if (data->table_data_main.table[i]) { 
                 combined_table.table[i][j] = data->table_data_main.table[i][j];
            } else {
                 combined_table.table[i][j] = NAN; // <--- NAN из math.h
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

// --- Вспомогательная функция для вычисления данных сглаживания ---
static bool EnsureLevel1SmoothingCalculations(AppData_Raylib *data) {
    if (!data) return false;

    // 1. СНАЧАЛА ОСВОБОЖДАЕМ СТАРЫЕ ДАННЫЕ СГЛАЖИВАНИЯ, ЕСЛИ ОНИ ЕСТЬ
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

    // 2. ТЕПЕРЬ ПРОВЕРЯЕМ НАЛИЧИЕ БАЗОВЫХ ДАННЫХ ДЛЯ НОВЫХ ВЫЧИСЛЕНИЙ
    if (!data->mu_data || data->mu_data_count == 0) {
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Нет mu_data для сглаживания. Новые данные сглаживания не будут вычислены.");
        // Старые уже очищены выше, так что просто выходим
        return false; 
    }
    if (!data->alpha_data || data->alpha_count == 0) { // alpha_count == 0 означает, что и alpha_count > 1 не выполнится
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Нет alpha_data для сглаживания. Новые данные сглаживания alpha не будут вычислены.");
        // Старые smoothing_alpha_data уже очищены.
        // Мы все еще можем попытаться вычислить сглаживание для mu_data, если оно есть.
        // Поэтому не возвращаем false здесь сразу, а позволяем коду ниже обработать mu.
        // Но для alpha сглаживания не будет.
    }

    // 3. ВЫЧИСЛЕНИЕ НОВЫХ ДАННЫХ СГЛАЖИВАНИЯ

    // Сглаживание для mu_data
    TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Вычисление сглаживания для μ (данных: %d)", data->mu_data_count);
    data->smoothing_data = compute_smoothing_rl(data->mu_data, data->mu_data_count,
                                                &data->smoothing_row_count,
                                                (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
    if (!data->smoothing_data) {
        TraceLog(LOG_ERROR, "L1_SMOOTH_CALC: Не удалось вычислить сглаживание для μ.");
        // Если не удалось для mu, то и для alpha, вероятно, нет смысла (или alpha уже обработан)
        return false; 
    }
    TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Сглаживание μ завершено (строк результата: %d).", data->smoothing_row_count);

    // Сглаживание для alpha_data
    if (data->alpha_data && data->alpha_count > 1) { // Убедимся, что alpha_data существует перед доступом к alpha_count
        TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Вычисление сглаживания для α (данных: %d, начиная с индекса 1)", data->alpha_count - 1);
        data->smoothing_alpha_data = compute_smoothing_rl(&data->alpha_data[1], data->alpha_count - 1,
                                                          &data->smoothing_alpha_row_count,
                                                          (double*)SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS);
        if (!data->smoothing_alpha_data) {
            TraceLog(LOG_ERROR, "L1_SMOOTH_CALC: Не удалось вычислить сглаживание для α.");
            // smoothing_data для mu уже вычислены, их не трогаем, но возвращаем false, т.к. не все удалось
            return false; 
        }
        TraceLog(LOG_INFO, "L1_SMOOTH_CALC: Сглаживание α завершено (строк результата: %d).", data->smoothing_alpha_row_count);
    } else {
        TraceLog(LOG_WARNING, "L1_SMOOTH_CALC: Недостаточно данных α для сглаживания (alpha_data NULL, или alpha_count <= 1).");
        // data->smoothing_alpha_data уже NULL после очистки в начале функции
        // data->smoothing_alpha_row_count уже 0
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

    smooth_table_display.column_names[0] = strdup("M (период)");
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
        smooth_table_display.epochs[i] = i; // M = 0, 1, 2, ...
    }

    // 3. Данные таблицы
    smooth_table_display.table = (double**)calloc(smooth_table_display.row_count, sizeof(double*));
    if (!smooth_table_display.table) goto cleanup_smooth_table;

    for (int i = 0; i < smooth_table_display.row_count; ++i) { // i - это M-период
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
        } else { // Если вдруг строка NULL, заполняем NAN
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

// --- Вспомогательная функция для вычисления основных mu и alpha ---
static bool EnsureLevel1BaseCalculations(AppData_Raylib *data) {
    if (!data) return false; // Добавил проверку

    if (!data->db_opened || data->table_data_main.row_count == 0) {
        TraceLog(LOG_WARNING, "L1_CALC: Нет данных в table_data_main для вычислений Уровня 1.");
        if (data->mu_data) { free(data->mu_data); data->mu_data = NULL; data->mu_data_count = 0; }
        if (data->alpha_data) { free(data->alpha_data); data->alpha_data = NULL; data->alpha_count = 0; }
        // TODO: Очистить и другие зависимые данные (сглаживания, производные и т.д.)
        return false;
    }

    // Освобождаем старые данные mu и alpha перед новым расчетом
    if (data->mu_data) {
        free(data->mu_data); // <--- Используем free из stdlib.h
        data->mu_data = NULL;
        data->mu_data_count = 0;
        TraceLog(LOG_DEBUG, "L1_CALC: Освобождены старые mu_data.");
    }
    if (data->alpha_data) {
        free(data->alpha_data); // <--- Используем free из stdlib.h
        data->alpha_data = NULL;
        data->alpha_count = 0;
        TraceLog(LOG_DEBUG, "L1_CALC: Освобождены старые alpha_data.");
    }
    // TODO: Также освободить здесь smoothing_data, smoothing_alpha_data и т.д.

    TraceLog(LOG_INFO, "L1_CALC: Вычисление μ и α для table_data_main (строк: %d, колонок данных: %d)",
             data->table_data_main.row_count, data->table_data_main.column_count);

    data->mu_data = compute_mu_rl(data->table_data_main.table,
                                  data->table_data_main.row_count,
                                  data->table_data_main.column_count);
    if (!data->mu_data) {
        TraceLog(LOG_ERROR, "L1_CALC: Не удалось вычислить μ.");
        return false;
    }
    data->mu_data_count = data->table_data_main.row_count;
    TraceLog(LOG_INFO, "L1_CALC: μ вычислены (%d значений).", data->mu_data_count);

    data->alpha_data = compute_alpha_rl(data->table_data_main.table,
                                        data->table_data_main.row_count,
                                        data->table_data_main.column_count,
                                        data->mu_data);
    if (!data->alpha_data) {
        TraceLog(LOG_ERROR, "L1_CALC: Не удалось вычислить α.");
        free(data->mu_data); // <--- Используем free из stdlib.h
        data->mu_data = NULL;
        data->mu_data_count = 0;
        return false;
    }
    data->alpha_count = data->table_data_main.row_count;
    TraceLog(LOG_INFO, "L1_CALC: α вычислены (%d значений).", data->alpha_count);
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

    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 400, 20}, "Исходные данные, Mu, Alpha сглаживание)");

    float current_y_offset = tab_content_bounds.y + 40; // Начальный отступ для контента вкладки
    float content_padding = 10.0f;

    // 1. Проверка наличия базовых данных из БД
    if (!data->db_opened || data->table_data_main.row_count == 0) {
        DrawText("Нет исходных данных. Откройте файл БД и убедитесь, что основная таблица загружена.",
                 (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, DARKGRAY);
        return;
    }

    // 2. Вычисление μ и α, если необходимо
    if (!data->mu_data || !data->alpha_data ||
        data->mu_data_count != data->table_data_main.row_count ||
        data->alpha_count != data->table_data_main.row_count) {
        TraceLog(LOG_DEBUG, "L1_InitialData: μ/α не актуальны или отсутствуют, вызываем EnsureLevel1BaseCalculations.");
        if (!EnsureLevel1BaseCalculations(data)) {
            DrawText("Ошибка при вычислении μ и α. Проверьте логи.",
                     (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, RED);
            return;
        }
        if (!data->mu_data || !data->alpha_data) { // Дополнительная проверка
             DrawText("Данные μ и α все еще не доступны после попытки вычисления.",
                     (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, ORANGE);
            return;
        }
    }

    // 3. Вычисление данных сглаживания, если необходимо
    // Проверяем, нужно ли пересчитывать сглаживание (например, если mu_data только что обновились или сглаживания нет)
    bool needs_smoothing_recalc = false;
    if (!data->smoothing_data || (data->mu_data_count > 0 && data->smoothing_row_count != (data->mu_data_count + 1))) {
        needs_smoothing_recalc = true;
    }
    if (!data->smoothing_alpha_data || (data->alpha_count > 1 && data->smoothing_alpha_row_count != (data->alpha_count -1 + 1))) {
         if (data->alpha_count <=1 && data->smoothing_alpha_row_count != 0) needs_smoothing_recalc = true; // если альфа мало, а сглаживание есть
         else if (data->alpha_count > 1) needs_smoothing_recalc = true;
    }

    if (needs_smoothing_recalc) {
        TraceLog(LOG_DEBUG, "L1_InitialData: Данные сглаживания не актуальны или отсутствуют, вызываем EnsureLevel1SmoothingCalculations.");
        if (!EnsureLevel1SmoothingCalculations(data)) {
            // Не фатально, просто не будет таблиц сглаживания
            TraceLog(LOG_WARNING, "L1_InitialData: Ошибка при вычислении данных сглаживания.");
        }
    }

    // --- Параметры размещения ---
    float available_width_content = tab_content_bounds.width - 2 * content_padding;
    float available_height_content = tab_content_bounds.height - (current_y_offset - tab_content_bounds.y) - content_padding;

    // --- Отображение объединенной таблицы (Эпоха | X1..XN | μ | α) ---
    float combined_table_height = available_height_content * 0.40f;
    if (combined_table_height < 150 && available_height_content > 150) combined_table_height = 150;
    else if (combined_table_height > available_height_content) combined_table_height = available_height_content;

    Rectangle combined_table_panel_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset,
        available_width_content, combined_table_height
    };

    TableData l1_combined_table = CreateL1CombinedInitialTable(data);
    if (l1_combined_table.row_count > 0 && l1_combined_table.column_names) {
        int temp_selected_row_combined = -1;
        DrawDataTable_RL(data, &l1_combined_table, NULL,
                         combined_table_panel_bounds,
                         &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][0], // Используем скролл из AppData
                         &temp_selected_row_combined, false);
    } else {
        DrawText("Не удалось сформировать объединенную таблицу.",
                 (int)combined_table_panel_bounds.x + 5, (int)combined_table_panel_bounds.y + 5, 18, RED);
    }
    free_table_data_content_rl(&l1_combined_table);
    current_y_offset += combined_table_panel_bounds.height + content_padding;

    // --- Область для таблиц и графиков сглаживания ---
    float remaining_height_for_smoothing_area = tab_content_bounds.y + tab_content_bounds.height - current_y_offset - content_padding;
    if (remaining_height_for_smoothing_area < 150) {
        if (remaining_height_for_smoothing_area > 20) {
             DrawText("Недостаточно места для секций сглаживания.", (int)tab_content_bounds.x + (int)content_padding, (int)current_y_offset, 18, GRAY);
        }
        return; // Выходим, если места мало
    }

    float smoothing_section_width = available_width_content / 2.0f - content_padding / 2.0f;
    float table_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.45f; // Чуть больше для таблиц
    float graph_smoothing_height_alloc = remaining_height_for_smoothing_area * 0.55f - content_padding;

    if (table_smoothing_height_alloc < 80) table_smoothing_height_alloc = 80;
    if (graph_smoothing_height_alloc < 100) graph_smoothing_height_alloc = 100;
    if (table_smoothing_height_alloc + graph_smoothing_height_alloc + content_padding > remaining_height_for_smoothing_area) {
        // Корректировка, если вышли за пределы
        graph_smoothing_height_alloc = remaining_height_for_smoothing_area - table_smoothing_height_alloc - content_padding;
        if (graph_smoothing_height_alloc < 80) { // Если график стал слишком мал, уменьшаем таблицу
            table_smoothing_height_alloc = remaining_height_for_smoothing_area - 80 - content_padding;
            graph_smoothing_height_alloc = 80;
        }
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
            data->mu_data, data->mu_data_count,
            "Mu (реальное)", 0);
        if (mu_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &mu_smoothing_table_display, NULL,
                             smooth_mu_table_bounds, &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][1], &temp_sel_row, false);
        }
        free_table_data_content_rl(&mu_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания μ", (int)smooth_mu_table_bounds.x + 5, (int)smooth_mu_table_bounds.y + 5, 16, DARKGRAY);
    }

    Rectangle smooth_mu_graph_bounds = {
        tab_content_bounds.x + content_padding, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    DrawRectangleLinesEx(smooth_mu_graph_bounds, 1, LIGHTGRAY); // Заглушка для графика
    DrawText("График сглаживания μ", (int)smooth_mu_graph_bounds.x + 5, (int)smooth_mu_graph_bounds.y + 5, 16, DARKGRAY);


    // --- Секция сглаживания для Альфа ---
    float alpha_section_x_start = tab_content_bounds.x + content_padding + smoothing_section_width + content_padding;
    Rectangle smooth_alpha_table_bounds = {
        alpha_section_x_start, current_y_offset,
        smoothing_section_width, table_smoothing_height_alloc
    };
    if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
        TableData alpha_smoothing_table_display = CreateL1SmoothingTable(
            data->smoothing_alpha_data, data->smoothing_alpha_row_count,
            SMOOTHING_COEFFS, NUM_SMOOTHING_COEFFS,
            data->alpha_data, data->alpha_count,
            "Alpha (реальное)", 1);
        if (alpha_smoothing_table_display.row_count > 0) {
            int temp_sel_row = -1;
            DrawDataTable_RL(data, &alpha_smoothing_table_display, NULL,
                             smooth_alpha_table_bounds, &data->l1_tables_scroll[L1_TAB_INITIAL_DATA][2], &temp_sel_row, false);
        }
        free_table_data_content_rl(&alpha_smoothing_table_display);
    } else {
        DrawText("Нет данных для таблицы сглаживания α", (int)smooth_alpha_table_bounds.x + 5, (int)smooth_alpha_table_bounds.y + 5, 16, DARKGRAY);
    }
    
    Rectangle smooth_alpha_graph_bounds = {
        alpha_section_x_start, current_y_offset + table_smoothing_height_alloc + content_padding,
        smoothing_section_width, graph_smoothing_height_alloc
    };
    DrawRectangleLinesEx(smooth_alpha_graph_bounds, 1, LIGHTGRAY); // Заглушка для графика
    DrawText("График сглаживания α", (int)smooth_alpha_graph_bounds.x + 5, (int)smooth_alpha_graph_bounds.y + 5, 16, DARKGRAY);
}

static void DrawLevel1Tab_DataPlusE(AppData_Raylib *data, Rectangle tab_content_bounds) {
    // TODO: Логика для вкладки "+E"
    // - Определить/ввести значение E
    // - Создать временную таблицу X+E на основе data->table_data_main
    // - Вычислить mu_prime_data, alpha_prime_data для X+E
    // - Отобразить таблицы и графики
    // - Возможно, сглаживание для mu_prime, alpha_prime
    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 200, 20}, "Вкладка: +E");
}

static void DrawLevel1Tab_DataMinusE(AppData_Raylib *data, Rectangle tab_content_bounds) {
    // TODO: Логика для вкладки "-E" (аналогично +E)
    // - Вычислить mu_double_prime_data, alpha_double_prime_data для X-E
    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 200, 20}, "Вкладка: -E");
}

static void DrawLevel1Tab_Conclusion(AppData_Raylib *data, Rectangle tab_content_bounds) {
    // TODO: Логика для вкладки "Заключение"
    // - Отображение итоговых графиков (например, α(μ), α'(μ'), α''(μ''))
    // - Возможно, какие-то текстовые выводы или сводные таблицы
    GuiLabel((Rectangle){tab_content_bounds.x + 10, tab_content_bounds.y + 10, 200, 20}, "Вкладка: Заключение");
}