// level4_analysis_rl.c

#include "level4_analysis_rl.h" // Соответствующий заголовочный файл
#include "app_data_rl.h"        // Для доступа к AppData_Raylib, константам
#include "raylib.h"             // Для функций отрисовки Raylib
#include "raygui.h"             // Для элементов интерфейса RayGUI
#include "raymath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// void ResetLevel4Data(AppData_Raylib *data);
static bool EnsureLevel4DataCalculations(AppData_Raylib *data);

static inline int ClampIntL4(int value, int min, int max);
void HandleLevel4GraphControls(AppData_Raylib *data, Rectangle graph_render_area);

void DrawMultiSeriesLineGraph_L4(
    AppData_Raylib *data,
    Rectangle plot_area_bounds, // Общие границы для всего компонента графика
    GraphSeriesData_L1 *series_array, // Используем ту же структуру для данных серий
    int num_series,
    GraphDisplayState *graph_state, // Указатель на data->l4_main_graph_state
    const char* x_axis_label_text,
    const char* y_axis_label_text
);

// --- Реализация главной функции Уровня 4 ---
void DrawLevel4View_Entry(AppData_Raylib *data) {
    if (!data) {
        TraceLog(LOG_ERROR, "DrawL4View: AppData is NULL.");
        // В реальном приложении здесь может быть более серьезная обработка или выход
        return;
    }

    // 1. Убедимся, что данные рассчитаны
    // Этот флаг устанавливается в true функцией EnsureLevel4DataCalculations
    if (!data->l4_data_calculated) {
        TraceLog(LOG_DEBUG, "DrawL4View: Данные для Уровня 4 не рассчитаны, вызывается EnsureLevel4DataCalculations.");
        if (!EnsureLevel4DataCalculations(data)) {
            // Если EnsureLevel4DataCalculations вернула false, значит, произошла ошибка при подготовке данных
            TraceLog(LOG_ERROR, "DrawL4View: EnsureLevel4DataCalculations вернула false. Данные не могут быть подготовлены.");
            // Отображаем сообщение об ошибке и кнопку "Назад"
            if (GuiButton((Rectangle){PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 100, BUTTON_HEIGHT}, "< Назад")) {
                data->current_view = APP_VIEW_MAIN;
                // ResetLevel4Data(data); // Можно вызвать здесь, если Ensure... не очищает полностью при ошибке
            }
            DrawText("Ошибка при подготовке данных для Уровня 4. Проверьте логи.",
                     PADDING + 100 + PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 20, RED);
            return;
        }
        // Если EnsureLevel4DataCalculations прошла успешно, data->l4_data_calculated должен быть true.
        if (!data->l4_data_calculated) { // Дополнительная защитная проверка
            TraceLog(LOG_ERROR, "DrawL4View: data->l4_data_calculated все еще false после успешного (предположительно) вызова EnsureLevel4DataCalculations.");
             DrawText("L4: Внутренняя ошибка, данные не отмечены как рассчитанные.",
                     PADDING + 100 + PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 20, MAROON);
            return;
        }
    }
    
    // Если данные рассчитаны, но нет серий (например, исходная таблица была пуста)
    if (data->l4_num_series == 0 && data->l4_data_calculated) {
        TraceLog(LOG_INFO, "DrawL4View: Данные Уровня 4 рассчитаны, но нет серий для отображения.");
        if (GuiButton((Rectangle){PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 100, BUTTON_HEIGHT}, "< Назад")) {
            data->current_view = APP_VIEW_MAIN;
            // ResetLevel4Data(data); // Сброс при выходе
        }
        DrawText("Данные для Уровня 4 рассчитаны, но нет серий для отображения (исходная таблица пуста?).",
                 PADDING + 100 + PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 20, DARKGRAY);
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
        // ResetLevel4Data(data); // Рекомендуется сбрасывать при выходе, если данные L4 тяжелые или не нужны сразу
        TraceLog(LOG_INFO, "L4: Возврат в Main View.");
        return;
    }

    // Размеры панелей
    float series_list_panel_width = 220.0f; 
    float graph_panel_x_start = padding + series_list_panel_width + padding;
    float available_height_for_content = (float)data->screen_height - top_offset_content - padding; // Общая высота под контент

    // --- Панель выбора серий (слева) ---
    Rectangle series_list_panel_bounds = {
        padding, // X
        top_offset_content + back_button_height + padding, // Y (под кнопкой "Назад")
        series_list_panel_width, // Width
        available_height_for_content - (back_button_height + padding) // Height (оставшееся место по высоте)
    };
    if (series_list_panel_bounds.height < 0) series_list_panel_bounds.height = 0; // Защита от отрицательной высоты

    // Заголовок для списка серий
    GuiLabel((Rectangle){series_list_panel_bounds.x, series_list_panel_bounds.y - 22, series_list_panel_bounds.width, 20}, NULL);

    // Параметры для отрисовки списка серий
    float item_height_custom = 22.0f;
    float checkbox_size = 12.0f;     
    float checkbox_text_spacing = 5.0f;
    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float list_font_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    if (list_font_size > item_height_custom - 4) list_font_size = item_height_custom - 4;
    if (list_font_size < 10) list_font_size = 10; // Минимальный размер шрифта
    float list_text_spacing = GuiGetStyle(DEFAULT, TEXT_SPACING);

    // Палитра цветов для серий
    Color series_palette[] = {
        RED, GREEN, BLUE, ORANGE, PURPLE, BROWN, GOLD, PINK, LIME, SKYBLUE, VIOLET, DARKGREEN,
        BEIGE, DARKBROWN, LIGHTGRAY, DARKBLUE, DARKPURPLE, (Color){200,200,0,255}/*DarkYellow*/, MAROON, YELLOW,
        (Color){255,0,255,255}/*MAGENTA*/, (Color){0,255,255,255}/*CYAN*/, 
        (Color){128,0,0,255}/*DarkRed*/, (Color){0,128,0,255}/*DarkGreen2*/, (Color){0,0,128,255}/*DarkBlue2*/
        // Добавь больше цветов, если нужно, или используй генератор цветов
    };
    int palette_size = sizeof(series_palette) / sizeof(series_palette[0]);

    // ScrollPanel для списка серий
    Rectangle scroll_panel_content_rect = {
        0, 0, // x, y относительно начала ScrollPanel
        series_list_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), // Ширина контента
        data->l4_num_series * item_height_custom // Общая высота всех элементов списка
    };
    Rectangle scroll_panel_view_rect = {0}; // Видимая область ScrollPanel, будет заполнена GuiScrollPanel

    GuiScrollPanel(series_list_panel_bounds, NULL, scroll_panel_content_rect, &data->l4_series_list_scroll, &scroll_panel_view_rect);
    
    BeginScissorMode((int)scroll_panel_view_rect.x, (int)scroll_panel_view_rect.y, (int)scroll_panel_view_rect.width, (int)scroll_panel_view_rect.height);
    { // Блок для ScissorMode
        for (int i = 0; i < data->l4_num_series; ++i) {
            Rectangle item_row_rect = {
                scroll_panel_view_rect.x, // X относительно экрана
                scroll_panel_view_rect.y + data->l4_series_list_scroll.y + i * item_height_custom, // Y с учетом прокрутки
                scroll_panel_view_rect.width, // Ширина видимой области
                item_height_custom
            };

            // Оптимизация: пропускаем отрисовку элементов, которые полностью не видны
            if ((item_row_rect.y + item_row_rect.height < scroll_panel_view_rect.y) ||
                (item_row_rect.y > scroll_panel_view_rect.y + scroll_panel_view_rect.height)) {
                continue;
            }

            Color item_bg_color = BLANK; // Прозрачный фон по умолчанию
            Color item_text_color = GetColor(data->current_theme_colors.text_primary);

            if (CheckCollisionPointRec(GetMousePosition(), item_row_rect)) { // Подсветка при наведении
                item_bg_color = Fade(GetColor(data->current_theme_colors.button_base_focused), 0.3f);
            }
            DrawRectangleRec(item_row_rect, item_bg_color); // Фон элемента

            // Квадратик-индикатор цвета серии
            Rectangle color_indicator_rect = {
                item_row_rect.x + 5, // Отступ слева
                item_row_rect.y + (item_height_custom - checkbox_size) / 2, // Центрирование по вертикали
                checkbox_size, checkbox_size
            };
            DrawRectangleRec(color_indicator_rect, series_palette[i % palette_size]);
            DrawRectangleLinesEx(color_indicator_rect, 1, DARKGRAY);

            // "Галочка", если серия выбрана
            if (data->l4_series_selected_for_graph && data->l4_series_selected_for_graph[i]) { // Проверка, что массив выделен
                DrawLineEx((Vector2){color_indicator_rect.x + 2, color_indicator_rect.y + checkbox_size/2},
                           (Vector2){color_indicator_rect.x + checkbox_size/2, color_indicator_rect.y + checkbox_size - 2}, 2, BLACK);
                DrawLineEx((Vector2){color_indicator_rect.x + checkbox_size/2, color_indicator_rect.y + checkbox_size - 2},
                           (Vector2){color_indicator_rect.x + checkbox_size - 2, color_indicator_rect.y + 2}, 2, BLACK);
            }

            // Текст имени серии
            const char* series_name_to_draw = (data->l4_series_names && data->l4_series_names[i]) ? data->l4_series_names[i] : "N/A";
            DrawTextEx(current_font, series_name_to_draw,
                       (Vector2){item_row_rect.x + 5 + checkbox_size + checkbox_text_spacing,
                                  item_row_rect.y + (item_height_custom - list_font_size) / 2}, // Центрирование текста по вертикали
                       list_font_size, list_text_spacing, item_text_color);

            // Обработка клика по строке элемента
            if (CheckCollisionPointRec(GetMousePosition(), item_row_rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (data->l4_series_selected_for_graph) { // Убедимся, что массив существует
                    data->l4_series_selected_for_graph[i] = !data->l4_series_selected_for_graph[i];
                    TraceLog(LOG_INFO, "L4: Серия '%s' (индекс %d) выбрана: %s", 
                             series_name_to_draw, i, 
                             data->l4_series_selected_for_graph[i] ? "ДА" : "НЕТ");
                    // Если нужна автоматическая подгонка графика при изменении набора серий:
                    // L4_FitDataToView(data, graph_total_bounds); // Понадобится функция L4_FitDataToView
                }
            }
        }
    }
    EndScissorMode();

    // --- График (справа) ---
    // graph_total_bounds - это вся область, выделенная под компонент графика (включая оси, легенду).
    Rectangle graph_total_bounds = {
        graph_panel_x_start,
        top_offset_content, // Выравниваем по верху с кнопкой "Назад"
        (float)data->screen_width - graph_panel_x_start - padding, // Ширина до правого края окна
        available_height_for_content // Используем всю доступную высоту под контентом
    };
    if (graph_total_bounds.width < 0) graph_total_bounds.width = 0; // Защита
    if (graph_total_bounds.height < 0) graph_total_bounds.height = 0;

    // Вызываем HandleLevel4GraphControls ПЕРЕД отрисовкой графика.
    // Эта функция будет изменять data->l4_main_graph_state.offset и .scale
    if (graph_total_bounds.width > 10 && graph_total_bounds.height > 10) { // Управление только если есть место для графика
         HandleLevel4GraphControls(data, graph_total_bounds);
    }

    // Подготовка данных для DrawMultiSeriesLineGraph_L4
    GraphSeriesData_L1 *series_to_draw_on_graph = NULL;
    int num_series_to_draw_on_graph = 0;

    if (data->l4_series_selected_for_graph) { // Убедимся, что массив флагов выбора существует
        for (int i = 0; i < data->l4_num_series; ++i) {
            if (data->l4_series_selected_for_graph[i]) {
                num_series_to_draw_on_graph++;
            }
        }
    }

    if (num_series_to_draw_on_graph > 0) {
        series_to_draw_on_graph = (GraphSeriesData_L1*)RL_MALLOC(num_series_to_draw_on_graph * sizeof(GraphSeriesData_L1));
        if (series_to_draw_on_graph) {
            int current_graph_series_idx = 0;
            for (int i = 0; i < data->l4_num_series; ++i) {
                if (data->l4_series_selected_for_graph[i]) {
                    // Проверка, что не выходим за пределы выделенной памяти (хотя не должны, если num_series_to_draw_on_graph посчитан верно)
                    if (current_graph_series_idx < num_series_to_draw_on_graph && 
                        data->l4_smoothed_series_data && data->l4_smoothed_series_data[i] &&
                        data->l4_series_names && data->l4_series_names[i]) { 
                        
                        series_to_draw_on_graph[current_graph_series_idx].x_values = NULL; // Используем индексы (0...N) для оси X
                        series_to_draw_on_graph[current_graph_series_idx].y_values = data->l4_smoothed_series_data[i];
                        series_to_draw_on_graph[current_graph_series_idx].num_points = data->l4_num_epochs_original + 1; // Включая прогнозную точку
                        series_to_draw_on_graph[current_graph_series_idx].color = series_palette[i % palette_size];
                        series_to_draw_on_graph[current_graph_series_idx].name = data->l4_series_names[i];
                        current_graph_series_idx++;
                    } else {
                        TraceLog(LOG_WARNING, "DrawL4View: Проблема с доступом к данным для серии %d при формировании массива для графика.", i);
                    }
                }
            }
            // Вызываем новую функцию отрисовки графика для Уровня 4
            DrawMultiSeriesLineGraph_L4(data, graph_total_bounds, series_to_draw_on_graph, 
                                        current_graph_series_idx, // Используем фактическое количество добавленных серий
                                        &data->l4_main_graph_state, 
                                        "Эпоха (сглаженная)", "Значение");
            RL_FREE(series_to_draw_on_graph); // Освобождаем временный массив
            series_to_draw_on_graph = NULL;
        } else {
            TraceLog(LOG_ERROR, "DrawL4View: Ошибка выделения памяти для series_to_draw_on_graph (запрошено %d серий).", num_series_to_draw_on_graph);
            DrawText("Ошибка выделения памяти для серий графика L4", (int)graph_total_bounds.x + 10, (int)graph_total_bounds.y + 10, 18, RED);
        }
    } else {
        // Если не выбрано ни одной серии, рисуем пустой график или сообщение
        DrawRectangleRec(graph_total_bounds, GetColor(data->current_theme_colors.panel_background));
        DrawRectangleLinesEx(graph_total_bounds, 1, GetColor(data->current_theme_colors.border_universal));
        const char* no_selection_msg = "Выберите серии из списка слева для отображения на графике.";
        Vector2 no_selection_msg_size = MeasureTextEx(current_font, no_selection_msg, 18, 1);
        DrawTextEx(current_font, no_selection_msg,
                   (Vector2){graph_total_bounds.x + (graph_total_bounds.width - no_selection_msg_size.x)/2,
                              graph_total_bounds.y + (graph_total_bounds.height - no_selection_msg_size.y)/2},
                   18, 1, GetColor(data->current_theme_colors.placeholder_text_color));
    }
}


void ResetLevel4Data(AppData_Raylib *data) {
    if (!data) {
        TraceLog(LOG_ERROR, "L4_RESET: Попытка сброса данных с NULL AppData_Raylib указателем.");
        return;
    }
    TraceLog(LOG_INFO, "L4_RESET: Начало сброса данных и состояния Уровня 4.");

    // 1. Сброс флагов и счетчиков
    data->l4_alpha_coeff_loaded = false;
    data->l4_data_calculated = false; 

    // 2. Освобождение динамически выделенной памяти для данных серий
    if (data->l4_smoothed_series_data) {
        TraceLog(LOG_DEBUG, "L4_RESET: Освобождение l4_smoothed_series_data (количество серий было: %d).", data->l4_num_series);
        for (int i = 0; i < data->l4_num_series; i++) {
            if (data->l4_smoothed_series_data[i]) {
                free(data->l4_smoothed_series_data[i]);
                data->l4_smoothed_series_data[i] = NULL;
            }
        }
        free(data->l4_smoothed_series_data);
        data->l4_smoothed_series_data = NULL;
    }

    if (data->l4_series_names) {
        TraceLog(LOG_DEBUG, "L4_RESET: Освобождение l4_series_names (количество серий было: %d).", data->l4_num_series);
        for (int i = 0; i < data->l4_num_series; i++) {
            if (data->l4_series_names[i]) {
                free(data->l4_series_names[i]);
                data->l4_series_names[i] = NULL;
            }
        }
        free(data->l4_series_names);
        data->l4_series_names = NULL;
    }

    if (data->l4_series_selected_for_graph) {
        TraceLog(LOG_DEBUG, "L4_RESET: Освобождение l4_series_selected_for_graph.");
        free(data->l4_series_selected_for_graph);
        data->l4_series_selected_for_graph = NULL;
    }

    // 3. Сброс счетчиков серий и эпох
    data->l4_num_series = 0;
    data->l4_num_epochs_original = 0;
    TraceLog(LOG_DEBUG, "L4_RESET: l4_num_series и l4_num_epochs_original обнулены.");

    // 4. Сброс состояния прокрутки списка серий
    data->l4_series_list_scroll = (Vector2){0.0f, 0.0f};
    TraceLog(LOG_DEBUG, "L4_RESET: l4_series_list_scroll сброшена.");

    // 5. Сброс состояния основного графика Уровня 4
    data->l4_main_graph_state.is_active = false; // При сбросе снова включаем автомасштаб
    data->l4_main_graph_state.offset = (Vector2){0.0f, 0.0f}; 
    data->l4_main_graph_state.scale = (Vector2){10.0f, 1.0f}; // Значение по умолчанию, будет перезаписано автомасштабом
    data->l4_main_graph_state.is_panning = false;
    // data->l4_main_graph_state.data_bounds_set = false; // Если этого поля нет, эта строка должна быть удалена

    // ---- ВЕСЬ БЛОК КОДА, СВЯЗАННЫЙ С l4_zoom_presets и l4_current_zoom_preset_index, ----
    // ---- ДОЛЖЕН БЫТЬ УДАЛЕН ИЗ ЭТОЙ ФУНКЦИИ (ResetLevel4Data) ----
    // ---- Ошибки компиляции как раз указывают на строки из этого блока. ----
    // ---- Убедись, что строки, на которые жалуется компилятор (343, 346, 347, 349, 352), ----
    // ---- в твоей функции ResetLevel4Data отсутствуют или закомментированы. ----

    TraceLog(LOG_INFO, "L4_RESET: Сброс данных и состояния Уровня 4 завершен. is_active=false (ожидается автомасштаб).");
}

static bool EnsureLevel4DataCalculations(AppData_Raylib *data) {
    if (!data) return false;

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

static inline int ClampIntL4(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void HandleLevel4GraphControls(AppData_Raylib *data, Rectangle graph_render_area) {
    if (!data) {
        TraceLog(LOG_WARNING, "HandleL4GraphControls: AppData is NULL.");
        return;
    }

    GraphDisplayState *gs = &data->l4_main_graph_state;
    bool scale_changed = false; // Флаг, что масштаб был изменен

    // --- ПАНОРАМИРОВАНИЕ (остается как было) ---
    if (CheckCollisionPointRec(GetMousePosition(), graph_render_area)) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            gs->is_panning = true;
            gs->pan_start_mouse_pos = GetMousePosition();
            gs->pan_start_offset = gs->offset;
            TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Начато панорамирование.");
        }
    }
    if (gs->is_panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            gs->is_active = true; // Пользовательское действие
            Vector2 mouse_delta = Vector2Subtract(GetMousePosition(), gs->pan_start_mouse_pos);
            if (gs->scale.x != 0) gs->offset.x = gs->pan_start_offset.x - mouse_delta.x / gs->scale.x;
            if (gs->scale.y != 0) gs->offset.y = gs->pan_start_offset.y + mouse_delta.y / gs->scale.y;
        } else {
            gs->is_panning = false;
            TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Завершено панорамирование.");
        }
    }

    // --- ЗУМ (с множителями) ---
    float zoom_multiplier_step = 1.15f; // Множитель для одного шага зума (15% увеличение/уменьшение)
    float min_scale_value = 0.000001f; // Минимально допустимое значение для scale.x и scale.y
    float max_scale_value = 100000.0f; // Максимально допустимое значение (чтобы не улететь в бесконечность)


    // 1. Колесико мыши
    if (CheckCollisionPointRec(GetMousePosition(), graph_render_area)) {
        float wheel_move = GetMouseWheelMove();
        if (wheel_move != 0) {
            gs->is_active = true; // Пользовательское действие
            scale_changed = true;
            float current_zoom_factor = (wheel_move > 0) ? zoom_multiplier_step : (1.0f / zoom_multiplier_step);

            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) { // Зум только по Y
                gs->scale.y *= current_zoom_factor;
                TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Zoom Y by wheel factor %.2f", current_zoom_factor);
            } else if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) { // Зум только по X
                gs->scale.x *= current_zoom_factor;
                TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Zoom X by wheel factor %.2f", current_zoom_factor);
            } else { // Зум по обеим осям
                gs->scale.x *= current_zoom_factor;
                gs->scale.y *= current_zoom_factor;
                TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Zoom XY by wheel factor %.2f", current_zoom_factor);
            }
        }
    }

    // 2. Клавиши для зума
    float keyboard_zoom_factor = 0.0f; // 0 - нет зума, >1 - приблизить, <1 - отдалить

    // Зум по Y
    if (IsKeyPressed(KEY_PAGE_UP)) { keyboard_zoom_factor = zoom_multiplier_step; gs->scale.y *= keyboard_zoom_factor; scale_changed = true; gs->is_active = true; }
    if (IsKeyPressed(KEY_PAGE_DOWN)) { keyboard_zoom_factor = 1.0f / zoom_multiplier_step; gs->scale.y *= keyboard_zoom_factor; scale_changed = true; gs->is_active = true; }

    // Зум по X (с CTRL)
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (IsKeyPressed(KEY_UP)) { keyboard_zoom_factor = zoom_multiplier_step; gs->scale.x *= keyboard_zoom_factor; scale_changed = true; gs->is_active = true;}
        if (IsKeyPressed(KEY_DOWN)) { keyboard_zoom_factor = 1.0f / zoom_multiplier_step; gs->scale.x *= keyboard_zoom_factor; scale_changed = true; gs->is_active = true;}
    }

    // Зум по обеим осям (+/-)
    if (IsKeyPressed(KEY_KP_ADD) || (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_EQUAL)) || IsKeyPressed(KEY_EQUAL)) {
        keyboard_zoom_factor = zoom_multiplier_step;
        gs->scale.x *= keyboard_zoom_factor;
        gs->scale.y *= keyboard_zoom_factor;
        scale_changed = true;
        gs->is_active = true;
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) {
        keyboard_zoom_factor = 1.0f / zoom_multiplier_step;
        gs->scale.x *= keyboard_zoom_factor;
        gs->scale.y *= keyboard_zoom_factor;
        scale_changed = true;
        gs->is_active = true;
    }

    // Ограничение масштаба
    if (scale_changed) {
        if (fabsf(gs->scale.x) < min_scale_value) gs->scale.x = (gs->scale.x < 0 ? -1 : 1) * min_scale_value;
        if (fabsf(gs->scale.y) < min_scale_value) gs->scale.y = (gs->scale.y < 0 ? -1 : 1) * min_scale_value;
        if (fabsf(gs->scale.x) > max_scale_value) gs->scale.x = (gs->scale.x < 0 ? -1 : 1) * max_scale_value;
        if (fabsf(gs->scale.y) > max_scale_value) gs->scale.y = (gs->scale.y < 0 ? -1 : 1) * max_scale_value;
    }

    // Если масштаб изменился, корректируем offset для зума к центру/курсору
    if (scale_changed) {
        TraceLog(LOG_INFO, "L4_GRAPH_CTRL: Масштаб изменен. Новый scale: (%.3f, %.3f)", gs->scale.x, gs->scale.y);

        Vector2 zoom_focus_screen;
         // Если зум был колесиком, то к курсору
        if (CheckCollisionPointRec(GetMousePosition(), graph_render_area) && GetMouseWheelMove() != 0.0f) {
            zoom_focus_screen = GetMousePosition();
        } else { // Иначе (клавиатура) - к центру области графика
            zoom_focus_screen = (Vector2){
                graph_render_area.x + graph_render_area.width / 2.0f,
                graph_render_area.y + graph_render_area.height / 2.0f
            };
        }

        // Мировые координаты точки фокуса ДО изменения масштаба
        // Для корректного расчета, нам нужен scale ДО его изменения этим шагом зума.
        // Но так как мы уже изменили gs->scale, нам нужно "откатить" его для этого расчета,
        // либо сохранить старый scale перед циклом зума.
        // Проще сохранить старый scale.
        // Однако, если keyboard_zoom_factor не 0, то это один шаг.
        // Если зум был колесиком, то current_zoom_factor уже содержит множитель этого шага.

        // Эта логика коррекции offset должна быть применена ПОСЛЕ того, как scale ИЗМЕНИЛСЯ,
        // но используя scale, который был ДО изменения, для расчета world_focus_...
        // Сейчас я упрощу: будем считать, что коррекция offset произойдет на следующем кадре
        // или что изменение scale настолько мало за один шаг, что смещение не будет слишком заметным.
        // Для идеального зума к точке, нужно сохранять scale *перед* его умножением.

        // Более точный подход (требует сохранения старого масштаба):
        // Vector2 old_scale = gs->scale / keyboard_zoom_factor; // или / current_zoom_factor если колесиком
        // float world_focus_x_before_zoom = gs->offset.x + (zoom_focus_screen.x - graph_render_area.x) / old_scale.x;
        // ...
        // gs->offset.x = world_focus_x_before_zoom - (zoom_focus_screen.x - graph_render_area.x) / gs->scale.x;
        // ...
        // Я пока оставлю без этой сложной коррекции для первого раза, чтобы не усложнять.
        // Поведение будет "зум к центру экрана" при зуме клавиатурой, если не двигать мышь.
        // При зуме колесиком, так как он происходит "вокруг курсора", это уже будет работать более-менее интуитивно.

        // Для более простого зума "относительно текущего центра вида":
        // Если зум не колесиком, то можно скорректировать offset, чтобы центр вида остался на месте:
        if (GetMouseWheelMove() == 0.0f && keyboard_zoom_factor != 0.0f) { // Зум клавиатурой
            float world_center_x_before = gs->offset.x + (graph_render_area.width / 2.0f) / (gs->scale.x / keyboard_zoom_factor); // scale до изменения
            float world_center_y_before = gs->offset.y + (graph_render_area.height / 2.0f) / (gs->scale.y / keyboard_zoom_factor);

            gs->offset.x = world_center_x_before - (graph_render_area.width / 2.0f) / gs->scale.x;
            gs->offset.y = world_center_y_before - (graph_render_area.height / 2.0f) / gs->scale.y;
             TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Скорректирован offset для зума клавиатурой: (%.2f, %.2f)", gs->offset.x, gs->offset.y);
        } else if (GetMouseWheelMove() != 0.0f) { // Зум колесиком (коррекция к курсору)
            float current_wheel_zoom_factor = (GetMouseWheelMove() > 0) ? zoom_multiplier_step : (1.0f / zoom_multiplier_step);
            Vector2 old_scale_for_wheel_zoom = {gs->scale.x / current_wheel_zoom_factor, gs->scale.y / current_wheel_zoom_factor};
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) old_scale_for_wheel_zoom.x = gs->scale.x; // Если зум только по Y, X не менялся
            else if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) old_scale_for_wheel_zoom.y = gs->scale.y; // Если зум только по X, Y не менялся

            float world_focus_x_before_zoom = gs->offset.x + (zoom_focus_screen.x - graph_render_area.x) / old_scale_for_wheel_zoom.x;
            float world_focus_y_before_zoom = gs->offset.y + (graph_render_area.height - (zoom_focus_screen.y - graph_render_area.y)) / old_scale_for_wheel_zoom.y;

            gs->offset.x = world_focus_x_before_zoom - (zoom_focus_screen.x - graph_render_area.x) / gs->scale.x;
            gs->offset.y = world_focus_y_before_zoom - (graph_render_area.height - (zoom_focus_screen.y - graph_render_area.y)) / gs->scale.y;
             TraceLog(LOG_DEBUG, "L4_GRAPH_CTRL: Скорректирован offset для зума колесиком: (%.2f, %.2f)", gs->offset.x, gs->offset.y);
        }
    }
}

void DrawMultiSeriesLineGraph_L4(
    AppData_Raylib *data,
    Rectangle plot_area_bounds, // Общие границы для всего компонента графика
    GraphSeriesData_L1 *series_array,
    int num_series,
    GraphDisplayState *graph_state, // Указатель на data->l4_main_graph_state
    const char* x_axis_label_text,
    const char* y_axis_label_text
) {
    if (!data || !graph_state) {
        if (plot_area_bounds.width > 1 && plot_area_bounds.height > 1) {
            DrawText("Graph Error: Invalid params (data or graph_state is NULL)",
                     (int)plot_area_bounds.x + 5, (int)plot_area_bounds.y + 5, 10, RED);
        }
        return;
    }
    if (num_series > 0 && !series_array) {
         DrawText("Graph Error: No series data provided for L4 graph",
                  (int)plot_area_bounds.x + 5, (int)plot_area_bounds.y + 5, 10, RED);
         return;
    }

    // --- Определяем отступы и область для самого графика (graph_content_area) ---
    // Эти значения можно сделать константами или параметрами функции
    float padding_left = 50;  // Место для меток оси Y
    float padding_bottom = 30; // Место для меток оси X и названия оси X
    float padding_top = 15;    // Отступ сверху
    float padding_right = 15;  // Отступ справа (если нет легенды)

    // Если есть легенда, увеличим правый отступ
    bool has_legend = false;
    if (num_series > 0) {
        for (int s = 0; s < num_series; ++s) {
            if (series_array[s].name && series_array[s].name[0] != '\0') {
                has_legend = true;
                break;
            }
        }
    }
    if (has_legend) {
        padding_right = 100.0f; // Место для легенды
    }

    Rectangle graph_content_area = {
        plot_area_bounds.x + padding_left,
        plot_area_bounds.y + padding_top,
        plot_area_bounds.width - padding_left - padding_right,
        plot_area_bounds.height - padding_top - padding_bottom
    };

    // Проверка, что область графика имеет смысл
    if (graph_content_area.width <= 10 || graph_content_area.height <= 10) {
        DrawText("Graph content area too small",
                 (int)plot_area_bounds.x + 5, (int)plot_area_bounds.y + 5, 10, RED);
        return;
    }

    // --- Удаляем внутреннюю логику зума и панорамирования ---
    // --- Удаляем логику авто-масштабирования (if !graph_state->is_active) ---
    // Масштаб (graph_state->scale) и смещение (graph_state->offset) теперь
    // полностью управляются извне через HandleLevel4GraphControls.

    // --- Отрисовка фона и рамки ---
    DrawRectangleRec(plot_area_bounds, GetColor(data->current_theme_colors.panel_background)); // Фон всей области компонента
    DrawRectangleRec(graph_content_area, WHITE); // Белый фон для самого графика

    // --- Отрисовка сетки и осей ---
    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float axis_label_font_size = 12.0f;
    float axis_tick_font_size = 10.0f;
    float text_spacing = 1.0f;
    Color grid_color = LIGHTGRAY;
    Color axis_color = DARKGRAY;
    Color tick_label_color = GetColor(data->current_theme_colors.text_primary);


    // Защита от нулевого масштаба (маловероятно, но на всякий случай)
    if (fabsf(graph_state->scale.x) < 1e-7 || fabsf(graph_state->scale.y) < 1e-7) {
        DrawText("L4 Graph Error: Invalid scale (near zero)",
                 (int)graph_content_area.x + 10, (int)graph_content_area.y + 10, 10, RED);
        EndScissorMode(); // Если был BeginScissorMode до этого
        return;
    }

    // --- Горизонтальная сетка и метки оси Y ---
    int num_y_grid_lines = 5; // Желаемое количество линий сетки по Y
    double y_world_view_min = graph_state->offset.y;
    double y_world_view_max = graph_state->offset.y + graph_content_area.height / graph_state->scale.y;
    
    for (int i = 0; i <= num_y_grid_lines; ++i) {
        double y_level_world = y_world_view_min + (y_world_view_max - y_world_view_min) * ((double)i / num_y_grid_lines);
        float screen_y_grid = (graph_content_area.y + graph_content_area.height) - (float)((y_level_world - graph_state->offset.y) * graph_state->scale.y);

        if (screen_y_grid >= graph_content_area.y - 0.5f && screen_y_grid <= graph_content_area.y + graph_content_area.height + 0.5f) {
            DrawLineEx((Vector2){graph_content_area.x, screen_y_grid},
                       (Vector2){graph_content_area.x + graph_content_area.width, screen_y_grid}, 1.0f, grid_color);
            
            // Метки оси Y
            char y_label_buf[32];
            snprintf(y_label_buf, sizeof(y_label_buf), "%.2f", y_level_world); // Формат можно настроить
            Vector2 y_tick_label_size = MeasureTextEx(current_font, y_label_buf, axis_tick_font_size, text_spacing);
            DrawTextEx(current_font, y_label_buf,
                       (Vector2){graph_content_area.x - y_tick_label_size.x - 5, screen_y_grid - y_tick_label_size.y / 2},
                       axis_tick_font_size, text_spacing, tick_label_color);
        }
    }

    // --- Вертикальная сетка и метки оси X ---
    int num_x_grid_lines = 8; // Желаемое количество линий сетки по X
    double x_world_view_min = graph_state->offset.x;
    double x_world_view_max = graph_state->offset.x + graph_content_area.width / graph_state->scale.x;

    for (int i = 0; i <= num_x_grid_lines; ++i) {
        double x_level_world = x_world_view_min + (x_world_view_max - x_world_view_min) * ((double)i / num_x_grid_lines);
        float screen_x_grid = graph_content_area.x + (float)((x_level_world - graph_state->offset.x) * graph_state->scale.x);

        if (screen_x_grid >= graph_content_area.x - 0.5f && screen_x_grid <= graph_content_area.x + graph_content_area.width + 0.5f) {
            DrawLineEx((Vector2){screen_x_grid, graph_content_area.y},
                       (Vector2){screen_x_grid, graph_content_area.y + graph_content_area.height}, 1.0f, grid_color);
            
            // Метки оси X
            char x_label_buf[32];
            snprintf(x_label_buf, sizeof(x_label_buf), "%.1f", x_level_world); // Формат можно настроить
            Vector2 x_tick_label_size = MeasureTextEx(current_font, x_label_buf, axis_tick_font_size, text_spacing);
            DrawTextEx(current_font, x_label_buf,
                       (Vector2){screen_x_grid - x_tick_label_size.x / 2, graph_content_area.y + graph_content_area.height + 5},
                       axis_tick_font_size, text_spacing, tick_label_color);
        }
    }
    
    DrawRectangleLinesEx(graph_content_area, 1, axis_color); // Рамка графика

    // --- Названия осей ---
    if (x_axis_label_text && x_axis_label_text[0] != '\0') {
        Vector2 x_label_size = MeasureTextEx(current_font, x_axis_label_text, axis_label_font_size, text_spacing);
        DrawTextEx(current_font, x_axis_label_text,
                   (Vector2){graph_content_area.x + (graph_content_area.width - x_label_size.x) / 2.0f,
                              graph_content_area.y + graph_content_area.height + padding_bottom - x_label_size.y - 2}, // Снизу
                   axis_label_font_size, text_spacing, tick_label_color);
    }
    if (y_axis_label_text && y_axis_label_text[0] != '\0') {
        Vector2 y_label_size = MeasureTextEx(current_font, y_axis_label_text, axis_label_font_size, text_spacing);
         DrawTextPro(current_font, y_axis_label_text,
                    (Vector2){plot_area_bounds.x + (padding_left - y_label_size.y)/2.0f, // Слева, повернуто
                               graph_content_area.y + (graph_content_area.height + y_label_size.x) / 2.0f},
                    (Vector2){0,0}, -90, axis_label_font_size, text_spacing, tick_label_color);
    }

    // --- Отрисовка серий данных ---
    BeginScissorMode((int)graph_content_area.x, (int)graph_content_area.y,
                     (int)graph_content_area.width, (int)graph_content_area.height);
    {
        for (int s = 0; s < num_series; ++s) {
            GraphSeriesData_L1 current_series = series_array[s];
            if (!current_series.y_values || current_series.num_points < 1) continue;

            // Маркеры и линии
            float point_radius = 3.0f; // Уменьшил радиус точек для L4
            float line_thickness = 1.5f;

            if (current_series.num_points == 1) {
                double x_val_single = current_series.x_values ? current_series.x_values[0] : 0.0; // Если x_values нет, берем 0-ю эпоху
                if (isnan(x_val_single) || isnan(current_series.y_values[0]) || 
                    isinf(x_val_single) || isinf(current_series.y_values[0])) continue;

                float screen_x_single = graph_content_area.x + (float)((x_val_single - graph_state->offset.x) * graph_state->scale.x);
                float screen_y_single = (graph_content_area.y + graph_content_area.height) - (float)((current_series.y_values[0] - graph_state->offset.y) * graph_state->scale.y);
                
                // Рисуем только если точка внутри видимой области (грубая проверка)
                if (screen_x_single >= graph_content_area.x && screen_x_single <= graph_content_area.x + graph_content_area.width &&
                    screen_y_single >= graph_content_area.y && screen_y_single <= graph_content_area.y + graph_content_area.height) {
                    DrawCircleV((Vector2){screen_x_single, screen_y_single}, point_radius, current_series.color);
                    DrawCircleLines(screen_x_single, screen_y_single, point_radius, BLACK);
                }
            } else {
                Vector2 prev_point_screen = {0};
                bool first_valid_segment_point = false;

                for (int i = 0; i < current_series.num_points; ++i) {
                    double current_x_val = current_series.x_values ? current_series.x_values[i] : (double)i; // Если x_values нет, используем индекс как эпоху
                    double current_y_val = current_series.y_values[i];

                    if (isnan(current_x_val) || isnan(current_y_val) || 
                        isinf(current_x_val) || isinf(current_y_val)) {
                        first_valid_segment_point = false; // Разрываем линию, если точка невалидна
                        continue;
                    }

                    float screen_x = graph_content_area.x + (float)((current_x_val - graph_state->offset.x) * graph_state->scale.x);
                    float screen_y = (graph_content_area.y + graph_content_area.height) - (float)((current_y_val - graph_state->offset.y) * graph_state->scale.y);
                    Vector2 current_point_screen = {screen_x, screen_y};
                    
                    // Рисуем маркер, если точка валидна и хотя бы частично видна (оптимизация)
                    bool current_point_visible_rough = 
                        (screen_x >= graph_content_area.x - point_radius && screen_x <= graph_content_area.x + graph_content_area.width + point_radius &&
                         screen_y >= graph_content_area.y - point_radius && screen_y <= graph_content_area.y + graph_content_area.height + point_radius);

                    if (current_point_visible_rough) { // Рисуем маркер
                         DrawCircleV(current_point_screen, point_radius, current_series.color);
                         DrawCircleLines(current_point_screen.x, current_point_screen.y, point_radius, BLACK);
                    }

                    if (first_valid_segment_point) { // first_valid_segment_point означает, что prev_point_screen валидна
                        // Рисуем линию, если обе точки валидны
                        DrawLineEx(prev_point_screen, current_point_screen, line_thickness, current_series.color);
                    }
                    prev_point_screen = current_point_screen;
                    first_valid_segment_point = true; // Текущая точка стала предыдущей валидной для следующего сегмента
                }
            }
        }
    }
    EndScissorMode();

    // --- Легенда (если нужна) ---
    if (has_legend) {
        float legend_x_start = graph_content_area.x + graph_content_area.width + 10; // Правее графика
        float legend_y_start = graph_content_area.y;
        float legend_item_height = 15.0f;
        float legend_color_box_size = 10.0f;
        float legend_text_padding = 5.0f;
        float legend_font_size = 10.0f;

        for (int s = 0; s < num_series; ++s) {
            if (series_array[s].name && series_array[s].name[0] != '\0') {
                float current_legend_y = legend_y_start + s * legend_item_height;
                // Проверка, чтобы легенда не выходила за пределы plot_area_bounds
                if (current_legend_y + legend_item_height > plot_area_bounds.y + plot_area_bounds.height - padding_bottom) break; 

                DrawRectangleRec((Rectangle){legend_x_start, current_legend_y + (legend_item_height - legend_color_box_size)/2.0f, 
                                            legend_color_box_size, legend_color_box_size}, 
                                 series_array[s].color);
                DrawTextEx(current_font, series_array[s].name,
                           (Vector2){legend_x_start + legend_color_box_size + legend_text_padding, 
                                      current_legend_y + (legend_item_height - legend_font_size)/2.0f},
                           legend_font_size, text_spacing, tick_label_color);
            }
        }
    }
}