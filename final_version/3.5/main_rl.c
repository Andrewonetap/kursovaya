#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#include "app_data_rl.h"
#include "sqlite_utils_rl.h"
#include "calculations_rl.h"
#include "level1_analysis_rl.h"
#include "level4_analysis_rl.h"
#include "color_themes.h"  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tinyfiledialogs.h"
#include <ctype.h>
#include <math.h>

// --- прототипы ---
static void InitApp(AppData_Raylib *data);
static void UpdateDrawAppFrame(AppData_Raylib *data);
static void ShutdownApp(AppData_Raylib *data);
static void DrawMainView(AppData_Raylib *data);
static void DrawEditMainTableRowDialog(AppData_Raylib *data);
static void DrawEditValuesDialog(AppData_Raylib *data);
static void DrawTopNavigationBar(AppData_Raylib *data, Rectangle bounds);

static void DrawHelpWindow(AppData_Raylib *data);


#define TABLE_CELL_TEXT_PADDING_X 5.0f


// --- функция применения стилей ---
static void ApplyGuiThemeStyles(AppData_Raylib *data) {
    ThemeColors *theme = &data->current_theme_colors;

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, theme->panel_background);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, theme->text_primary);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(DEFAULT, LINE_COLOR, theme->border_universal);

    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, theme->button_base_normal);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, theme->text_on_button_normal);
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, theme->button_base_focused);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, theme->text_on_button_normal);
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, theme->border_universal);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, theme->button_base_pressed);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, theme->text_on_button_active);
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, theme->border_universal);
    GuiSetStyle(BUTTON, TEXT_COLOR_DISABLED, theme->button_text_disabled);
    GuiSetStyle(BUTTON, BASE_COLOR_DISABLED, theme->button_base_disabled);

    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, theme->textbox_bg_normal);
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, theme->text_primary);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED, theme->text_primary);
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, theme->border_focused_accent);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_DISABLED, theme->text_disabled);

    GuiSetStyle(LISTVIEW, BACKGROUND_COLOR, theme->list_item_bg);
    GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, theme->list_item_bg);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(LISTVIEW, TEXT_COLOR_NORMAL, theme->text_primary);
    GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, theme->list_item_selected_bg);
    GuiSetStyle(LISTVIEW, TEXT_COLOR_FOCUSED, theme->list_item_selected_text);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_FOCUSED, theme->list_selected_border);
    GuiSetStyle(LISTVIEW, BASE_COLOR_DISABLED, theme->list_item_alt_bg);

    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, theme->text_primary);

    GuiSetStyle(SCROLLBAR, BASE_COLOR_NORMAL, theme->scrollbar_track_bg);
    GuiSetStyle(SCROLLBAR, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, theme->scrollbar_thumb_bg);
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, theme->scrollbar_thumb_bg);
    GuiSetStyle(SLIDER, BASE_COLOR_FOCUSED, theme->scrollbar_thumb_focused);
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, theme->scrollbar_thumb_pressed);
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL, theme->border_universal);

    GuiSetStyle(ICON_BOX, BACKGROUND_COLOR, theme->panel_background);
    GuiSetStyle(ICON_BOX, BORDER_COLOR_NORMAL, theme->border_universal);
    GuiSetStyle(ICON_BOX, TEXT_COLOR_NORMAL, theme->text_primary);


}


// main
int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    //SetTraceLogLevel(LOG_ALL);

    AppData_Raylib app_data;
    InitApp(&app_data);
    while (!WindowShouldClose()) {
        UpdateDrawAppFrame(&app_data);
    }
    ShutdownApp(&app_data);
    return 0;
}

static void InitApp(AppData_Raylib *data) {
    if (!data) return;
    memset(data, 0, sizeof(AppData_Raylib));

    for (int i = 0; i <= L1_TAB_CONCLUSION; i++) {
        for (int j = 0; j < 3; j++) {
            data->l1_tables_scroll[i][j] = (Vector2){0.0f, 0.0f};
        }
    }

    data->show_help_window = false; // справка по умолчания закрыта

    data->screen_width = 1280; //размер окна
    data->screen_height = 720; // специально такой выбрал чтобы на ноуте тоже нормально выглядело
    data->current_view = APP_VIEW_MAIN;
    data->l1_active_tab = L1_TAB_INITIAL_DATA;

    strcpy(data->db_file_path_text_box, "Вариант28.sqlite");
    data->db_file_path_edit_mode = false;
    strcpy(data->values_table_name, "Значения");
    strcpy(data->schema_blob_column_name, "Схема");
    data->main_h_split_pos = (float)data->screen_width * 0.65f;
    data->show_right_panel_main_view = true;

    for (int i = 0; i < GRAPH_ID_COUNT; i++) {
        data->graph_states[i].scale = (Vector2){0.0f, 0.0f};
        data->graph_states[i].offset = (Vector2){0.0f, 0.0f};
        data->graph_states[i].is_active = false;
    }

    data->main_table_selected_row = -1;
    data->values_table_selected_row = -1;
    data->app_font_loaded = false;
    data->main_table_scroll = (Vector2){0, 0};
    data->values_table_scroll = (Vector2){0, 0};
    data->main_edit_dialog_scroll = (Vector2){0,0};

    data->levelsDropdownActive = false;
    data->levelsDropdownRect = (Rectangle){0,0,0,0};
    data->levelsMainButtonRect = (Rectangle){0,0,0,0};

    data->l1_conclusion_table_selected_row = -1;

    data->show_main_edit_dialog = false;
    data->main_edit_dialog_row_index = -1;
    for (int i = 0; i < MAX_DATA_COLUMNS_MAIN; i++) {
        data->main_edit_dialog_field_buffers[i][0] = '\0';
        data->main_edit_dialog_field_edit_mode[i] = false;
    }
    data->show_values_edit_dialog = false;
    data->values_edit_dialog_row_index = -1;
     for (int i = 0; i < MAX_DATA_COLUMNS_VALUES; i++) {
        data->values_edit_dialog_field_buffers[i][0] = '\0';
        data->values_edit_dialog_field_edit_mode[i] = false;
    }
    data->values_edit_dialog_scroll = (Vector2){0,0};

    // Level 4 
    data->l4_smoothing_alpha_coeff = 0.0f;
    data->l4_alpha_coeff_loaded = false;
    data->l4_smoothed_series_data = NULL;
    data->l4_num_series = 0;
    data->l4_num_epochs_original = 0;
    data->l4_series_names = NULL;
    data->l4_series_selected_for_graph = NULL;
    data->l4_series_list_scroll = (Vector2){0, 0};
    data->l4_main_graph_state.is_active = false;
    data->l4_main_graph_state.offset = (Vector2){0,0};
    data->l4_main_graph_state.scale = (Vector2){0,0};
    data->l4_main_graph_state.is_panning = false;
    data->l4_data_calculated = false;

    data->l4_main_graph_state.is_active = true; //
    data->l4_main_graph_state.offset = (Vector2){0.0f, 500.0f}; // Начальное смещение

    data->screen_width = 1176;
    data->screen_height = 664;

    InitializeAllApplicationThemes();
    data->current_theme_id = LIGHT_THEME_ID;
    data->current_theme_colors = app_themes[data->current_theme_id];

    InitWindow(data->screen_width, data->screen_height, "Анализ данных временных рядов (Raylib)");
    SetTargetFPS(60);

    GuiLoadStyleDefault();    // сначала загружается стандартный стиль RayGUI
    ApplyGuiThemeStyles(data); // затем применяю мою темку

    // --- ЗАГРУЗКА ШРИФТА---
    #define MAX_CODEPOINTS_INIT_APP 512
    int codepoints[MAX_CODEPOINTS_INIT_APP] = { 0 };
    int currentGlyphCount = 0;
    for (int i = 0; i < 95; i++) { if (currentGlyphCount < MAX_CODEPOINTS_INIT_APP) codepoints[currentGlyphCount++] = 32 + i; else break; }
    for (int i = 0; i < 256; i++) { if (currentGlyphCount < MAX_CODEPOINTS_INIT_APP) codepoints[currentGlyphCount++] = 0x0400 + i; else break; }

    TraceLog(LOG_INFO, "InitApp: Загрузка шрифта с %d кодовыми точками.", currentGlyphCount);
    data->app_font = LoadFontEx("fonts/LinotypeSyntaxCom-Bold.ttf", 20, codepoints, currentGlyphCount);

    if (data->app_font.texture.id == 0) {
        TraceLog(LOG_WARNING, "InitApp: ШРИФТ: Не удалось загрузить пользовательский шрифт! Будет использован стандартный.");
        data->app_font_loaded = false;
        data->app_font = GetFontDefault();
    } else {
        TraceLog(LOG_INFO, "InitApp: ШРИФТ: Загружен успешно (%d глифов)!", data->app_font.glyphCount);
        data->app_font_loaded = true;
        GuiSetFont(data->app_font);
    }

    srand(time(NULL));
    TraceLog(LOG_INFO, "InitApp: Приложение инициализировано.");
}

static void ShutdownApp(AppData_Raylib *data) {
    if (!data) return;
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Завершение работы приложения...");

    if (data->db_opened && data->db_handle) {
        sqlite3_close(data->db_handle);
        data->db_handle = NULL;
        data->db_opened = false;
        TraceLog(LOG_INFO, "APP_SHUTDOWN: База данных закрыта.");
    }

    free_table_data_content_rl(&data->table_data_main);
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Данные table_data_main освобождены.");
    free_table_data_content_rl(&data->table_data_values);
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Данные table_data_values освобождены.");

    if (data->schema_texture_loaded) {
        UnloadTexture(data->schema_texture);
        data->schema_texture_loaded = false;
        TraceLog(LOG_INFO, "APP_SHUTDOWN: Текстура схемы выгружена.");
    }

    #define SAFE_FREE_ARRAY_SHUTDOWN(arr_ptr_field, count_field) \
        do { \
            if(data->arr_ptr_field) { \
                free(data->arr_ptr_field); \
                data->arr_ptr_field = NULL; \
                data->count_field = 0; \
                TraceLog(LOG_DEBUG, "APP_SHUTDOWN: Освобожден массив %s.", #arr_ptr_field); \
            } \
        } while(0)

    #define SAFE_FREE_TABLE_SHUTDOWN(table_ptr_field, row_count_field) \
        do { \
            if(data->table_ptr_field) { \
                TraceLog(LOG_DEBUG, "APP_SHUTDOWN: Освобождение таблицы %s (%d строк)...", #table_ptr_field, data->row_count_field); \
                for(int i_clean_sh = 0; i_clean_sh < data->row_count_field; i_clean_sh++) { \
                    if(data->table_ptr_field[i_clean_sh]) free(data->table_ptr_field[i_clean_sh]); \
                } \
                free(data->table_ptr_field); \
                data->table_ptr_field = NULL; \
                data->row_count_field = 0; \
                TraceLog(LOG_DEBUG, "APP_SHUTDOWN: Таблица %s освобождена.", #table_ptr_field); \
            } \
        } while(0)

    SAFE_FREE_ARRAY_SHUTDOWN(mu_data, mu_data_count);
    SAFE_FREE_ARRAY_SHUTDOWN(alpha_data, alpha_count);
    SAFE_FREE_ARRAY_SHUTDOWN(mu_prime_data, mu_prime_data_count);
    SAFE_FREE_ARRAY_SHUTDOWN(alpha_prime_data, alpha_prime_data_count);
    SAFE_FREE_ARRAY_SHUTDOWN(mu_double_prime_data, mu_double_prime_data_count);
    SAFE_FREE_ARRAY_SHUTDOWN(alpha_double_prime_data, alpha_double_prime_data_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_data, smoothing_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_data, smoothing_alpha_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_mu_prime_data, smoothing_mu_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_prime_data, smoothing_alpha_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_mu_double_prime_data, smoothing_mu_double_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_double_prime_data, smoothing_alpha_double_prime_row_count);
    #undef SAFE_FREE_ARRAY_SHUTDOWN
    #undef SAFE_FREE_TABLE_SHUTDOWN
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Вычисленные массивы и таблицы освобождены.");

    // освобождение данных Уровня 4
    ResetLevel4Data(data);
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Данные Уровня 4 освобождены.");


    if (data->app_font_loaded) {
        UnloadFont(data->app_font);
        data->app_font_loaded = false;
        TraceLog(LOG_INFO, "APP_SHUTDOWN: Пользовательский шрифт выгружен.");
    }

    close_sqlite_debug_log_rl();
    close_calc_debug_log_rl();
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Файлы логов модулей закрыты (попытка).");

    CloseWindow();
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Raylib окно закрыто. Приложение завершено.");
}

void DrawDataTable_RL(AppData_Raylib *data, TableData *tableData, const char* title,
                             Rectangle panel_bounds, Vector2 *scroll, int *selectedRow, bool isValuesTableType) {
    if (!data) return;

    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float base_font_size = data->app_font_loaded ? data->app_font.baseSize : 20;
    if (base_font_size < 10) base_font_size = 10;

    if (!tableData || !tableData->column_names) {
        DrawTextEx(current_font, "TableData is NULL or column_names missing",
                   (Vector2){panel_bounds.x + PADDING, panel_bounds.y + PADDING},
                   base_font_size, 1, GetColor(data->current_theme_colors.error_text_color));
        return;
    }

    // использование констант из app_data_rl.h
    int first_col_width = TABLE_EPOCH_COL_WIDTH;
    bool hide_first_col = (isValuesTableType && HIDE_ROWID_COLUMN_FOR_VALUES_TABLE);

    Rectangle contentRect = {0};
    float current_content_width = 0;
    if (!hide_first_col) {
        current_content_width += first_col_width;
    }
    for (int c_idx = 0; c_idx < tableData->column_count; ++c_idx) {
        if (isValuesTableType && c_idx < tableData->num_allocated_column_names && tableData->column_names[c_idx] &&
            strcmp(tableData->column_names[c_idx], data->schema_blob_column_name) == 0 &&
            HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
            continue;
        }
        current_content_width += TABLE_DATA_COL_WIDTH;
    }
    contentRect.width = current_content_width;
    contentRect.height = TABLE_HEADER_HEIGHT + (float)tableData->row_count * TABLE_ROW_HEIGHT;

    Rectangle viewArea = {0};
    GuiScrollPanel(panel_bounds, NULL, contentRect, scroll, &viewArea);

    BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

    float current_col_offset_x = 0;

    if (!hide_first_col) {
        const char *first_col_header = isValuesTableType ? "ROWID" : ((tableData->column_names[0]) ? tableData->column_names[0] : "Эпоха");
        Rectangle headerFirstScr = { viewArea.x + current_col_offset_x + scroll->x, viewArea.y + scroll->y, first_col_width, TABLE_HEADER_HEIGHT };
        GuiDrawRectangle(headerFirstScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(data->current_theme_colors.list_header_bg));
        GuiDrawText(first_col_header, GetTextBounds(TEXTBOX, headerFirstScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL)));
        current_col_offset_x += first_col_width;
    }

    for (int c = 0; c < tableData->column_count; c++) {
        const char *data_col_name = "";
        if (isValuesTableType) {
             data_col_name = (c < tableData->num_allocated_column_names) ? tableData->column_names[c] : TextFormat("Val%d", c);
        } else {
            data_col_name = ((c + 1) < tableData->num_allocated_column_names ) ? tableData->column_names[c+1] : TextFormat("X%d", c+1);
        }
        if (!data_col_name) data_col_name = "ERR_COL_NAME";

        if (isValuesTableType && strcmp(data_col_name, data->schema_blob_column_name) == 0 && HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
            continue;
        }
        Rectangle headerDataScr = { viewArea.x + current_col_offset_x + scroll->x, viewArea.y + scroll->y, TABLE_DATA_COL_WIDTH, TABLE_HEADER_HEIGHT };
        GuiDrawRectangle(headerDataScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(data->current_theme_colors.list_header_bg));
        GuiDrawText(data_col_name, GetTextBounds(TEXTBOX, headerDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL)));
        current_col_offset_x += TABLE_DATA_COL_WIDTH;
    }

    if (tableData->row_count == 0) {
         DrawTextEx(current_font, "Нет данных",
                   (Vector2){viewArea.x + PADDING + scroll->x, viewArea.y + TABLE_HEADER_HEIGHT + PADDING + scroll->y},
                   base_font_size * 0.9f, 1, GetColor(data->current_theme_colors.placeholder_text_color));
    }

    for (int r = 0; r < tableData->row_count; r++) {
        current_col_offset_x = 0;
        float screen_y_current_row = viewArea.y + TABLE_HEADER_HEIGHT + (float)r * TABLE_ROW_HEIGHT + scroll->y;

        Color row_bg_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, BASE_COLOR_FOCUSED) : GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL));
        if (r % 2 != 0 && *selectedRow != r) {
             row_bg_color = GetColor(data->current_theme_colors.list_item_alt_bg);
        }
        Color row_text_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, TEXT_COLOR_FOCUSED) : GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));

        if (!hide_first_col) {
            Rectangle cellFirstScr = { viewArea.x + current_col_offset_x + scroll->x, screen_y_current_row, first_col_width, TABLE_ROW_HEIGHT };
            GuiDrawRectangle(cellFirstScr, 0, BLANK, row_bg_color);
            if (tableData->epochs) {
                char text_to_draw_in_first_col[32];
                bool is_conclusion_table_type = (title && strcmp(title, "Итоговая таблица погрешности") == 0);
                if (is_conclusion_table_type) {
                    if (tableData->epochs[r] == -1) { strcpy(text_to_draw_in_first_col, "прогноз"); }
                    else { sprintf(text_to_draw_in_first_col, "t%d", tableData->epochs[r]); }
                } else {
                    sprintf(text_to_draw_in_first_col, "%d", tableData->epochs[r]);
                }
                GuiDrawText(text_to_draw_in_first_col, GetTextBounds(TEXTBOX, cellFirstScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
            }
            current_col_offset_x += first_col_width;
        }

        if (tableData->table && tableData->table[r]) {
            for (int c = 0; c < tableData->column_count; c++) {
                const char *current_data_column_name = NULL;
                if (isValuesTableType) {
                    if (c < tableData->num_allocated_column_names && tableData->column_names[c]) {
                        current_data_column_name = tableData->column_names[c];
                    }
                } else {
                    if ((c + 1) < tableData->num_allocated_column_names && tableData->column_names[c + 1]) {
                        current_data_column_name = tableData->column_names[c + 1];
                    }
                }
                if (isValuesTableType && current_data_column_name &&
                    strcmp(current_data_column_name, data->schema_blob_column_name) == 0 &&
                    HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
                    continue;
                }
                Rectangle cellDataScr = { viewArea.x + current_col_offset_x + scroll->x, screen_y_current_row, TABLE_DATA_COL_WIDTH, TABLE_ROW_HEIGHT };
                GuiDrawRectangle(cellDataScr, 0, BLANK, row_bg_color);
                if (isValuesTableType && current_data_column_name && strcmp(current_data_column_name, data->schema_blob_column_name) == 0) {
                    GuiDrawText("BLOB", GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                } else {
                    const char* format_string_for_double;
                    bool is_conclusion_table_type_for_data = (title && strcmp(title, "Итоговая таблица погрешности") == 0);
                    if (is_conclusion_table_type_for_data) {
                        if (c == 0 || c == 1 || c == 2) { format_string_for_double = "%.7f"; }
                        else if (c == 3) { format_string_for_double = "%.5f"; }
                        else if (c == 4) {
                            double abs_val_l = fabs(tableData->table[r][c]);
                            if (abs_val_l > 0 && (abs_val_l < 0.0001 || abs_val_l >= 100000.0)) { format_string_for_double = "%.2E"; }
                            else { format_string_for_double = "%.6f"; }
                        } else { format_string_for_double = "%.4f"; }
                    } else {
                        bool treat_as_potentially_small_original = false;
                        if (title) {
                            if (strstr(title, "Alpha") || strstr(title, "alpha") || strstr(title, "Альфа") || strstr(title, "альфа") || strstr(title, "Сглаживание α") || strstr(title, "Сглаживание Alpha") ) {
                                treat_as_potentially_small_original = true;
                            }
                        }
                        if (!treat_as_potentially_small_original && current_data_column_name) {
                            if (strstr(current_data_column_name, "Alpha") || strstr(current_data_column_name, "alpha") || strstr(current_data_column_name, "Альфа") || strstr(current_data_column_name, "альфа")) {
                                treat_as_potentially_small_original = true;
                            }
                        }
                        if (treat_as_potentially_small_original) {
                            double abs_val = fabs(tableData->table[r][c]);
                            if (abs_val > 0 && (abs_val < 0.001 || abs_val >= 100000.0)) { format_string_for_double = "%.3E"; }
                            else if (abs_val > 0 && abs_val < 1.0) { format_string_for_double = "%.5f"; }
                            else { format_string_for_double = "%.4f"; }
                        } else { format_string_for_double = "%.4f"; }
                    }
                    if (isnan(tableData->table[r][c])) {
                        GuiDrawText("N/A", GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                    } else {
                        GuiDrawText(TextFormat(format_string_for_double, tableData->table[r][c]), GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                    }
                }
                current_col_offset_x += TABLE_DATA_COL_WIDTH;
            }
        }
        Rectangle row_rect_on_screen = { viewArea.x + scroll->x, screen_y_current_row, contentRect.width, TABLE_ROW_HEIGHT };
        if (!data->show_main_edit_dialog && !data->show_values_edit_dialog) {
            if (CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointRec(GetMousePosition(), row_rect_on_screen) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                     *selectedRow = r;
                     TraceLog(LOG_INFO, "Выбрана строка: %d в таблице '%s'", r, title ? title : "N/A");
                }
            }
        }
    }
    EndScissorMode();
}

static void DrawTopNavigationBar(AppData_Raylib *data, Rectangle bounds) {
    if (!data) return;
    GuiDrawRectangle(bounds, 0, BLANK, GetColor(data->current_theme_colors.header_toolbar_bg));
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height -1), (int)(bounds.x + bounds.width), (int)(bounds.y + bounds.height -1), GetColor(data->current_theme_colors.border_universal));

    float button_y = bounds.y + (bounds.height - BUTTON_HEIGHT) / 2.0f;
    float current_x = bounds.x + PADDING;
    float button_padding = 8;
    const char *open_text_str = TextFormat("%s Открыть", GuiIconText(ICON_FILE_OPEN, NULL));
    float open_button_width = MeasureTextEx(GuiGetFont(), open_text_str, GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    if (open_button_width < 110) open_button_width = 110;
    if (GuiButton((Rectangle){current_x, button_y, open_button_width, BUTTON_HEIGHT}, open_text_str)) {
        TraceLog(LOG_INFO,"Тулбар: Кнопка 'Открыть' нажата.");
        const char * filterPatterns[3] = { "*.sqlite", "*.db", "*.sqlite3" };
        const char * selectedFilePath = tinyfd_openFileDialog("Выберите SQLite базу данных", data->current_db_path[0] != '\0' ? data->current_db_path : "", 3, filterPatterns, "SQLite файлы", 0);
        if (selectedFilePath) {
            strncpy(data->db_file_path_text_box, selectedFilePath, sizeof(data->db_file_path_text_box) - 1);
            data->db_file_path_text_box[sizeof(data->db_file_path_text_box) - 1] = '\0';
            if (data->db_opened && data->db_handle) { sqlite3_close(data->db_handle); data->db_handle = NULL; data->db_opened = false; }
            free_table_data_content_rl(&data->table_data_main);
            free_table_data_content_rl(&data->table_data_values);
            if (data->schema_texture_loaded) { UnloadTexture(data->schema_texture); data->schema_texture_loaded = false; memset(&data->schema_texture, 0, sizeof(Texture2D));}
            data->current_main_table_name[0] = '\0';
            data->main_table_selected_row = -1; data->values_table_selected_row = -1;
            data->main_table_scroll = (Vector2){0,0}; data->values_table_scroll = (Vector2){0,0};
            if (data->show_main_edit_dialog) { data->show_main_edit_dialog = false; data->main_edit_dialog_row_index = -1; }
            if (data->show_values_edit_dialog) { data->show_values_edit_dialog = false; data->values_edit_dialog_row_index = -1; }
            #define SAFE_FREE_ARRAY_TNB(arr_ptr_field, count_field) do { if(data->arr_ptr_field) { free(data->arr_ptr_field); data->arr_ptr_field = NULL; data->count_field = 0; } } while(0)
            #define SAFE_FREE_TABLE_TNB(table_ptr_field, row_count_field) \
                do { if(data->table_ptr_field) { for(int i_tnb = 0; i_tnb < data->row_count_field; i_tnb++) { if(data->table_ptr_field[i_tnb]) free(data->table_ptr_field[i_tnb]); } free(data->table_ptr_field); data->table_ptr_field = NULL; data->row_count_field = 0; } } while(0)
            SAFE_FREE_ARRAY_TNB(mu_data, mu_data_count); SAFE_FREE_ARRAY_TNB(alpha_data, alpha_count);
            SAFE_FREE_ARRAY_TNB(mu_prime_data, mu_prime_data_count); SAFE_FREE_ARRAY_TNB(alpha_prime_data, alpha_prime_data_count);
            SAFE_FREE_ARRAY_TNB(mu_double_prime_data, mu_double_prime_data_count); SAFE_FREE_ARRAY_TNB(alpha_double_prime_data, alpha_double_prime_data_count);
            SAFE_FREE_TABLE_TNB(smoothing_data, smoothing_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_data, smoothing_alpha_row_count);
            SAFE_FREE_TABLE_TNB(smoothing_mu_prime_data, smoothing_mu_prime_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_prime_data, smoothing_alpha_prime_row_count);
            SAFE_FREE_TABLE_TNB(smoothing_mu_double_prime_data, smoothing_mu_double_prime_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_double_prime_data, smoothing_alpha_double_prime_row_count);
            #undef SAFE_FREE_ARRAY_TNB
            #undef SAFE_FREE_TABLE_TNB
            for (int i = 0; i < GRAPH_ID_COUNT; i++) {
                data->graph_states[i].is_active = false;
                data->graph_states[i].scale = (Vector2){0.0f, 0.0f};
                data->graph_states[i].offset = (Vector2){0.0f, 0.0f};
            }
            TraceLog(LOG_INFO, "TNB: Состояния графиков сброшены для новой БД.");
            ResetLevel4Data(data);
            TraceLog(LOG_INFO, "TNB: Данные Уровня 4 сброшены для новой БД.");
            data->db_handle = open_database_rl(data->db_file_path_text_box);
            if (data->db_handle) {
                data->db_opened = true; strncpy(data->current_db_path, data->db_file_path_text_box, sizeof(data->current_db_path) -1); data->current_db_path[sizeof(data->current_db_path)-1] = '\0';
                sprintf(data->status_message, "БД '%s' открыта.", data->current_db_path); data->status_message_timer = 3.0f;
                if (load_table_rl(data->db_handle, &data->table_data_values, data->values_table_name)) {
                    if (data->table_data_values.row_count > 0 && data->table_data_values.epochs) {
                        int first_rowid_candidate = data->table_data_values.epochs[0];
                        BlobData_RL blob = load_blob_from_table_by_rowid_rl(data->db_handle, data->values_table_name, data->schema_blob_column_name, first_rowid_candidate);
                        if (blob.loaded && blob.data && blob.size > 0) {
                            Image img = LoadImageFromMemory(".png", blob.data, blob.size);
                            if (img.data) { data->schema_texture = LoadTextureFromImage(img); UnloadImage(img); if (data->schema_texture.id != 0) data->schema_texture_loaded = true; }
                            free_blob_data_rl(&blob);
                        }
                    }
                }
                TableList_RL table_list = get_table_list_rl(data->db_handle);
                char *main_analysis_table_to_load = NULL;
                if (table_list.count > 0) {
                    for (int i = 0; i < table_list.count; i++) { if (strcmp(table_list.names[i], data->values_table_name) != 0) { main_analysis_table_to_load = table_list.names[i]; break; }}
                    if (!main_analysis_table_to_load && table_list.count > 0) { if (strcmp(table_list.names[0], data->values_table_name) != 0) main_analysis_table_to_load = table_list.names[0]; else if (table_list.count > 1) main_analysis_table_to_load = table_list.names[1];}
                }
                if (main_analysis_table_to_load) {
                    strncpy(data->current_main_table_name, main_analysis_table_to_load, sizeof(data->current_main_table_name) - 1); data->current_main_table_name[sizeof(data->current_main_table_name) - 1] = '\0';
                    if (load_table_rl(data->db_handle, &data->table_data_main, data->current_main_table_name)) { sprintf(data->status_message, "Загружена: %s (%d строк)", data->current_main_table_name, data->table_data_main.row_count); data->status_message_timer = 3.0f;}
                    else { sprintf(data->status_message, "Ошибка загрузки: %s", data->current_main_table_name); data->status_message_timer = 3.0f; data->current_main_table_name[0] = '\0';}
                } else { strcpy(data->status_message, "Основная таблица не найдена."); data->status_message_timer = 3.0f;}
                free_table_list_rl(&table_list);
            } else {
                data->db_opened = false; sprintf(data->status_message, "Ошибка открытия БД: %s", data->db_file_path_text_box); data->status_message_timer = 3.0f;
            }
        }
    }
    current_x += open_button_width + button_padding;

    const char *levels_button_text = data->levelsDropdownActive ? "Уровни" : "Уровни";
    float levels_button_width = MeasureTextEx(GuiGetFont(), "Уровни УУ", GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    if (levels_button_width < 130) levels_button_width = 130;
    data->levelsMainButtonRect = (Rectangle){current_x, button_y, levels_button_width, BUTTON_HEIGHT};
    bool can_access_any_level = (data->db_opened && data->table_data_main.row_count > 0);
    GuiSetState(can_access_any_level ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton(data->levelsMainButtonRect, levels_button_text)) {
        if (can_access_any_level) {
            data->levelsDropdownActive = !data->levelsDropdownActive;
            TraceLog(LOG_INFO, "Тулбар: Кнопка 'Уровни' нажата, dropdown: %s", data->levelsDropdownActive ? "ОТКРЫТ" : "ЗАКРЫТ");
        }
    }
    GuiSetState(STATE_NORMAL);
    if (data->levelsDropdownActive && can_access_any_level) {
        float dropdown_item_x = data->levelsMainButtonRect.x;
        float dropdown_item_y = data->levelsMainButtonRect.y + data->levelsMainButtonRect.height + 2;
        float dropdown_item_width = data->levelsMainButtonRect.width;
        float dropdown_padding_internal = 2;
        const char *level_names[] = {"Уровень 1", "Уровень 4"};
        bool level_active_flags[] = {
            (data->db_opened && data->table_data_main.row_count > 0),
            (data->db_opened && data->table_data_main.row_count > 0)
        };
        int num_dropdown_items = sizeof(level_names) / sizeof(level_names[0]);
        float total_dropdown_height = 0;
        if (num_dropdown_items > 0) {
            total_dropdown_height = (BUTTON_HEIGHT * num_dropdown_items) + (dropdown_padding_internal * (num_dropdown_items - 1));
        }
        data->levelsDropdownRect = (Rectangle){ dropdown_item_x, dropdown_item_y, dropdown_item_width, total_dropdown_height };
        for (int i = 0; i < num_dropdown_items; ++i) {
            Rectangle item_rect = { dropdown_item_x, dropdown_item_y + i * (BUTTON_HEIGHT + dropdown_padding_internal), dropdown_item_width, BUTTON_HEIGHT };
            GuiSetState(level_active_flags[i] ? STATE_NORMAL : STATE_DISABLED);
            if (GuiButton(item_rect, level_names[i])) {
                if (level_active_flags[i]) {
                    if (i == 0) { data->current_view = APP_VIEW_LEVEL1; TraceLog(LOG_INFO, "Тулбар: Выбран 'Уровень 1'."); }
                    else if (i == 1) { data->current_view = APP_VIEW_LEVEL4; TraceLog(LOG_INFO, "Тулбар: Выбран '%s'.", level_names[i]); }
                    data->levelsDropdownActive = false;
                } else { TraceLog(LOG_INFO, "Тулбар: Попытка выбрать неактивный уровень '%s'.", level_names[i]); }
            }
            GuiSetState(STATE_NORMAL);
        }
    } else {
        data->levelsDropdownRect = (Rectangle){0,0,0,0};
    }
    current_x += levels_button_width + button_padding;

    const char *theme_text_str = TextFormat("%s Тема", GuiIconText(ICON_COLOR_BUCKET, NULL));
    float theme_button_width = MeasureTextEx(GuiGetFont(), theme_text_str, GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    if (GuiButton((Rectangle){current_x, button_y, theme_button_width, BUTTON_HEIGHT}, theme_text_str)) {
        TraceLog(LOG_INFO,"Кнопка 'Сменить тему' нажата.");
        data->current_theme_id = (data->current_theme_id + 1) % TOTAL_THEMES;
        data->current_theme_colors = app_themes[data->current_theme_id];
        if (data->current_theme_id == LIGHT_THEME_ID) {
            TraceLog(LOG_INFO, "Тема изменена на: Светлая");
        } else if (data->current_theme_id == DARK_THEME_ID) {
            TraceLog(LOG_INFO, "Тема изменена на: Темная");
        }
        ApplyGuiThemeStyles(data);
    }
    current_x += theme_button_width + button_padding;

    const char *help_text_str = GuiIconText(ICON_STEP_INTO, NULL);
    float help_button_width = BUTTON_HEIGHT;

    // Позиционируем кнопку в правом углу панели навигации
    Rectangle help_button_rect = {
        bounds.x + bounds.width - PADDING - help_button_width, // X: правый край панели минус отступ минус ширина кнопки
        button_y,                                              // Y: тот же что и у других кнопок
        help_button_width,                                     // Width
        BUTTON_HEIGHT                                          // Height
    };

    if (GuiButton(help_button_rect, help_text_str)) {
        data->show_help_window = !data->show_help_window; // Переключаем состояние видимости окна справки

        if (data->show_help_window) {
            TraceLog(LOG_INFO, "Тулбар: Окно справки ОТКРЫТО.");

            if (data->levelsDropdownActive) {
                data->levelsDropdownActive = false;
                TraceLog(LOG_DEBUG, "Тулбар: Выпадающий список уровней закрыт при открытии справки.");
            }

        } else {
            TraceLog(LOG_INFO, "Тулбар: Окно справки ЗАКРЫТО.");
        }
    }
}

static void DrawMainView(AppData_Raylib *data) {
    if (!data) return;
    float panel_y_start = TOP_NAV_BAR_HEIGHT + PADDING;
    float available_height_for_panels = (float)data->screen_height - panel_y_start - PADDING - 20;
    float main_controls_height = BUTTON_HEIGHT + PADDING;
    float panel_height = available_height_for_panels - main_controls_height;
    if (panel_height < 100) panel_height = 100;
    if (data->main_h_split_pos == 0) data->main_h_split_pos = data->screen_width * 0.65f;
    float left_panel_width = data->main_h_split_pos - PADDING - (PADDING / 2.0f);
    if (left_panel_width < 200) left_panel_width = 200;
    float right_panel_x_start = data->main_h_split_pos + (PADDING / 2.0f);
    float right_panel_width = data->screen_width - right_panel_x_start - PADDING;
    if (right_panel_width < 150) right_panel_width = 150;

    Rectangle main_table_container_bounds = { PADDING, panel_y_start, left_panel_width, panel_height };
    GuiDrawRectangle(main_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.8f) );

    if (data->db_opened && data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names) {
        Rectangle table_actual_bounds = { main_table_container_bounds.x + 1, main_table_container_bounds.y + 1, main_table_container_bounds.width - 2, main_table_container_bounds.height - 2 };
        DrawDataTable_RL(data, &data->table_data_main, NULL, table_actual_bounds, &data->main_table_scroll, &data->main_table_selected_row, false);
    } else if (data->db_opened) {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        const char* msg = "Основная таблица не загружена или пуста.";
        DrawTextEx(data->app_font, msg, (Vector2){text_bounds.x + (text_bounds.width - MeasureTextEx(data->app_font, msg, data->app_font.baseSize, 1).x)/2, text_bounds.y}, data->app_font.baseSize, 1, GetColor(data->current_theme_colors.placeholder_text_color));
    } else {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        const char* msg = "Откройте файл базы данных.";
        DrawTextEx(data->app_font, msg, (Vector2){text_bounds.x + (text_bounds.width - MeasureTextEx(data->app_font, msg, data->app_font.baseSize, 1).x)/2, text_bounds.y}, data->app_font.baseSize, 1, GetColor(data->current_theme_colors.placeholder_text_color));
    }

    if (data->show_right_panel_main_view && right_panel_width > PADDING * 2) {
        Rectangle right_panel_bounds = { right_panel_x_start, panel_y_start, right_panel_width, panel_height };
        float values_table_height_ratio = 0.35f;
        float values_table_height = right_panel_bounds.height * values_table_height_ratio;
        float min_button_area_values_edit_h = BUTTON_HEIGHT + PADDING;
        float min_schema_area_height = TABLE_HEADER_HEIGHT + 50 + PADDING;
        if (values_table_height < TABLE_HEADER_HEIGHT + TABLE_ROW_HEIGHT * 3) values_table_height = TABLE_HEADER_HEIGHT + TABLE_ROW_HEIGHT * 3;
        if (values_table_height > right_panel_bounds.height - min_button_area_values_edit_h - min_schema_area_height) {
            values_table_height = right_panel_bounds.height - min_button_area_values_edit_h - min_schema_area_height;
        }
        if (values_table_height < 0) values_table_height = 0;
        Rectangle values_table_container_bounds = { right_panel_bounds.x, right_panel_bounds.y, right_panel_bounds.width, values_table_height };
        GuiDrawRectangle(values_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.8f));
        if (data->db_opened && data->table_data_values.num_allocated_column_names > 0 && data->table_data_values.column_names) {
             Rectangle values_table_actual_bounds = { values_table_container_bounds.x + 1, values_table_container_bounds.y + 1, values_table_container_bounds.width - 2, values_table_container_bounds.height - 2 };
            DrawDataTable_RL(data, &data->table_data_values, NULL, values_table_actual_bounds, &data->values_table_scroll, &data->values_table_selected_row, true);
        } else if (data->db_opened) {
            Rectangle text_bounds_val = {values_table_container_bounds.x + PADDING, values_table_container_bounds.y + PADDING, values_table_container_bounds.width - 2*PADDING, 30};
            const char *msg_val = "Таблица 'Значения' не загружена или пуста.";
            DrawTextEx(data->app_font, msg_val, (Vector2){text_bounds_val.x + (text_bounds_val.width - MeasureTextEx(data->app_font, msg_val, data->app_font.baseSize, 1).x)/2, text_bounds_val.y}, data->app_font.baseSize, 1, GetColor(data->current_theme_colors.placeholder_text_color));
        }
        float edit_values_button_y = values_table_container_bounds.y + values_table_container_bounds.height + PADDING;
        float schema_y_start_val;
        if (edit_values_button_y + BUTTON_HEIGHT <= right_panel_bounds.y + right_panel_bounds.height - min_schema_area_height) {
            bool can_edit_values = (data->db_opened && data->table_data_values.row_count > 0 && data->values_table_selected_row != -1 && data->table_data_values.column_names);
            GuiSetState(can_edit_values ? STATE_NORMAL : STATE_DISABLED);
            if (GuiButton((Rectangle){values_table_container_bounds.x, edit_values_button_y, values_table_container_bounds.width, BUTTON_HEIGHT}, "Редактировать Значения")) {
                if (can_edit_values) {
                    data->values_edit_dialog_row_index = data->values_table_selected_row;
                    data->show_values_edit_dialog = true;
                    data->values_edit_dialog_scroll = (Vector2){0,0};
                    int field_idx = 0;
                    TableData *vals_table = &data->table_data_values;
                    for (int c = 0; c < vals_table->column_count; c++) {
                        if (field_idx >= MAX_DATA_COLUMNS_VALUES) break;
                        if (!(c < vals_table->num_allocated_column_names && vals_table->column_names[c])) continue;
                        const char *col_name = vals_table->column_names[c];
                        if (HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(col_name, data->schema_blob_column_name) == 0) continue;
                        if (vals_table->table && vals_table->table[data->values_edit_dialog_row_index] != NULL && c < vals_table->column_count) {
                             sprintf(data->values_edit_dialog_field_buffers[field_idx], "%.4f", vals_table->table[data->values_edit_dialog_row_index][c]);
                        } else { data->values_edit_dialog_field_buffers[field_idx][0] = '\0'; }
                        data->values_edit_dialog_field_edit_mode[field_idx] = false;
                        field_idx++;
                    }
                    if (field_idx == 0 && vals_table->column_count > 0) {
                        data->show_values_edit_dialog = false;
                        strcpy(data->status_message, "Нет редактируемых полей для этой строки 'Значения'.");
                        data->status_message_timer = 3.0f;
                    }
                }
            }
            GuiSetState(STATE_NORMAL);
            schema_y_start_val = edit_values_button_y + BUTTON_HEIGHT + PADDING;
        } else { schema_y_start_val = values_table_container_bounds.y + values_table_container_bounds.height + PADDING; }

        Rectangle schema_container_bounds = { right_panel_bounds.x, schema_y_start_val, right_panel_bounds.width, right_panel_bounds.y + right_panel_bounds.height - schema_y_start_val };
        if (schema_container_bounds.height < 0) schema_container_bounds.height = 0;
        if (schema_container_bounds.height > TABLE_HEADER_HEIGHT) {
            GuiDrawRectangle(schema_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.8f));
            Font font_sch_title = data->app_font_loaded ? data->app_font : GetFontDefault();
            float size_sch_title = (data->app_font_loaded ? data->app_font.baseSize : 20) * 0.9f;
            if(size_sch_title < 10) size_sch_title = 10;
            if (data->schema_texture_loaded && data->schema_texture.id != 0) {
                Rectangle img_area = { schema_container_bounds.x + PADDING, schema_container_bounds.y + PADDING, schema_container_bounds.width - 2 * PADDING, schema_container_bounds.height - 2 * PADDING };
                if (img_area.width < 0) img_area.width = 0; if (img_area.height < 0) img_area.height = 0;
                if (img_area.width > 10 && img_area.height > 10) {
                    float scale = fminf(img_area.width/(float)data->schema_texture.width, img_area.height/(float)data->schema_texture.height);
                    float dw = data->schema_texture.width * scale; float dh = data->schema_texture.height * scale;
                    Vector2 tp = {img_area.x + (img_area.width - dw)/2.0f, img_area.y + (img_area.height - dh)/2.0f};
                    DrawTexturePro(data->schema_texture, (Rectangle){0,0,(float)data->schema_texture.width,(float)data->schema_texture.height}, (Rectangle){tp.x,tp.y,dw,dh}, (Vector2){0,0},0,WHITE);
                }
            } else if (data->db_opened) {
                Rectangle text_bounds_sch = { schema_container_bounds.x + PADDING, schema_container_bounds.y + PADDING + size_sch_title + PADDING, schema_container_bounds.width - 2*PADDING, 30};
                 if (text_bounds_sch.y < schema_container_bounds.y + schema_container_bounds.height - PADDING) {
                    const char *msg_sch = "Схема не загружена.";
                    DrawTextEx(data->app_font, msg_sch, (Vector2){text_bounds_sch.x + (text_bounds_sch.width - MeasureTextEx(data->app_font, msg_sch, data->app_font.baseSize, 1).x)/2, text_bounds_sch.y}, data->app_font.baseSize, 1, GetColor(data->current_theme_colors.placeholder_text_color));
                 }
            }
        }
    }
    float control_button_y = main_table_container_bounds.y + main_table_container_bounds.height + PADDING;
    float control_button_x = main_table_container_bounds.x;
    bool can_edit_main = (data->db_opened && data->main_table_selected_row >= 0 && data->main_table_selected_row < data->table_data_main.row_count);
    GuiSetState(can_edit_main ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton((Rectangle){control_button_x, control_button_y, 180, BUTTON_HEIGHT}, "Редактировать строку")) {
        if (can_edit_main) {
            data->main_edit_dialog_row_index = data->main_table_selected_row;
            data->show_main_edit_dialog = true;
            data->main_edit_dialog_scroll = (Vector2){0,0};
            if (data->table_data_main.table && data->table_data_main.table[data->main_edit_dialog_row_index] && data->table_data_main.column_count > 0) {
                for (int c = 0; c < data->table_data_main.column_count; c++) {
                    if (c < MAX_DATA_COLUMNS_MAIN) {
                        sprintf(data->main_edit_dialog_field_buffers[c], "%.4f", data->table_data_main.table[data->main_edit_dialog_row_index][c]);
                        data->main_edit_dialog_field_edit_mode[c] = false;
                    }
                }
            } else { data->show_main_edit_dialog = false; strcpy(data->status_message, "Ошибка: нет данных для редактирования."); data->status_message_timer = 3.0f; }
        }
    }
    GuiSetState(STATE_NORMAL);
    control_button_x += 180 + PADDING;
    bool can_add_row = (data->db_opened && data->table_data_main.row_count >= 2 && data->table_data_main.column_count > 0 && data->current_main_table_name[0] != '\0');
    GuiSetState(can_add_row ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton((Rectangle){control_button_x, control_button_y, 200, BUTTON_HEIGHT}, "Добавить строку новых значений")) {
        if (can_add_row) {
            TraceLog(LOG_INFO, "Нажата кнопка 'Добавить строку новых значений'.");
            double *new_coordinates = calculate_new_row_rl(&data->table_data_main);
            if (new_coordinates) {
                int new_epoch = 0;
                if (data->table_data_main.row_count > 0 && data->table_data_main.epochs) {
                    new_epoch = data->table_data_main.epochs[0];
                    for (int i = 1; i < data->table_data_main.row_count; i++) { if (data->table_data_main.epochs[i] > new_epoch) new_epoch = data->table_data_main.epochs[i]; }
                    new_epoch++;
                } else { new_epoch = 1; }
                if (data->table_data_main.column_names && data->table_data_main.num_allocated_column_names >= (data->table_data_main.column_count + 1) ) {
                    if (insert_row_into_table_rl(data->db_handle, data->current_main_table_name, new_epoch, new_coordinates, data->table_data_main.column_count, data->table_data_main.column_names)) {
                        Vector2 old_scroll = data->main_table_scroll;
                        if (load_table_rl(data->db_handle, &data->table_data_main, data->current_main_table_name)) {
                            sprintf(data->status_message, "Новая строка (Эпоха %d) добавлена.", new_epoch); data->status_message_timer = 3.0f;
                            data->main_table_scroll = old_scroll;
                            float total_content_height = TABLE_HEADER_HEIGHT + (float)data->table_data_main.row_count * TABLE_ROW_HEIGHT;
                            float visible_height = main_table_container_bounds.height - 2;
                            if (total_content_height > visible_height && visible_height > 0) data->main_table_scroll.y = -(total_content_height - visible_height);
                            else data->main_table_scroll.y = 0;
                            if (data->main_table_scroll.y > 0) data->main_table_scroll.y = 0;
                            data->main_table_selected_row = data->table_data_main.row_count - 1;
                        } else { sprintf(data->status_message, "Ошибка перезагрузки таблицы '%s'.", data->current_main_table_name); data->status_message_timer = 3.0f; }
                    } else { sprintf(data->status_message, "Ошибка добавления строки в БД для '%s'.", data->current_main_table_name); data->status_message_timer = 3.0f; }
                } else { sprintf(data->status_message, "Ошибка: нет имен колонок для вставки в '%s'.", data->current_main_table_name); data->status_message_timer = 3.0f; }
                free(new_coordinates);
            } else { sprintf(data->status_message, "Не удалось рассчитать данные для новой строки."); data->status_message_timer = 3.0f; }
        }
    }
    GuiSetState(STATE_NORMAL);
}

static void DrawEditMainTableRowDialog(AppData_Raylib *data) {
    if (!data || !data->show_main_edit_dialog || data->main_edit_dialog_row_index < 0 ||
        !data->table_data_main.epochs || !data->table_data_main.column_names ||
        data->main_edit_dialog_row_index >= data->table_data_main.row_count) {
        if (data && data->show_main_edit_dialog) { data->show_main_edit_dialog = false; }
        return;
    }

    int dialog_width = 450;
    int max_dialog_height = data->screen_height - 100;
    int min_dialog_height = 150;
    int default_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    int text_padding_for_title = GuiGetStyle(DEFAULT, TEXT_PADDING);
    int title_bar_height = default_text_size + text_padding_for_title * 2;
    if (title_bar_height < 24) title_bar_height = 24;
    int fields_area_padding_top = PADDING;
    int fields_area_padding_bottom = PADDING;
    int buttons_area_height = BUTTON_HEIGHT + PADDING;
    int num_fields = data->table_data_main.column_count;
    int required_fields_height = num_fields * (BUTTON_HEIGHT + PADDING);
    if (required_fields_height < 0) required_fields_height = 0;
    int calculated_dialog_height = title_bar_height + fields_area_padding_top + required_fields_height + fields_area_padding_bottom + buttons_area_height;
    int dialog_height = calculated_dialog_height;
    if (dialog_height > max_dialog_height) dialog_height = max_dialog_height;
    if (dialog_height < min_dialog_height && min_dialog_height <= max_dialog_height) dialog_height = min_dialog_height;
    Rectangle dialog_rect = { (float)(data->screen_width - dialog_width) / 2.0f, (float)(data->screen_height - dialog_height) / 2.0f, (float)dialog_width, (float)dialog_height };
    bool close_dialog_request = false;
    int current_epoch_val = data->table_data_main.epochs[data->main_edit_dialog_row_index];
    bool pressed_window_close_button = GuiWindowBox(dialog_rect, TextFormat("Редактировать строку (Эпоха: %d)", current_epoch_val));
    if (pressed_window_close_button) { close_dialog_request = true; }
    Rectangle fields_panel_bounds = { dialog_rect.x + PADDING, dialog_rect.y + title_bar_height + fields_area_padding_top, dialog_rect.width - 2 * PADDING, dialog_rect.height - title_bar_height - fields_area_padding_top - fields_area_padding_bottom - buttons_area_height };
    if (fields_panel_bounds.height < 0) fields_panel_bounds.height = 0;
    Rectangle fields_content_rect = { 0, 0, fields_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), (float)required_fields_height };
    if (fields_content_rect.width < 0) fields_content_rect.width = 0;
    Rectangle fields_view_area = {0};
    GuiScrollPanel(fields_panel_bounds, NULL, fields_content_rect, &data->main_edit_dialog_scroll, &fields_view_area);
    BeginScissorMode((int)fields_view_area.x, (int)fields_view_area.y, (int)fields_view_area.width, (int)fields_view_area.height);
    float current_y_fields_content = 0; float label_width = 100;
    float textbox_x_fields_content_rel = label_width + PADDING;
    float textbox_width_fields = fields_content_rect.width - textbox_x_fields_content_rel - PADDING;
    if (textbox_width_fields < 100) textbox_width_fields = 100;
    for (int c = 0; c < num_fields; c++) {
        if (c >= MAX_DATA_COLUMNS_MAIN) break;
        const char *col_name = ((c + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[c + 1]) ? data->table_data_main.column_names[c + 1] : TextFormat("X%d", c + 1);
        float screen_y_field = fields_view_area.y + current_y_fields_content + data->main_edit_dialog_scroll.y;
        GuiLabel((Rectangle){fields_view_area.x + PADDING + data->main_edit_dialog_scroll.x, screen_y_field, label_width, BUTTON_HEIGHT}, col_name);
        if (GuiTextBox((Rectangle){fields_view_area.x + PADDING + textbox_x_fields_content_rel + data->main_edit_dialog_scroll.x, screen_y_field, textbox_width_fields, BUTTON_HEIGHT}, data->main_edit_dialog_field_buffers[c], TEXT_FIELD_BUFFER_SIZE, data->main_edit_dialog_field_edit_mode[c])) {
            data->main_edit_dialog_field_edit_mode[c] = !data->main_edit_dialog_field_edit_mode[c];
        }
        current_y_fields_content += BUTTON_HEIGHT + PADDING;
    }
    EndScissorMode();
    float button_y_dialog = dialog_rect.y + dialog_rect.height - BUTTON_HEIGHT - PADDING;
    if (GuiButton((Rectangle){dialog_rect.x + PADDING, button_y_dialog, 120, BUTTON_HEIGHT}, "Сохранить")) {
        bool all_valid = true;
        double new_values[MAX_DATA_COLUMNS_MAIN];
        for (int c = 0; c < num_fields; c++) {
            if (c >= MAX_DATA_COLUMNS_MAIN) break;
            char *endptr; char temp_buffer[TEXT_FIELD_BUFFER_SIZE];
            strncpy(temp_buffer, data->main_edit_dialog_field_buffers[c], TEXT_FIELD_BUFFER_SIZE -1);
            temp_buffer[TEXT_FIELD_BUFFER_SIZE-1] = '\0';
            for(char *p = temp_buffer; *p; ++p) if (*p == ',') *p = '.';
            new_values[c] = strtod(temp_buffer, &endptr);
            if (endptr == temp_buffer || (*endptr != '\0' && !isspace((unsigned char)*endptr))) {
                all_valid = false;
                const char *err_col_name = ((c+1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[c+1]) ? data->table_data_main.column_names[c+1] : TextFormat("X%d",c+1);
                sprintf(data->status_message, "Ошибка: неверное число в поле '%s'", err_col_name); data->status_message_timer = 3.0f;
                break;
            }
        }
        if (all_valid) {
            bool db_updated_all = true;
            for (int c = 0; c < num_fields; c++) {
                if (c >= MAX_DATA_COLUMNS_MAIN) break;
                const char *db_col_name = ((c+1) < data->table_data_main.num_allocated_column_names) ? data->table_data_main.column_names[c + 1] : NULL;
                const char *id_col_name = (data->table_data_main.num_allocated_column_names > 0) ? data->table_data_main.column_names[0] : NULL;
                int row_id_value = data->table_data_main.epochs[data->main_edit_dialog_row_index];
                if (!db_col_name || !id_col_name) { db_updated_all = false; break; }
                if (!update_table_cell_rl(data->db_handle, data->current_main_table_name, db_col_name, id_col_name, new_values[c], row_id_value)) {
                    sprintf(data->status_message, "Ошибка обновления БД для '%s'.", db_col_name); data->status_message_timer = 3.0f;
                    db_updated_all = false; break;
                }
            }
            if (db_updated_all) {
                for (int c = 0; c < num_fields; c++) {
                     if (c >= MAX_DATA_COLUMNS_MAIN) break;
                    if (data->table_data_main.table && data->table_data_main.table[data->main_edit_dialog_row_index] && c < data->table_data_main.column_count){
                        data->table_data_main.table[data->main_edit_dialog_row_index][c] = new_values[c];
                    }
                }
                strcpy(data->status_message, "Строка успешно обновлена."); data->status_message_timer = 2.0f; close_dialog_request = true;
            }
        }
    }
    if (GuiButton((Rectangle){dialog_rect.x + dialog_rect.width - PADDING - 120, button_y_dialog, 120, BUTTON_HEIGHT}, "Отмена") || IsKeyPressed(KEY_ESCAPE)) {
        close_dialog_request = true;
    }
    if (close_dialog_request) {
        data->show_main_edit_dialog = false; data->main_edit_dialog_row_index = -1;
        for (int i = 0; i < MAX_DATA_COLUMNS_MAIN; i++) { data->main_edit_dialog_field_edit_mode[i] = false; }
    }
}

static void DrawEditValuesDialog(AppData_Raylib *data) {
    if (!data || !data->show_values_edit_dialog || data->values_edit_dialog_row_index < 0 ||
        !data->table_data_values.epochs || !data->table_data_values.column_names ||
        data->values_edit_dialog_row_index >= data->table_data_values.row_count) {
        if(data) data->show_values_edit_dialog = false;
        return;
    }
    TableData *current_table = &data->table_data_values;
    int current_row_idx = data->values_edit_dialog_row_index;
    int dialog_width = 400;
    int num_editable_fields = 0;
    for (int c = 0; c < current_table->column_count; c++) {
        if (c < current_table->num_allocated_column_names && current_table->column_names[c]) {
            if (!(HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(current_table->column_names[c], data->schema_blob_column_name) == 0)) {
                num_editable_fields++;
            }
        }
    }
    int default_text_size_val = GuiGetStyle(DEFAULT, TEXT_SIZE);
    int text_padding_for_title_val = GuiGetStyle(DEFAULT, TEXT_PADDING);
    int title_bar_h = default_text_size_val + text_padding_for_title_val * 2;
    if (title_bar_h < 24) title_bar_h = 24;
    int fields_pad_top = PADDING; int fields_pad_bottom = PADDING; int buttons_area_h = BUTTON_HEIGHT + PADDING;
    int required_fields_h = num_editable_fields * (BUTTON_HEIGHT + PADDING);
    if (required_fields_h < 0) required_fields_h = 0;
    int calculated_dialog_h = title_bar_h + fields_pad_top + required_fields_h + fields_pad_bottom + buttons_area_h;
    int dialog_height = calculated_dialog_h;
    int max_h = data->screen_height - 100; int min_h = 150;
    if (dialog_height > max_h) dialog_height = max_h;
    if (dialog_height < min_h && min_h <= max_h) dialog_height = min_h;
    Rectangle dialog_rect = {
        (float)(data->screen_width - dialog_width) / 2.0f, (float)(data->screen_height - dialog_height) / 2.0f, (float)dialog_width, (float)dialog_height
    };

    bool close_dialog_req = false;
    int current_row_id_val = current_table->epochs[current_row_idx];
    bool pressed_window_close_button_val = GuiWindowBox(dialog_rect, TextFormat("Редактировать '%s' (Строка ID: %d)", data->values_table_name, current_row_id_val));
    if (pressed_window_close_button_val) 
    {
        close_dialog_req = true; 
    }
    Rectangle fields_panel_bounds = {
        dialog_rect.x + PADDING, dialog_rect.y + title_bar_h + fields_pad_top, dialog_rect.width - 2 * PADDING, dialog_rect.height - title_bar_h - fields_pad_top - fields_pad_bottom - buttons_area_h
    };

    if (fields_panel_bounds.height < 0) fields_panel_bounds.height = 0;
    Rectangle fields_content_rect = {
        0, 0, fields_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), (float)required_fields_h
    };
    if (fields_content_rect.width < 0) fields_content_rect.width = 0;
    Rectangle fields_view_area = {0};

    GuiScrollPanel(fields_panel_bounds, NULL, fields_content_rect, &data->values_edit_dialog_scroll, &fields_view_area);
    BeginScissorMode((int)fields_view_area.x, (int)fields_view_area.y, (int)fields_view_area.width, (int)fields_view_area.height);
    float current_y_fields_content = 0; float label_w = 100;
    float textbox_x_fields_content_rel = label_w + PADDING;
    float textbox_w_fields = fields_content_rect.width - textbox_x_fields_content_rel - PADDING;
    if (textbox_w_fields < 100) textbox_w_fields = 100;
    int field_buffer_idx = 0;
    for (int c = 0; c < current_table->column_count; c++) {
        if (field_buffer_idx >= MAX_DATA_COLUMNS_VALUES) break;
        if (!(c < current_table->num_allocated_column_names && current_table->column_names[c])) continue;
        const char *col_name = current_table->column_names[c];
        if (HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(col_name, data->schema_blob_column_name) == 0) continue;
        float screen_y_field = fields_view_area.y + current_y_fields_content + data->values_edit_dialog_scroll.y;
        GuiLabel((Rectangle){fields_view_area.x + PADDING + data->values_edit_dialog_scroll.x, screen_y_field, label_w, BUTTON_HEIGHT}, col_name);
        if (GuiTextBox((Rectangle){fields_view_area.x + PADDING + textbox_x_fields_content_rel + data->values_edit_dialog_scroll.x, screen_y_field, textbox_w_fields, BUTTON_HEIGHT}, data->values_edit_dialog_field_buffers[field_buffer_idx], TEXT_FIELD_BUFFER_SIZE, data->values_edit_dialog_field_edit_mode[field_buffer_idx])) {
            data->values_edit_dialog_field_edit_mode[field_buffer_idx] = !data->values_edit_dialog_field_edit_mode[field_buffer_idx];
        }
        current_y_fields_content += BUTTON_HEIGHT + PADDING;
        field_buffer_idx++;
    }
    EndScissorMode();
    float button_y_dialog = dialog_rect.y + dialog_rect.height - BUTTON_HEIGHT - PADDING;
    if (GuiButton((Rectangle){dialog_rect.x + PADDING, button_y_dialog, 120, BUTTON_HEIGHT}, "Сохранить")) {
        bool all_valid = true;
        double new_values_arr[MAX_DATA_COLUMNS_VALUES];
        int current_field_buffer_idx_val = 0;
        for (int c_val = 0; c_val < current_table->column_count; c_val++) {
            if (current_field_buffer_idx_val >= MAX_DATA_COLUMNS_VALUES) break;
            if (!(c_val < current_table->num_allocated_column_names && current_table->column_names[c_val])) continue;
            const char *col_name_for_val = current_table->column_names[c_val];
            if (HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(col_name_for_val, data->schema_blob_column_name) == 0) continue;
            char *endptr_val; char temp_buffer_val[TEXT_FIELD_BUFFER_SIZE];
            strncpy(temp_buffer_val, data->values_edit_dialog_field_buffers[current_field_buffer_idx_val], TEXT_FIELD_BUFFER_SIZE -1);
            temp_buffer_val[TEXT_FIELD_BUFFER_SIZE-1] = '\0';
            for(char *p_val = temp_buffer_val; *p_val; ++p_val) if (*p_val == ',') *p_val = '.';
            new_values_arr[current_field_buffer_idx_val] = strtod(temp_buffer_val, &endptr_val);
            if (endptr_val == temp_buffer_val || (*endptr_val != '\0' && !isspace((unsigned char)*endptr_val))) {
                all_valid = false; sprintf(data->status_message, "Ошибка: неверное число в поле '%s'", col_name_for_val); data->status_message_timer = 3.0f;
                break;
            }
            current_field_buffer_idx_val++;
        }
        if (all_valid) {
            bool db_updated_all_val = true;
            current_field_buffer_idx_val = 0;
            for (int c_db = 0; c_db < current_table->column_count; c_db++) {
                if (current_field_buffer_idx_val >= MAX_DATA_COLUMNS_VALUES) break;
                if (!(c_db < current_table->num_allocated_column_names && current_table->column_names[c_db])) continue;
                const char *db_col_name_val = current_table->column_names[c_db];
                if (HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(db_col_name_val, data->schema_blob_column_name) == 0) continue;
                int row_id_to_update = current_table->epochs[current_row_idx];
                if (!update_table_cell_rl(data->db_handle, data->values_table_name, db_col_name_val, NULL, new_values_arr[current_field_buffer_idx_val], row_id_to_update)) {
                    db_updated_all_val = false; sprintf(data->status_message, "Ошибка БД для '%s'", db_col_name_val); data->status_message_timer = 3.0f;
                    break;
                }
                current_field_buffer_idx_val++;
            }
            if (db_updated_all_val) {
                current_field_buffer_idx_val = 0;
                for (int c_mem = 0; c_mem < current_table->column_count; c_mem++) {
                    if (current_field_buffer_idx_val >= MAX_DATA_COLUMNS_VALUES) break;
                     if (!(c_mem < current_table->num_allocated_column_names && current_table->column_names[c_mem])) continue;
                    const char *col_name_to_update_mem_val = current_table->column_names[c_mem];
                    if (HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE && strcmp(col_name_to_update_mem_val, data->schema_blob_column_name) == 0) continue;
                    if(current_table->table && current_table->table[current_row_idx] && c_mem < current_table->column_count) {
                         current_table->table[current_row_idx][c_mem] = new_values_arr[current_field_buffer_idx_val];
                    }
                    current_field_buffer_idx_val++;
                }
                sprintf(data->status_message, "Таблица '%s' обновлена для ROWID %d.", data->values_table_name, current_row_id_val);
                data->status_message_timer = 2.0f; close_dialog_req = true;
            }
        }
    }
    if (GuiButton((Rectangle){dialog_rect.x + dialog_rect.width - PADDING - 120, button_y_dialog, 120, BUTTON_HEIGHT}, "Отмена") || IsKeyPressed(KEY_ESCAPE)) {
        close_dialog_req = true;
    }
    if (close_dialog_req) {
        data->show_values_edit_dialog = false; data->values_edit_dialog_row_index = -1;
         for (int i = 0; i < MAX_DATA_COLUMNS_VALUES; i++) { data->values_edit_dialog_field_edit_mode[i] = false; }
    }
}

static void UpdateDrawAppFrame(AppData_Raylib *data) {
    if (!data) return;

    // --- Обновление состояния ---
    if (data->status_message_timer > 0) {
        data->status_message_timer -= GetFrameTime();
        if (data->status_message_timer <= 0) data->status_message[0] = '\0';
    }

    if (data->levelsDropdownActive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePoint = GetMousePosition();
        bool clickedOnMainToggleButton = CheckCollisionPointRec(mousePoint, data->levelsMainButtonRect);
        bool clickedInsideDropdownPanel = CheckCollisionPointRec(mousePoint, data->levelsDropdownRect);
        if (!clickedOnMainToggleButton && !clickedInsideDropdownPanel) {
            data->levelsDropdownActive = false;
            TraceLog(LOG_INFO, "Dropdown уровней закрыт по клику вне области.");
        }
    }

    bool main_edit_dialog_active = data->show_main_edit_dialog;
    bool values_edit_dialog_active = data->show_values_edit_dialog;
    bool help_window_active = data->show_help_window;

    bool any_modal_is_blocking_ui = main_edit_dialog_active || 
                                    values_edit_dialog_active || 
                                    help_window_active;

    if (any_modal_is_blocking_ui) {
        GuiLock();
    }

    // --- Начало отрисовки ---
    BeginDrawing();
    ClearBackground(GetColor(data->current_theme_colors.window_background));

    // --- Отрисовка основного вида ---
    switch (data->current_view) {
        case APP_VIEW_MAIN:   DrawMainView(data); break;
        case APP_VIEW_LEVEL1: DrawLevel1View_Entry(data); break;
        case APP_VIEW_LEVEL4: DrawLevel4View_Entry(data); break;
        default: {
            Font font_err = data->app_font_loaded ? data->app_font : GetFontDefault();
            float size_err = data->app_font_loaded ? data->app_font.baseSize : 20;
            if (size_err < 10) size_err = 10;
            DrawTextEx(font_err, "Неизвестное состояние View!",
                       (Vector2){20, TOP_NAV_BAR_HEIGHT + 20.0f}, size_err, 1,
                       GetColor(data->current_theme_colors.error_text_color));
        } break;
    }


    Rectangle top_nav_bar_bounds = {0, 0, (float)data->screen_width, TOP_NAV_BAR_HEIGHT};
    DrawTopNavigationBar(data, top_nav_bar_bounds);

    if (any_modal_is_blocking_ui) {
        GuiUnlock();
    }

    // --- Обработка и отрисовка модальных окон (диалоги И справка) ---
    if (any_modal_is_blocking_ui) {
        DrawRectangle(0, 0, data->screen_width, data->screen_height, GetColor(data->current_theme_colors.modal_overlay_color));
    }

    // Отрисовка диалогов редактирования 
    if (main_edit_dialog_active) {
        DrawEditMainTableRowDialog(data);
    } else if (values_edit_dialog_active) { 
        DrawEditValuesDialog(data);
    }

    // Отрисовка окна справки
    if (help_window_active) {
        DrawHelpWindow(data);
    }

    // --- Статусная строка и FPS ---
    if (data->status_message_timer > 0 && data->status_message[0] != '\0') {
        GuiStatusBar((Rectangle){0, (float)data->screen_height - 20, (float)data->screen_width, 20}, data->status_message);
    }  

    int fps_y_pos = 10;
    if (TOP_NAV_BAR_HEIGHT > 0 && TOP_NAV_BAR_HEIGHT <= 20) { 
        if (data->current_theme_id == DARK_THEME_ID) {
            fps_y_pos = TOP_NAV_BAR_HEIGHT + 2;
        }
    } else if (TOP_NAV_BAR_HEIGHT == 0) {
        fps_y_pos = 10;
    }

    DrawFPS(data->screen_width - 90, fps_y_pos);
    
    EndDrawing();
}

//справка
static void DrawHelpWindow(AppData_Raylib *data) {
    if (!data || !data->show_help_window) return;

    int window_width = 550;
    int window_height = 450;
    Rectangle window_bounds = {
        (float)(data->screen_width - window_width) / 2.0f,
        (float)(data->screen_height - window_height) / 2.0f,
        (float)window_width,
        (float)window_height
    };
    
    bool window_should_close = GuiWindowBox(window_bounds, TextFormat("%s Справка по программе", GuiIconText(ICON_INFO, NULL)));
    if (window_should_close || IsKeyPressed(KEY_ESCAPE)) {
        data->show_help_window = false;
        TraceLog(LOG_INFO, "Окно справки закрыто (из DrawHelpWindow).");
        return;
    }

    float text_padding = 10.0f;
    const float title_bar_approx_height = 28.0f;

    Rectangle text_area_bounds = {
        window_bounds.x + text_padding,
        window_bounds.y + title_bar_approx_height + text_padding, 
        window_bounds.width - 2 * text_padding,
        window_bounds.height - title_bar_approx_height - 2 * text_padding - (BUTTON_HEIGHT + text_padding)
    };
    if (text_area_bounds.height < 0) text_area_bounds.height = 0;

const char *help_sections[] = {
        "ОБЩЕЕ УПРАВЛЕНИЕ:",
        "- 'Открыть': Загрузка файла базы данных SQLite (форматы: .sqlite, .db).",
        "- 'Уровни': Выбор режима анализа данных (Уровень 1, Уровень 4).",
        "- 'Тема': Переключение цветовой схемы интерфейса (светлая/тёмная).",
        "- Верхняя правая кнопка (i): Показать/скрыть это окно справки.",
        "",
        "ГЛАВНЫЙ ЭКРАН ('Исходные данные'):",
        "- Левая панель: Таблица с основными данными из файла.",
        "  - Клик по строке: Выбор строки для редактирования.",
        "- Правая панель:",
        "  - Верхняя часть: Таблица 'Значения' (содержит, например, E, A).", 
        "    - Клик по строке: Выбор строки для редактирования.",
        "  - Нижняя часть: Изображение схемы (если загружено из BLOB).",
        "- Кнопки под левой панелью:",
        "  - 'Редактировать строку': Открывает диалог изменения значений выбранной строки основной таблицы.",
        "  - 'Добавить строку...': Рассчитывает и добавляет новую строку в основную таблицу.", 
        "- Кнопка под таблицей 'Значения':",
        "  - 'Редактировать Значения': Открывает диалог изменения значений выбранной строки таблицы 'Значения'.",
        "",
        "УРОВЕНЬ 1 (Анализ):",
        "- Кнопка '< Назад': Возврат на главный экран ('Исходные данные').", 
        "- Вкладки слева: 'Основные данные', '+E', '-E', 'Заключение'.",
        "  Каждая вкладка отображает таблицы и графики соответствующих расчётов.", 
        "- Графики Уровня 1: Масштабирование колесиком мыши, панорамирование левой кнопкой мыши (зажать и двигать).",
        "",
        "УРОВЕНЬ 4 (Детальный анализ временных рядов):",
        "- Кнопка '< Назад': Возврат на главный экран ('Исходные данные').", 
        "- Панель слева: Список временных рядов. Клик для выбора/отмены выбора ряда для графика.", 
        "- Основной график:",
        "  - Панорамирование: Средней кнопкой мыши (зажать и двигать).", 
        "  - Кнопка 'Вписать график': Автомасштабирование для отображения всех выбранных рядов.", 
        "  - Зум колесиком мыши:",
        "    - Колесико: Зум по осям X и Y (относительно курсора).",
        "    - SHIFT + Колесико: Зум только по оси Y.",
        "    - CTRL + Колесико: Зум только по оси X.",
        "  - Зум клавиатурой (относительно центра графика):",
        "    - PAGE UP / PAGE DOWN: Зум по оси Y.",
        "    - CTRL + Стрелка ВВЕРХ / CTRL + Стрелка ВНИЗ: Зум по оси X.",
        "    - '+' / '-': Зум по осям X и Y.",
        "",
        "РЕДАКТИРОВАНИЕ ДАННЫХ:",
        "- В диалогах редактирования используйте поля для ввода новых значений.",
        "- 'Сохранить': Применяет изменения в базе данных и в текущем представлении.",
        "- 'Отмена': Закрывает диалог без сохранения.", 
        NULL
    };

    float total_text_height = 0;
    Font help_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float help_font_size = GuiGetStyle(DEFAULT, TEXT_SIZE) * 0.9f; 
    if (help_font_size < 12) help_font_size = 12;
    float help_line_spacing = help_font.baseSize > 0 ? (help_font_size + help_font_size * 0.3f) : help_font_size * 1.3f;

    for (int i = 0; help_sections[i] != NULL; ++i) {
        if (strlen(help_sections[i]) == 0) {
            total_text_height += help_line_spacing * 0.5f;
        } else {
            total_text_height += help_line_spacing;
        }
    }
    if (total_text_height < text_area_bounds.height) total_text_height = text_area_bounds.height;

    Rectangle scroll_content_rect = {0, 0, text_area_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), total_text_height};
    Rectangle scroll_view_rect = {0};
    static Vector2 help_scroll = {0, 0};

    GuiScrollPanel(text_area_bounds, NULL, scroll_content_rect, &help_scroll, &scroll_view_rect);

    BeginScissorMode((int)scroll_view_rect.x, (int)scroll_view_rect.y, (int)scroll_view_rect.width, (int)scroll_view_rect.height);
    {
        float current_text_y = scroll_view_rect.y + help_scroll.y;
        Color text_color = GetColor(data->current_theme_colors.text_primary);
        Color header_color = GetColor(data->current_theme_colors.text_primary); 

        for (int i = 0; help_sections[i] != NULL; ++i) {
            if (strlen(help_sections[i]) == 0) {
                current_text_y += help_line_spacing * 0.5f;
                continue;
            }
            
            bool is_header = help_sections[i][strlen(help_sections[i])-1] == ':';
            Color current_line_color = is_header ? header_color : text_color;
            float current_font_size = is_header ? help_font_size * 1.05f : help_font_size;
            
            DrawTextEx(help_font, help_sections[i], 
                       (Vector2){scroll_view_rect.x + 5, current_text_y}, 
                       current_font_size, 1.0f, current_line_color);

            current_text_y += help_line_spacing;
        }
    }
    EndScissorMode();

    if (GuiButton((Rectangle){window_bounds.x + (window_bounds.width - 100)/2.0f, 
                               window_bounds.y + window_bounds.height - text_padding - BUTTON_HEIGHT, 
                               100, BUTTON_HEIGHT}, "Закрыть")) {
        data->show_help_window = false;
        TraceLog(LOG_INFO, "Окно справки закрыто кнопкой 'Закрыть'.");
    }
}