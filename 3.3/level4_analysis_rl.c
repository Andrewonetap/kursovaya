// level4_analysis_rl.c

#include "level4_analysis_rl.h" // Соответствующий заголовочный файл
#include "app_data_rl.h"        // Для доступа к AppData_Raylib, константам
#include "raylib.h"             // Для функций отрисовки Raylib
#include "raygui.h"             // Для элементов интерфейса RayGUI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "level1_analysis_rl.h"

// void ResetLevel4Data(AppData_Raylib *data);
static bool EnsureLevel4DataCalculations(AppData_Raylib *data);


// --- Реализация главной функции Уровня 4 ---
void DrawLevel4View_Entry(AppData_Raylib *data) {
    if (!data) {
        return;
    }

    // 1. Убедимся, что данные рассчитаны
    if (!data->l4_data_calculated) {
        if (!EnsureLevel4DataCalculations(data)) {
            // Кнопка "Назад" для выхода, если данные не могут быть рассчитаны
            if (GuiButton((Rectangle){PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 100, BUTTON_HEIGHT}, "< Назад")) {
                data->current_view = APP_VIEW_MAIN;
            }
            DrawText("Ошибка при подготовке данных для Уровня 4. Проверьте логи.",
                     150, PADDING + TOP_NAV_BAR_HEIGHT + BUTTON_HEIGHT + PADDING, 20, RED);
            return;
        }
    }

    // Если после Ensure... все еще нет данных (маловероятно, если Ensure... вернула true, но для защиты)
    if (!data->l4_data_calculated || data->l4_num_series == 0) {
        if (GuiButton((Rectangle){PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 100, BUTTON_HEIGHT}, "< Назад")) {
            data->current_view = APP_VIEW_MAIN;
            ResetLevel4Data(data);
        }
        DrawText("Данные для Уровня 4 отсутствуют или не могут быть рассчитаны.",
                 150, PADDING + TOP_NAV_BAR_HEIGHT + BUTTON_HEIGHT + PADDING, 20, MAROON);
        return;
    }

    // --- UI Layout ---
    const float padding = PADDING; // Используем PADDING из app_data_rl.h
    const float top_offset_content = TOP_NAV_BAR_HEIGHT + padding;
    const float back_button_width = 100.0f;
    const float back_button_height = BUTTON_HEIGHT; // Используем BUTTON_HEIGHT из app_data_rl.h

    // Кнопка "Назад"
    if (GuiButton((Rectangle){padding, top_offset_content, back_button_width, back_button_height}, "< Назад")) {
        data->current_view = APP_VIEW_MAIN;
        // Решение о сбросе данных l4_data_calculated оставляем на EnsureLevel4DataCalculations при следующем входе
        TraceLog(LOG_INFO, "L4: Возврат в Main View.");
        return;
    }

    float series_list_panel_width = 220.0f; // Немного увеличим ширину для названий
    float graph_panel_x_start = padding + series_list_panel_width + padding;
    float available_height_for_content = (float)data->screen_height - top_offset_content - padding;

    // --- Панель выбора серий (слева) ---
    Rectangle series_list_panel_bounds = {
        padding,
        top_offset_content + back_button_height + padding,
        series_list_panel_width,
        available_height_for_content - (back_button_height + padding)
    };
    if (series_list_panel_bounds.height < 0) series_list_panel_bounds.height = 0;

    // Заголовок для списка
    GuiLabel((Rectangle){series_list_panel_bounds.x, series_list_panel_bounds.y - 22, series_list_panel_bounds.width, 20}, NULL);

    // Параметры для кастомного списка
    float item_height_custom = 22.0f; // Высота одного элемента в списке
    float checkbox_size = 12.0f;      // Размер квадратика для "галочки"
    float checkbox_text_spacing = 5.0f; // Расстояние между галочкой и текстом
    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float list_font_size = GuiGetStyle(DEFAULT, TEXT_SIZE); // Можно настроить отдельно
    if (list_font_size > item_height_custom - 4) list_font_size = item_height_custom - 4; // Уменьшаем, если не влезает
    if (list_font_size < 10) list_font_size = 10;
    float list_text_spacing = GuiGetStyle(DEFAULT, TEXT_SPACING);

    // Определяем палитру цветов, которая будет использоваться и для списка, и для графика
    Color series_palette[] = {
        RED, GREEN, BLUE, ORANGE, PURPLE, BROWN, GOLD, PINK, LIME, SKYBLUE, VIOLET, DARKGREEN,
        BEIGE, DARKBROWN, LIGHTGRAY, DARKBLUE, DARKPURPLE, LIME, MAROON, YELLOW, // Добавим еще цветов
        (Color){255,0,255,255}, (Color){0,255,255,255}, (Color){128,0,0,255}, (Color){0,128,0,255}, (Color){0,0,128,255}
    };
    int palette_size = sizeof(series_palette) / sizeof(series_palette[0]);


    Rectangle scroll_panel_content_rect = {
        0, 0,
        series_list_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), // Ширина контента внутри ScrollPanel
        data->l4_num_series * item_height_custom // Общая высота всех элементов списка
    };
    Rectangle scroll_panel_view_rect = {0}; // Видимая область ScrollPanel

    // Рисуем фон для панели списка, если нужно (например, темный)
    // DrawRectangleRec(series_list_panel_bounds, GetColor(0x2B3E50FF)); // Пример темного фона

    GuiScrollPanel(series_list_panel_bounds, NULL, scroll_panel_content_rect, &data->l4_series_list_scroll, &scroll_panel_view_rect);
    BeginScissorMode((int)scroll_panel_view_rect.x, (int)scroll_panel_view_rect.y, (int)scroll_panel_view_rect.width, (int)scroll_panel_view_rect.height);
    {
        for (int i = 0; i < data->l4_num_series; ++i) {
            Rectangle item_row_rect = {
                scroll_panel_view_rect.x,
                scroll_panel_view_rect.y + data->l4_series_list_scroll.y + i * item_height_custom,
                scroll_panel_view_rect.width,
                item_height_custom
            };

            // Пропускаем отрисовку элементов, которые не видны
            if ((item_row_rect.y + item_row_rect.height < scroll_panel_view_rect.y) ||
                (item_row_rect.y > scroll_panel_view_rect.y + scroll_panel_view_rect.height)) {
                continue;
            }

            Color item_bg_color = BLANK; // Прозрачный фон по умолчанию
            Color item_text_color = GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)); // Стандартный цвет текста

            // Подсветка при наведении (опционально)
            if (CheckCollisionPointRec(GetMousePosition(), item_row_rect)) {
                item_bg_color = Fade(GetColor(GuiGetStyle(BUTTON, BASE_COLOR_FOCUSED)), 0.3f); // Полупрозрачный фон при наведении
            }

            DrawRectangleRec(item_row_rect, item_bg_color); // Рисуем фон элемента

            // Квадратик-индикатор цвета серии (как в легенде на скриншоте друга)
            Rectangle color_indicator_rect = {
                item_row_rect.x + 5,
                item_row_rect.y + (item_height_custom - checkbox_size) / 2,
                checkbox_size,
                checkbox_size
            };
            DrawRectangleRec(color_indicator_rect, series_palette[i % palette_size]);
            DrawRectangleLinesEx(color_indicator_rect, 1, DARKGRAY); // Рамка для индикатора

            // "Галочка" (если выбрано)
            if (data->l4_series_selected_for_graph[i]) {
                // Рисуем галочку внутри color_indicator_rect или рядом
                // Для простоты можно нарисовать ее поверх цветного квадратика
                DrawLineEx((Vector2){color_indicator_rect.x + 2, color_indicator_rect.y + checkbox_size/2},
                           (Vector2){color_indicator_rect.x + checkbox_size/2, color_indicator_rect.y + checkbox_size - 2}, 2, BLACK);
                DrawLineEx((Vector2){color_indicator_rect.x + checkbox_size/2, color_indicator_rect.y + checkbox_size - 2},
                           (Vector2){color_indicator_rect.x + checkbox_size - 2, color_indicator_rect.y + 2}, 2, BLACK);
            }

            // Текст имени серии
            DrawTextEx(current_font,
                       data->l4_series_names[i] ? data->l4_series_names[i] : "N/A",
                       (Vector2){item_row_rect.x + 5 + checkbox_size + checkbox_text_spacing,
                                  item_row_rect.y + (item_height_custom - list_font_size) / 2},
                       list_font_size, list_text_spacing, item_text_color);

            // Обработка клика по всей строке элемента
            if (CheckCollisionPointRec(GetMousePosition(), item_row_rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                data->l4_series_selected_for_graph[i] = !data->l4_series_selected_for_graph[i];
                data->l4_main_graph_state.is_active = false; // Пересчитать/перемасштабировать график
            }
        }
    }
    EndScissorMode();

    // --- График (справа) ---
    Rectangle graph_bounds = {
        graph_panel_x_start,
        top_offset_content,
        (float)data->screen_width - graph_panel_x_start - padding,
        available_height_for_content
    };
    if (graph_bounds.width < 0) graph_bounds.width = 0;
    if (graph_bounds.height < 0) graph_bounds.height = 0;

    // Подготовка данных для DrawMultiSeriesLineGraph_L1
    GraphSeriesData_L1 *series_to_draw_on_graph = NULL;
    int num_series_to_draw_on_graph = 0;
    for (int i = 0; i < data->l4_num_series; ++i) {
        if (data->l4_series_selected_for_graph[i]) {
            num_series_to_draw_on_graph++;
        }
    }

    if (num_series_to_draw_on_graph > 0) {
        series_to_draw_on_graph = (GraphSeriesData_L1*)RL_MALLOC(num_series_to_draw_on_graph * sizeof(GraphSeriesData_L1));
        if (series_to_draw_on_graph) {
            int current_graph_series_idx = 0;
            for (int i = 0; i < data->l4_num_series; ++i) {
                if (data->l4_series_selected_for_graph[i]) {
                    series_to_draw_on_graph[current_graph_series_idx].x_values = NULL;
                    series_to_draw_on_graph[current_graph_series_idx].y_values = data->l4_smoothed_series_data[i];
                    series_to_draw_on_graph[current_graph_series_idx].num_points = data->l4_num_epochs_original + 1; // Включая прогноз
                    series_to_draw_on_graph[current_graph_series_idx].color = series_palette[i % palette_size];
                    series_to_draw_on_graph[current_graph_series_idx].name = data->l4_series_names[i];
                    current_graph_series_idx++;
                }
            }
            DrawMultiSeriesLineGraph_L1(data, graph_bounds, series_to_draw_on_graph, num_series_to_draw_on_graph,
                                        &data->l4_main_graph_state, "Эпоха (сглаженная)", "Значение");
            RL_FREE(series_to_draw_on_graph);
        } else {
            DrawText("Ошибка выделения памяти для серий графика L4", (int)graph_bounds.x + 10, (int)graph_bounds.y + 10, 18, RED);
        }
    } else {
        // Рисуем пустой график или сообщение
        DrawRectangleRec(graph_bounds, GetColor(0x1A242FFF)); // Темный фон для пустого графика
        DrawRectangleLinesEx(graph_bounds, 1, DARKGRAY);
        const char* no_selection_msg = "Выберите точки из списка слева для отображения на графике.";
        Vector2 no_selection_msg_size = MeasureTextEx(current_font, no_selection_msg, 18, 1);
        DrawTextEx(current_font, no_selection_msg,
                   (Vector2){graph_bounds.x + (graph_bounds.width - no_selection_msg_size.x)/2,
                              graph_bounds.y + (graph_bounds.height - no_selection_msg_size.y)/2},
                   18, 1, LIGHTGRAY);
        data->l4_main_graph_state.is_active = false; // Сброс для автомасштабирования при следующем выборе
    }
}


void ResetLevel4Data(AppData_Raylib *data) {
    if (!data) return;
    TraceLog(LOG_INFO, "L4_RESET: Сброс данных Уровня 4.");

    data->l4_alpha_coeff_loaded = false;
    data->l4_data_calculated = false;

    if (data->l4_smoothed_series_data) {
        for (int i = 0; i < data->l4_num_series; i++) {
            if (data->l4_smoothed_series_data[i]) {
                free(data->l4_smoothed_series_data[i]);
            }
        }
        free(data->l4_smoothed_series_data);
        data->l4_smoothed_series_data = NULL;
    }

    if (data->l4_series_names) {
        for (int i = 0; i < data->l4_num_series; i++) {
            if (data->l4_series_names[i]) {
                free(data->l4_series_names[i]);
            }
        }
        free(data->l4_series_names);
        data->l4_series_names = NULL;
    }

    if (data->l4_series_selected_for_graph) {
        free(data->l4_series_selected_for_graph);
        data->l4_series_selected_for_graph = NULL;
    }

    data->l4_num_series = 0;
    data->l4_num_epochs_original = 0;

    // Сброс состояния графика
    data->l4_main_graph_state.is_active = false;
    data->l4_main_graph_state.offset = (Vector2){0,0};
    data->l4_main_graph_state.scale = (Vector2){0,0};
    data->l4_main_graph_state.is_panning = false;
}

static bool EnsureLevel4DataCalculations(AppData_Raylib *data) {
    if (!data) return false;

    // Если данные уже рассчитаны и актуальны (например, если не было смены БД или table_data_main),
    // можно добавить проверку и выйти. Пока будем пересчитывать каждый раз при входе на уровень,
    // если data->l4_data_calculated == false.
    // Для более сложной проверки актуальности можно использовать хэши данных или временные метки.

    // Сначала сбрасываем предыдущие данные Уровня 4
    ResetLevel4Data(data);

    // 1. Проверка наличия исходных данных
    if (!data->db_opened || data->table_data_main.row_count == 0 || data->table_data_main.column_count == 0) {
        TraceLog(LOG_WARNING, "L4_CALC: Нет данных в table_data_main для вычислений Уровня 4.");
        return false;
    }

    if (data->table_data_values.row_count == 0 || data->table_data_values.column_names == NULL || data->table_data_values.num_allocated_column_names == 0) {
        TraceLog(LOG_WARNING, "L4_CALC: Таблица 'Значения' не загружена (нет строк или имен колонок).");
        return false;
    }

    // 2. Получение коэффициента сглаживания Alpha (A)
    int col_A_idx = -1;
    for (int i = 0; i < data->table_data_values.num_allocated_column_names; ++i) {
        if (data->table_data_values.column_names[i] && strcmp(data->table_data_values.column_names[i], "A") == 0) {
            col_A_idx = i;
            break;
        }
    }
    if (col_A_idx == -1 || col_A_idx >= data->table_data_values.column_count) {
        TraceLog(LOG_ERROR, "L4_CALC: Колонка 'A' не найдена или некорректна в таблице 'Значения'.");
        return false;
    }
    // Предполагаем, что значение 'A' находится в первой строке данных таблицы "Значения"
    if (!data->table_data_values.table || !data->table_data_values.table[0]) {
        TraceLog(LOG_ERROR, "L4_CALC: Отсутствуют строки данных в таблице 'Значения'.");
        return false;
    }
    data->l4_smoothing_alpha_coeff = (float)data->table_data_values.table[0][col_A_idx];
    if (isnan(data->l4_smoothing_alpha_coeff) || data->l4_smoothing_alpha_coeff < 0.0f || data->l4_smoothing_alpha_coeff > 1.0f) {
        TraceLog(LOG_ERROR, "L4_CALC: Некорректное значение коэффициента Alpha (A=%.4f). Должно быть [0, 1].", data->l4_smoothing_alpha_coeff);
        return false;
    }
    data->l4_alpha_coeff_loaded = true;
    TraceLog(LOG_INFO, "L4_CALC: Используется коэффициент Alpha (A) = %.4f", data->l4_smoothing_alpha_coeff);

    // 3. Инициализация размеров
    data->l4_num_series = data->table_data_main.column_count; // Количество колонок данных
    data->l4_num_epochs_original = data->table_data_main.row_count;
    int num_epochs_smoothed = data->l4_num_epochs_original + 1; // +1 для прогноза

    if (data->l4_num_series == 0 || data->l4_num_epochs_original == 0) {
        TraceLog(LOG_WARNING, "L4_CALC: Недостаточно серий (%d) или эпох (%d) в table_data_main.", data->l4_num_series, data->l4_num_epochs_original);
        return false;
    }

    // 4. Выделение памяти
    data->l4_smoothed_series_data = (double**)calloc(data->l4_num_series, sizeof(double*));
    data->l4_series_names = (char**)calloc(data->l4_num_series, sizeof(char*));
    data->l4_series_selected_for_graph = (bool*)calloc(data->l4_num_series, sizeof(bool));

    if (!data->l4_smoothed_series_data || !data->l4_series_names || !data->l4_series_selected_for_graph) {
        TraceLog(LOG_ERROR, "L4_CALC: Ошибка выделения памяти для основных структур Уровня 4.");
        ResetLevel4Data(data); // Очистка того, что могло выделиться
        return false;
    }

    for (int i = 0; i < data->l4_num_series; ++i) {
        data->l4_smoothed_series_data[i] = (double*)calloc(num_epochs_smoothed, sizeof(double));
        if (!data->l4_smoothed_series_data[i]) {
            TraceLog(LOG_ERROR, "L4_CALC: Ошибка выделения памяти для l4_smoothed_series_data[%d].", i);
            ResetLevel4Data(data); return false;
        }
        // Имена серий (пропускаем первую колонку "Эпоха" из table_data_main.column_names)
        if ((i + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[i + 1]) {
            data->l4_series_names[i] = strdup(data->table_data_main.column_names[i + 1]);
        } else {
            char temp_name[32];
            sprintf(temp_name, "Точка %d", i + 1);
            data->l4_series_names[i] = strdup(temp_name);
        }
        if (!data->l4_series_names[i]) {
             TraceLog(LOG_ERROR, "L4_CALC: Ошибка strdup для имени серии %d.", i);
             ResetLevel4Data(data); return false;
        }
        data->l4_series_selected_for_graph[i] = true; // По умолчанию выбираем все для отображения
    }

    // 5. Расчет сглаживания для каждой серии (колонки)
    float alpha = data->l4_smoothing_alpha_coeff;

    for (int j = 0; j < data->l4_num_series; ++j) { // j - индекс серии/колонки
        // a. Расчет среднего значения для текущей серии Yj
        double sum_Yj = 0;
        int count_valid_Yj = 0;
        for (int e = 0; e < data->l4_num_epochs_original; ++e) { // e - индекс эпохи
            if (data->table_data_main.table[e] && !isnan(data->table_data_main.table[e][j])) {
                sum_Yj += data->table_data_main.table[e][j];
                count_valid_Yj++;
            }
        }
        double avg_Yj = (count_valid_Yj > 0) ? (sum_Yj / count_valid_Yj) : 0.0;
        if (count_valid_Yj == 0) { // Если все значения в колонке NAN
             TraceLog(LOG_WARNING, "L4_CALC: Все значения в серии '%s' (индекс %d) являются NAN. Сглаживание будет NAN.", data->l4_series_names[j], j);
             for(int k=0; k < num_epochs_smoothed; ++k) data->l4_smoothed_series_data[j][k] = NAN;
             continue; // Переходим к следующей серии
        }


        // b. Расчет S0 для серии j
        double Y0j = (data->table_data_main.table[0] && !isnan(data->table_data_main.table[0][j])) ? data->table_data_main.table[0][j] : avg_Yj; // Если Y0 NAN, берем среднее
        data->l4_smoothed_series_data[j][0] = Y0j * alpha + (1.0 - alpha) * avg_Yj;

        // c. Расчет S_i для серии j (i от 1 до N_original - 1)
        for (int e = 1; e < data->l4_num_epochs_original; ++e) {
            double Yej = (data->table_data_main.table[e] && !isnan(data->table_data_main.table[e][j]))
                         ? data->table_data_main.table[e][j]
                         : data->l4_smoothed_series_data[j][e-1]; // Если Y_i NAN, используем S_{i-1} (метод последней точки)
            data->l4_smoothed_series_data[j][e] = Yej * alpha + (1.0 - alpha) * data->l4_smoothed_series_data[j][e-1];
        }

        // d. Расчет S_прогноз для серии j (находится в индексе l4_num_epochs_original)
        double sum_Sj_calculated = 0;
        int count_valid_Sj = 0;
        for (int e = 0; e < data->l4_num_epochs_original; ++e) {
            if (!isnan(data->l4_smoothed_series_data[j][e])) {
                sum_Sj_calculated += data->l4_smoothed_series_data[j][e];
                count_valid_Sj++;
            }
        }
        double avg_Sj_calculated = (count_valid_Sj > 0) ? (sum_Sj_calculated / count_valid_Sj) : 0.0;
        double S_last_calculated_j = data->l4_smoothed_series_data[j][data->l4_num_epochs_original - 1];
        if (count_valid_Sj == 0) S_last_calculated_j = 0.0; // Если все S_i были NAN

        data->l4_smoothed_series_data[j][data->l4_num_epochs_original] = avg_Sj_calculated * alpha + (1.0 - alpha) * S_last_calculated_j;
    }

    data->l4_data_calculated = true;
    TraceLog(LOG_INFO, "L4_CALC: Данные Уровня 4 успешно рассчитаны для %d серий.", data->l4_num_series);
    return true;
}