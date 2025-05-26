#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#include "app_data_rl.h"
#include "sqlite_utils_rl.h"
#include "calculations_rl.h"
#include "level1_analysis_rl.h" // Когда появится

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tinyfiledialogs.h"
#include <ctype.h>
#include <math.h>

// --- Прототипы функций ---
static void InitApp(AppData_Raylib *data);
static void UpdateDrawAppFrame(AppData_Raylib *data);
static void ShutdownApp(AppData_Raylib *data);
static void DrawMainView(AppData_Raylib *data);
//static void DrawLevel1View(AppData_Raylib *data);
static void DrawEditMainTableRowDialog(AppData_Raylib *data);
static void DrawEditValuesDialog(AppData_Raylib *data);



// Глобальные константы для UI
#define BUTTON_HEIGHT 30
#define PADDING 10

// Константы для отрисовки таблицы
#define TABLE_HEADER_HEIGHT 25.0f
#define TABLE_ROW_HEIGHT 22.0f
#define TABLE_EPOCH_COL_WIDTH 70.0f
#define TABLE_DATA_COL_WIDTH 90.0f
#define TABLE_CELL_TEXT_PADDING_X 5.0f
#define HIDE_ROWID_COLUMN_FOR_VALUES_TABLE 1
#define HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE 1
//#define TOP_NAV_BAR_HEIGHT 40

// --- main ---
int main(void) {
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
        for (int j = 0; j < 3; j++) { // У нас 3 таблицы на вкладке "Основные данные"
            data->l1_tables_scroll[i][j] = (Vector2){0.0f, 0.0f};
        }
    }

    data->screen_width = 1280;
    data->screen_height = 720;
    data->current_view = APP_VIEW_MAIN;
    data->l1_active_tab = L1_TAB_INITIAL_DATA;

    strcpy(data->db_file_path_text_box, "Вариант28.sqlite");
    data->db_file_path_edit_mode = false;
    strcpy(data->values_table_name, "Значения");
    strcpy(data->schema_blob_column_name, "Схема");
    data->main_h_split_pos = (float)data->screen_width * 0.65f;
    data->show_right_panel_main_view = true;

    for (int i = 0; i < GRAPH_ID_COUNT; i++) {
        data->graph_states[i].scale = (Vector2){1.0f, 1.0f};
        data->graph_states[i].offset = (Vector2){0.0f, 0.0f};
        data->graph_states[i].is_active = false;
    }
    
    data->main_table_selected_row = -1;
    data->values_table_selected_row = -1;
    data->app_font_loaded = false;
    data->main_table_scroll = (Vector2){0, 0};
    data->values_table_scroll = (Vector2){0, 0};
    data->main_edit_dialog_scroll = (Vector2){0,0};

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

    InitWindow(data->screen_width, data->screen_height, "Анализ данных временных рядов (Raylib)");
    SetTargetFPS(60);

    GuiLoadStyleDefault(); 

    // 1. Основной фон для панелей, контейнеров, диалогов
    unsigned int clr_panel_background     = 0xf1f2f2FF; // HEX: #f1f2f2

    // 2. Фон для строк таблиц/списков
    unsigned int clr_list_item_bg         = 0xe3e8ffFF; // HEX: #e3e8ff

    // 3. Основной цвет текста
    unsigned int clr_text_primary         = 0x303032FF; // HEX: #303032

    // 4. Цвет для всех рамок (панелей, кнопок, списков, скроллбаров)
    unsigned int clr_border_universal     = 0x080808FF; // HEX: #080808

    // 5. Базовый фон для кнопок в обычном состоянии
    unsigned int clr_button_base_normal   = 0xdcdbe1FF; // HEX: #dcdbe1

    // --- Производные и специфические цвета ---

    // Фон для заголовков окон (GuiWindowBox) и верхнего тулбара
    // Используем тот же цвет, что и для кнопок для консистентности или clr_panel_background, если нужен тот же фон
    unsigned int clr_header_toolbar_bg  = clr_button_base_normal; // т.е. 0xdcdbe1FF

    // Текст на кнопке в обычном состоянии
    unsigned int clr_text_on_button_normal= clr_text_primary;     // т.е. 0x303032FF

    // Текст на кнопке в нажатом/активном состоянии (светлый, как фон панелей)
    unsigned int clr_text_on_button_active= clr_panel_background; // т.е. 0xf1f2f2FF

    // Цвет для неактивного/отключенного текста (можно оставить ваш предыдущий или сделать производным)
    unsigned int clr_text_disabled        = 0xA0A0A0FF; // Как в вашем предыдущем варианте, или ColorToInt(Fade(GetColor(clr_text_primary), 0.5f))

    // Цвет рамки для активных/фокусированных элементов (например, TextBox), оставим акцентный синий
    unsigned int clr_border_focused_accent= 0x007BFFFF; // HEX: #007BFF

    // Цвета для состояний кнопки
    unsigned int clr_button_base_focused  = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal), -0.1f)); // Чуть темнее normal
    unsigned int clr_button_base_pressed  = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal), -0.30f)); // Значительно темнее normal, для контраста со светлым текстом clr_text_on_button_active
    unsigned int clr_button_base_disabled = ColorToInt(Fade(GetColor(clr_button_base_normal), 0.4f));   // Блеклый normal
    unsigned int clr_button_text_disabled = ColorToInt(Fade(GetColor(clr_text_on_button_normal), 0.5f)); // Блеклый текст кнопки

    // Цвета для таблиц/списков (ListView)
    unsigned int clr_list_item_alt_bg     = ColorToInt(ColorBrightness(GetColor(clr_list_item_bg), -0.05f)); // Чуть темнее фона строк
    unsigned int clr_list_item_selected_bg= ColorToInt(ColorBrightness(GetColor(clr_list_item_bg), -0.2f)); // Заметно темнее фона строк для выделения (или ваш 0xD3D3D3FF)
    unsigned int clr_list_item_selected_text = clr_text_primary;       // Текст на выделенной строке
    unsigned int clr_list_header_bg       = clr_header_toolbar_bg;    // Фон заголовка таблицы (как у тулбара)
    unsigned int clr_list_selected_border = clr_border_focused_accent;// Рамка выделенной строки (акцентная)

    // Цвета для скроллбара (Scrollbar)
    unsigned int clr_scrollbar_track_bg   = ColorToInt(ColorBrightness(GetColor(clr_header_toolbar_bg), -0.05f)); // Чуть темнее фона тулбара
    unsigned int clr_scrollbar_thumb_bg   = clr_button_base_normal;   // Бегунок как обычная кнопка
    unsigned int clr_scrollbar_thumb_focused = ColorToInt(ColorBrightness(GetColor(clr_scrollbar_thumb_bg), -0.1f)); // Бегунок при фокусе
    unsigned int clr_scrollbar_thumb_pressed = ColorToInt(ColorBrightness(GetColor(clr_scrollbar_thumb_bg), -0.2f)); // Бегунок при нажатии


    // --- Применение стилей ---
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, clr_panel_background);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, clr_text_primary);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, clr_border_universal); // Универсальный цвет рамки
    GuiSetStyle(DEFAULT, LINE_COLOR, clr_border_universal);         // Линии тоже универсальной рамкой

    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, clr_button_base_normal);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, clr_text_on_button_normal);
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, clr_border_universal);    // Универсальный цвет рамки
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, clr_button_base_focused);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, clr_text_on_button_normal); // Текст при фокусе тот же, что и normal
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, clr_border_universal);    // Рамка при фокусе та же
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, clr_button_base_pressed);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, clr_text_on_button_active); // Специальный светлый текст для нажатой кнопки
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, clr_border_universal);    // Рамка при нажатии та же
    GuiSetStyle(BUTTON, TEXT_COLOR_DISABLED, clr_button_text_disabled);
    GuiSetStyle(BUTTON, BASE_COLOR_DISABLED, clr_button_base_disabled);

    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, 0xFFFFFFFF);             // Белый фон для текстовых полей для читаемости
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, clr_border_universal); // Универсальный цвет рамки
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, clr_text_primary);
    GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED, clr_text_primary);
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, clr_border_focused_accent); // Акцентная рамка при фокусе
    GuiSetStyle(TEXTBOX, TEXT_COLOR_DISABLED, clr_text_disabled);

    GuiSetStyle(LISTVIEW, BACKGROUND_COLOR, clr_list_item_bg);        // Фон таблицы/списка
    GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, clr_list_item_bg);        // Фон строк
    GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL, clr_border_universal);   // Универсальный цвет рамки
    GuiSetStyle(LISTVIEW, TEXT_COLOR_NORMAL, clr_text_primary);
    GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, clr_list_item_selected_bg); // Фон выделенной строки
    GuiSetStyle(LISTVIEW, TEXT_COLOR_FOCUSED, clr_list_item_selected_text);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_FOCUSED, clr_list_selected_border);
    GuiSetStyle(LISTVIEW, BASE_COLOR_DISABLED, clr_list_item_alt_bg);  // Фон для чередующихся или неактивных строк

    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, clr_text_primary);

    GuiSetStyle(SCROLLBAR, BASE_COLOR_NORMAL, clr_scrollbar_track_bg);
    GuiSetStyle(SCROLLBAR, BORDER_COLOR_NORMAL, clr_border_universal); // Универсальный цвет рамки
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, clr_scrollbar_thumb_bg);    // Бегунок скроллбара
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, clr_scrollbar_thumb_bg);    // (обычно не используется для thumb)
    GuiSetStyle(SLIDER, BASE_COLOR_FOCUSED, clr_scrollbar_thumb_focused);
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, clr_scrollbar_thumb_pressed);
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL, clr_border_universal);    // Универсальный цвет рамки для бегунка

    GuiSetStyle(ICON_BOX, BACKGROUND_COLOR, clr_panel_background);
    GuiSetStyle(ICON_BOX, BORDER_COLOR_NORMAL, clr_border_universal);
    GuiSetStyle(ICON_BOX, TEXT_COLOR_NORMAL, clr_text_primary);

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
    SAFE_FREE_TABLE_SHUTDOWN(error_adjusted_table_data, error_adjusted_rows_alloc);
    SAFE_FREE_TABLE_SHUTDOWN(error_subtracted_table_data, error_subtracted_rows_alloc);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_data, smoothing_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_data, smoothing_alpha_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_mu_prime_data, smoothing_mu_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_prime_data, smoothing_alpha_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_mu_double_prime_data, smoothing_mu_double_prime_row_count);
    SAFE_FREE_TABLE_SHUTDOWN(smoothing_alpha_double_prime_data, smoothing_alpha_double_prime_row_count);
    #undef SAFE_FREE_ARRAY_SHUTDOWN
    #undef SAFE_FREE_TABLE_SHUTDOWN
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Вычисленные массивы и таблицы освобождены.");

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
        DrawTextEx(current_font, "TableData is NULL or column_names missing", (Vector2){panel_bounds.x + PADDING, panel_bounds.y + PADDING}, base_font_size, 1, RED);
        return;
    }

    int first_col_width = TABLE_EPOCH_COL_WIDTH;
    bool hide_first_col = (isValuesTableType && HIDE_ROWID_COLUMN_FOR_VALUES_TABLE); 

    if (title && title[0] != '\0') {
         DrawTextEx(current_font, title, (Vector2){panel_bounds.x, panel_bounds.y - TABLE_HEADER_HEIGHT - 2}, base_font_size, 1, BLACK);
    }

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
        GuiDrawRectangle(headerFirstScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
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
        if (!data_col_name) data_col_name = "ERR_COL_NAME"; // Failsafe
        
        if (isValuesTableType && strcmp(data_col_name, data->schema_blob_column_name) == 0 && HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
            continue; 
        }
        Rectangle headerDataScr = { viewArea.x + current_col_offset_x + scroll->x, viewArea.y + scroll->y, TABLE_DATA_COL_WIDTH, TABLE_HEADER_HEIGHT };
        GuiDrawRectangle(headerDataScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
        GuiDrawText(data_col_name, GetTextBounds(TEXTBOX, headerDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL)));
        current_col_offset_x += TABLE_DATA_COL_WIDTH;
    }

    if (tableData->row_count == 0) { 
         DrawTextEx(current_font, "Нет данных", 
                   (Vector2){viewArea.x + PADDING + scroll->x, viewArea.y + TABLE_HEADER_HEIGHT + PADDING + scroll->y}, 
                   base_font_size * 0.9f, 1, GRAY);
    }

    for (int r = 0; r < tableData->row_count; r++) {
        current_col_offset_x = 0; 
        float screen_y_current_row = viewArea.y + TABLE_HEADER_HEIGHT + (float)r * TABLE_ROW_HEIGHT + scroll->y;

        Color row_bg_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, BASE_COLOR_FOCUSED) : GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL));
        if (r % 2 != 0 && *selectedRow != r) row_bg_color = ColorAlphaBlend(row_bg_color, GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_DISABLED)), Fade(WHITE, 0.1f));
        Color row_text_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, TEXT_COLOR_FOCUSED) : GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));

        if (!hide_first_col) {
            Rectangle cellFirstScr = { viewArea.x + current_col_offset_x + scroll->x, screen_y_current_row, first_col_width, TABLE_ROW_HEIGHT };
            GuiDrawRectangle(cellFirstScr, 0, BLANK, row_bg_color);
            if (tableData->epochs) GuiDrawText(TextFormat("%d", tableData->epochs[r]), GetTextBounds(TEXTBOX, cellFirstScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
            current_col_offset_x += first_col_width;
        }

        if (tableData->table && tableData->table[r]) {
            for (int c = 0; c < tableData->column_count; c++) { 
                const char *header_name_for_check = "";
                 if (isValuesTableType && c < tableData->num_allocated_column_names && tableData->column_names[c]) {
                     header_name_for_check = tableData->column_names[c];
                }
                if (!header_name_for_check) header_name_for_check = ""; // Failsafe
                
                if (isValuesTableType && strcmp(header_name_for_check, data->schema_blob_column_name) == 0 && HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
                    continue; 
                }

                Rectangle cellDataScr = { viewArea.x + current_col_offset_x + scroll->x, screen_y_current_row, TABLE_DATA_COL_WIDTH, TABLE_ROW_HEIGHT };
                GuiDrawRectangle(cellDataScr, 0, BLANK, row_bg_color);
                
                if (isValuesTableType && strcmp(header_name_for_check, data->schema_blob_column_name) == 0) { 
                    GuiDrawText("BLOB", GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                } else {
                     if (c < tableData->column_count) { 
                        GuiDrawText(TextFormat("%.4f", tableData->table[r][c]), GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                     } else { 
                        GuiDrawText("ERR", GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), RED); 
                     }
                }
                current_col_offset_x += TABLE_DATA_COL_WIDTH;
            }
        }
        
        Rectangle row_rect_on_screen = {
            viewArea.x + scroll->x, screen_y_current_row, contentRect.width, TABLE_ROW_HEIGHT
        };

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
    GuiDrawRectangle(bounds, 0, BLANK, GetColor(0xE9ECEFFF)); 
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height -1), (int)(bounds.x + bounds.width), (int)(bounds.y + bounds.height -1), GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
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
            SAFE_FREE_TABLE_TNB(error_adjusted_table_data, error_adjusted_rows_alloc); SAFE_FREE_TABLE_TNB(error_subtracted_table_data, error_subtracted_rows_alloc);
            SAFE_FREE_TABLE_TNB(smoothing_data, smoothing_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_data, smoothing_alpha_row_count);
            SAFE_FREE_TABLE_TNB(smoothing_mu_prime_data, smoothing_mu_prime_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_prime_data, smoothing_alpha_prime_row_count);
            SAFE_FREE_TABLE_TNB(smoothing_mu_double_prime_data, smoothing_mu_double_prime_row_count); SAFE_FREE_TABLE_TNB(smoothing_alpha_double_prime_data, smoothing_alpha_double_prime_row_count);
            #undef SAFE_FREE_ARRAY_TNB
            #undef SAFE_FREE_TABLE_TNB
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
    const char *level1_text_str = "Уровень 1"; 
    float level1_button_width = MeasureTextEx(GuiGetFont(), level1_text_str, GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    GuiSetState((data->db_opened && data->table_data_main.row_count > 0) ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton((Rectangle){current_x, button_y, level1_button_width, BUTTON_HEIGHT}, level1_text_str)) {
        if (data->db_opened && data->table_data_main.row_count > 0) data->current_view = APP_VIEW_LEVEL1;
    }
    GuiSetState(STATE_NORMAL);
    current_x += level1_button_width + button_padding;
    const char *level2_text_str = "Уровень 2";
    float level2_button_width = MeasureTextEx(GuiGetFont(), level2_text_str, GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    GuiSetState(STATE_DISABLED);
    GuiButton((Rectangle){current_x, button_y, level2_button_width, BUTTON_HEIGHT}, level2_text_str);
    GuiSetState(STATE_NORMAL);
    current_x += level2_button_width + button_padding;
    const char *theme_text_str = TextFormat("%s Тема", GuiIconText(ICON_COLOR_BUCKET, NULL)); 
    float theme_button_width = MeasureTextEx(GuiGetFont(), theme_text_str, GuiGetStyle(DEFAULT, TEXT_SIZE), GuiGetStyle(DEFAULT, TEXT_SPACING)).x + 2 * PADDING;
    if (GuiButton((Rectangle){current_x, button_y, theme_button_width, BUTTON_HEIGHT}, theme_text_str)) {
        TraceLog(LOG_INFO,"Кнопка 'Сменить тему' нажата (логика не реализована).");
    }
    current_x += theme_button_width + button_padding;
    const char *help_text_str = GuiIconText(ICON_STEP_INTO, NULL); 
    float help_button_width = BUTTON_HEIGHT; 
    if (GuiButton((Rectangle){bounds.x + bounds.width - PADDING - help_button_width, button_y, help_button_width, BUTTON_HEIGHT}, help_text_str)){
        TraceLog(LOG_INFO,"Кнопка 'Помощь' нажата (логика не реализована).");
    }
}

static void DrawMainView(AppData_Raylib *data) {
    if (!data) return;
    Rectangle top_nav_bar_bounds = {0, 0, (float)data->screen_width, TOP_NAV_BAR_HEIGHT};
    DrawTopNavigationBar(data, top_nav_bar_bounds);
    float panel_y_start = top_nav_bar_bounds.y + top_nav_bar_bounds.height + PADDING;
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
    GuiDrawRectangle(main_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.1f) );
    if (data->db_opened && data->table_data_main.num_allocated_column_names > 0 && data->table_data_main.column_names) {
        Rectangle table_actual_bounds = { main_table_container_bounds.x + 1, main_table_container_bounds.y + 1, main_table_container_bounds.width - 2, main_table_container_bounds.height - 2 };
        DrawDataTable_RL(data, &data->table_data_main, NULL, table_actual_bounds, &data->main_table_scroll, &data->main_table_selected_row, false);
    } else if (data->db_opened) {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        const char* msg = "Основная таблица не загружена или пуста (или нет имен колонок).";
        DrawTextEx(data->app_font, msg, (Vector2){text_bounds.x + (text_bounds.width - MeasureTextEx(data->app_font, msg, data->app_font.baseSize, 1).x)/2, text_bounds.y}, data->app_font.baseSize, 1, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
    } else {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        const char* msg = "Откройте файл базы данных.";
        DrawTextEx(data->app_font, msg, (Vector2){text_bounds.x + (text_bounds.width - MeasureTextEx(data->app_font, msg, data->app_font.baseSize, 1).x)/2, text_bounds.y}, data->app_font.baseSize, 1, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
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
        GuiDrawRectangle(values_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.1f));
        if (data->db_opened && data->table_data_values.num_allocated_column_names > 0 && data->table_data_values.column_names) {
             Rectangle values_table_actual_bounds = { values_table_container_bounds.x + 1, values_table_container_bounds.y + 1, values_table_container_bounds.width - 2, values_table_container_bounds.height - 2 };
            DrawDataTable_RL(data, &data->table_data_values, NULL, values_table_actual_bounds, &data->values_table_scroll, &data->values_table_selected_row, true);
        } else if (data->db_opened) {
            Rectangle text_bounds_val = {values_table_container_bounds.x + PADDING, values_table_container_bounds.y + PADDING, values_table_container_bounds.width - 2*PADDING, 30};
            const char *msg_val = "Таблица 'Значения' не загружена или пуста (или нет имен колонок).";
            DrawTextEx(data->app_font, msg_val, (Vector2){text_bounds_val.x + (text_bounds_val.width - MeasureTextEx(data->app_font, msg_val, data->app_font.baseSize, 1).x)/2, text_bounds_val.y}, data->app_font.baseSize, 1, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
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
            GuiDrawRectangle(schema_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.02f));
            Font font_sch_title = data->app_font_loaded ? data->app_font : GetFontDefault();
            float size_sch_title = (data->app_font_loaded ? data->app_font.baseSize : 20) * 0.9f;
            if(size_sch_title < 10) size_sch_title = 10;
            //DrawTextEx(font_sch_title, "Схема:", (Vector2){schema_container_bounds.x + PADDING, schema_container_bounds.y + PADDING/2.0f}, size_sch_title, 1, BLACK);
            if (data->schema_texture_loaded && data->schema_texture.id != 0) {
                // ИЗМЕНЕНИЕ: Корректируем Y и высоту области изображения, т.к. заголовок "Схема:" удален
                Rectangle img_area = {
                    schema_container_bounds.x + PADDING, 
                    schema_container_bounds.y + PADDING, // Начинаем изображение с отступом от верха контейнера схемы
                    schema_container_bounds.width - 2 * PADDING, 
                    schema_container_bounds.height - 2 * PADDING // Высота изображения - это высота контейнера минус отступы
                };
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
                    DrawTextEx(data->app_font, msg_sch, (Vector2){text_bounds_sch.x + (text_bounds_sch.width - MeasureTextEx(data->app_font, msg_sch, data->app_font.baseSize, 1).x)/2, text_bounds_sch.y}, data->app_font.baseSize, 1, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
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
            } else { data->show_main_edit_dialog = false; strcpy(data->status_message, "Ошибка: нет данных для редактирования в выбранной строке."); data->status_message_timer = 3.0f; }
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
                    if (insert_row_into_table_rl(data->db_handle, data->current_main_table_name, 
                                         new_epoch, new_coordinates, 
                                         data->table_data_main.column_count, 
                                         data->table_data_main.column_names)) {
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
                        } else { sprintf(data->status_message, "Ошибка перезагрузки таблицы '%s' после добавления.", data->current_main_table_name); data->status_message_timer = 3.0f; TraceLog(LOG_WARNING, "Ошибка перезагрузки таблицы '%s' после добавления новой строки.", data->current_main_table_name); }
                    } else { sprintf(data->status_message, "Ошибка добавления новой строки в БД для таблицы '%s'.", data->current_main_table_name); data->status_message_timer = 3.0f; TraceLog(LOG_WARNING, "insert_row_into_table_rl не удалась для таблицы '%s'.", data->current_main_table_name); }
                } else { sprintf(data->status_message, "Ошибка: нет имен колонок для вставки в '%s'.", data->current_main_table_name); data->status_message_timer = 3.0f; TraceLog(LOG_WARNING, "Недостаточно имен колонок (num_allocated_column_names: %d, column_count: %d) в table_data_main для insert_row_into_table_rl.", data->table_data_main.num_allocated_column_names, data->table_data_main.column_count); }
                free(new_coordinates); 
            } else { sprintf(data->status_message, "Не удалось рассчитать данные для новой строки."); data->status_message_timer = 3.0f; TraceLog(LOG_WARNING, "calculate_new_row_rl вернула NULL."); }
        }
    }
    GuiSetState(STATE_NORMAL);
} 

// static void DrawLevel1View(AppData_Raylib *data) {
//     if (!data) return;
//     if (GuiButton((Rectangle){ PADDING, PADDING + TOP_NAV_BAR_HEIGHT, 100, BUTTON_HEIGHT }, "< Назад")) { 
//         data->current_view = APP_VIEW_MAIN;
//         TraceLog(LOG_DEBUG,"Возврат в основной вид из Уровня 1.");
//         return;
//     }
//     Font font_to_draw = data->app_font_loaded ? data->app_font : GetFontDefault();
//     float font_size_to_draw = data->app_font_loaded ? data->app_font.baseSize : GetFontDefault().baseSize;
//      if (font_size_to_draw < 10) font_size_to_draw = 10;
//     DrawTextEx(font_to_draw, TextFormat("DrawTextEx: Уровень 1 - Вкладка: %d", data->l1_active_tab), 
//                (Vector2){ PADDING, PADDING + TOP_NAV_BAR_HEIGHT + BUTTON_HEIGHT + PADDING }, 
//                font_size_to_draw, 1, DARKGRAY);
// }

static void DrawEditMainTableRowDialog(AppData_Raylib *data) {
    if (!data || !data->show_main_edit_dialog || data->main_edit_dialog_row_index < 0 || 
        !data->table_data_main.epochs || !data->table_data_main.column_names || // Added column_names check
        data->main_edit_dialog_row_index >= data->table_data_main.row_count) {
        if (data) data->show_main_edit_dialog = false; 
        return;
    }

    int dialog_width = 450; 
    int max_dialog_height = data->screen_height - 100;
    int min_dialog_height = 150;

    int default_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    // ИЗМЕНЕНИЕ: Используем DEFAULT для TEXT_PADDING, так как GuiGetStyle(WINDOWBOX, ...) вызывал ошибку
    int text_padding_for_title = GuiGetStyle(DEFAULT, TEXT_PADDING); 
    int title_bar_height = default_text_size + text_padding_for_title * 2; 
    if (title_bar_height < 24) title_bar_height = 24;

    int fields_area_padding_top = PADDING;
    int fields_area_padding_bottom = PADDING;
    int buttons_area_height = BUTTON_HEIGHT + PADDING;
    
    int num_fields = data->table_data_main.column_count; 
    int required_fields_height = num_fields * (BUTTON_HEIGHT + PADDING);
    if (required_fields_height < 0) required_fields_height = 0;

    int calculated_dialog_height = title_bar_height + fields_area_padding_top + 
                                   required_fields_height + 
                                   fields_area_padding_bottom + buttons_area_height;

    int dialog_height = calculated_dialog_height;
    if (dialog_height > max_dialog_height) dialog_height = max_dialog_height;
    if (dialog_height < min_dialog_height && min_dialog_height <= max_dialog_height) dialog_height = min_dialog_height;

    Rectangle dialog_rect = {
        (float)(data->screen_width - dialog_width) / 2.0f,
        (float)(data->screen_height - dialog_height) / 2.0f,
        (float)dialog_width,
        (float)dialog_height
    };

    DrawRectangle(0, 0, data->screen_width, data->screen_height, Fade(BLACK, 0.65f));
    bool close_dialog_request = false;
    int current_epoch_val = data->table_data_main.epochs[data->main_edit_dialog_row_index];
    
    GuiWindowBox(dialog_rect, TextFormat("Редактировать строку (Эпоха: %d)", current_epoch_val));
    
    Rectangle fields_panel_bounds = {
        dialog_rect.x + PADDING, dialog_rect.y + title_bar_height + fields_area_padding_top,
        dialog_rect.width - 2 * PADDING,
        dialog_rect.height - title_bar_height - fields_area_padding_top - fields_area_padding_bottom - buttons_area_height
    };
    if (fields_panel_bounds.height < 0) fields_panel_bounds.height = 0;

    Rectangle fields_content_rect = { 
        0, 0, fields_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), (float)required_fields_height 
    };
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
            // Проверка, что (c + 1) не выходит за пределы num_allocated_column_names
            const char *col_name = ((c + 1) < data->table_data_main.num_allocated_column_names && data->table_data_main.column_names[c + 1]) ?
                                   data->table_data_main.column_names[c + 1] : TextFormat("X%d", c + 1);
            float screen_y_field = fields_view_area.y + current_y_fields_content + data->main_edit_dialog_scroll.y;
            GuiLabel((Rectangle){fields_view_area.x + PADDING + data->main_edit_dialog_scroll.x, screen_y_field, label_width, BUTTON_HEIGHT}, col_name);
            if (GuiTextBox((Rectangle){fields_view_area.x + PADDING + textbox_x_fields_content_rel + data->main_edit_dialog_scroll.x, screen_y_field, textbox_width_fields, BUTTON_HEIGHT}, 
                           data->main_edit_dialog_field_buffers[c], TEXT_FIELD_BUFFER_SIZE, 
                           data->main_edit_dialog_field_edit_mode[c])) {
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
                sprintf(data->status_message, "Ошибка: неверное число в поле '%s'", err_col_name);
                data->status_message_timer = 3.0f;
                TraceLog(LOG_WARNING, "Ошибка конвертации для поля %s: '%s'", err_col_name, data->main_edit_dialog_field_buffers[c]);
                break;
            }
        }
        if (all_valid) {
            bool db_updated_all = true;
            for (int c = 0; c < num_fields; c++) { 
                if (c >= MAX_DATA_COLUMNS_MAIN) break;
                const char *db_col_name = ((c+1) < data->table_data_main.num_allocated_column_names) ? data->table_data_main.column_names[c + 1] : NULL;
                const char *id_col_name = (data->table_data_main.num_allocated_column_names > 0) ? data->table_data_main.column_names[0] : NULL; // epochs column name
                int row_id_value = data->table_data_main.epochs[data->main_edit_dialog_row_index];
                if (!db_col_name || !id_col_name) {
                     TraceLog(LOG_ERROR, "Ошибка: Недостаточно данных для обновления ячейки (db_col_name или id_col_name не определены).");
                     sprintf(data->status_message, "Критическая ошибка: имена колонок для обновления не найдены."); data->status_message_timer = 3.0f; db_updated_all = false; break;
                }
                if (!update_table_cell_rl(data->db_handle, data->current_main_table_name, db_col_name, id_col_name, new_values[c], row_id_value)) {
                    sprintf(data->status_message, "Ошибка обновления БД для поля '%s'.", db_col_name); data->status_message_timer = 3.0f;
                    TraceLog(LOG_WARNING, "update_table_cell_rl не удалось для %s.%s (ID %d)", data->current_main_table_name, db_col_name, row_id_value);
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
                TraceLog(LOG_INFO, "Строка %d (Эпоха: %d) успешно обновлена в таблице '%s'.", data->main_edit_dialog_row_index, current_epoch_val, data->current_main_table_name);
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
    // ИЗМЕНЕНИЕ: Используем DEFAULT для TEXT_PADDING
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

    Rectangle dialog_rect = {(float)(data->screen_width - dialog_width) / 2.0f, (float)(data->screen_height - dialog_height) / 2.0f, (float)dialog_width, (float)dialog_height};

    DrawRectangle(0, 0, data->screen_width, data->screen_height, Fade(BLACK, 0.65f));
    bool close_dialog_req = false;
    int current_row_id_val = current_table->epochs[current_row_idx];
    GuiWindowBox(dialog_rect, TextFormat("Редактировать '%s' (Строка ID: %d)", data->values_table_name, current_row_id_val));

    Rectangle fields_panel_bounds = {dialog_rect.x + PADDING, dialog_rect.y + title_bar_h + fields_pad_top, dialog_rect.width - 2 * PADDING, dialog_rect.height - title_bar_h - fields_pad_top - fields_pad_bottom - buttons_area_h};
    if (fields_panel_bounds.height < 0) fields_panel_bounds.height = 0;

    Rectangle fields_content_rect = {0, 0, fields_panel_bounds.width - GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH), (float)required_fields_h };
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
            if (GuiTextBox((Rectangle){fields_view_area.x + PADDING + textbox_x_fields_content_rel + data->values_edit_dialog_scroll.x, screen_y_field, textbox_w_fields, BUTTON_HEIGHT}, 
                           data->values_edit_dialog_field_buffers[field_buffer_idx], TEXT_FIELD_BUFFER_SIZE, 
                           data->values_edit_dialog_field_edit_mode[field_buffer_idx])) {
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
                all_valid = false; sprintf(data->status_message, "Ошибка: неверное число в поле '%s'", col_name_for_val);
                data->status_message_timer = 3.0f; 
                TraceLog(LOG_WARNING, "Ошибка конвертации для '%s' (Значения): '%s'", col_name_for_val, data->values_edit_dialog_field_buffers[current_field_buffer_idx_val]);
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
                if (!update_table_cell_rl(data->db_handle, data->values_table_name, db_col_name_val, NULL, 
                                          new_values_arr[current_field_buffer_idx_val], row_id_to_update)) {
                    db_updated_all_val = false; sprintf(data->status_message, "Ошибка БД для '%s'", db_col_name_val); data->status_message_timer = 3.0f; 
                    TraceLog(LOG_WARNING, "update_table_cell_rl не удалось для %s.%s (ROWID %d)", data->values_table_name, db_col_name_val, row_id_to_update);
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
    if (data->status_message_timer > 0) {
        data->status_message_timer -= GetFrameTime();
        if (data->status_message_timer <= 0) data->status_message[0] = '\0';
    }
    bool dialogue_active = data->show_main_edit_dialog || data->show_values_edit_dialog;
    if (dialogue_active) { GuiLock(); }
    BeginDrawing();
    ClearBackground(GetColor(0xf1f2f2FF)); 
    switch (data->current_view) {
        case APP_VIEW_MAIN: DrawMainView(data); break;
        case APP_VIEW_LEVEL1: DrawLevel1View_Entry(data); break;
        default: {
            Font font_err = data->app_font_loaded ? data->app_font : GetFontDefault();
            float size_err = data->app_font_loaded ? data->app_font.baseSize : 20;
            if (size_err < 10) size_err = 10;
            DrawTextEx(font_err, "Неизвестное состояние!", (Vector2){20, 20 + TOP_NAV_BAR_HEIGHT}, size_err, 1, RED);
        } break;
    }
    if (dialogue_active) { GuiUnlock(); }
    if (data->show_main_edit_dialog) {
        DrawEditMainTableRowDialog(data);
    } else if (data->show_values_edit_dialog) {
        DrawEditValuesDialog(data);
    }
    if (data->status_message_timer > 0 && data->status_message[0] != '\0') {
        GuiStatusBar((Rectangle){0, (float)data->screen_height - 20, (float)data->screen_width, 20}, data->status_message);
    }
    DrawFPS(data->screen_width - 90, 10); 
    EndDrawing();
}