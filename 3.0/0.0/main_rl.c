#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#include "app_data_rl.h"
#include "sqlite_utils_rl.h"
#include "calculations_rl.h"
// #include "level1_analysis_rl.h" // Когда появится

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tinyfiledialogs.h"
#include <ctype.h>

// --- Прототипы функций ---
static void InitApp(AppData_Raylib *data);
static void UpdateDrawAppFrame(AppData_Raylib *data); // Используем эту версию
static void ShutdownApp(AppData_Raylib *data);

// Функции отрисовки для каждого вида
static void DrawMainView(AppData_Raylib *data);
static void DrawLevel1View(AppData_Raylib *data);


// Глобальные константы для UI
#define BUTTON_HEIGHT 30
#define PADDING 10

// Константы для отрисовки таблицы
#define TABLE_HEADER_HEIGHT 25.0f
#define TABLE_ROW_HEIGHT 22.0f
#define TABLE_EPOCH_COL_WIDTH 70.0f
#define TABLE_DATA_COL_WIDTH 90.0f // Ширина для колонок с данными
#define TABLE_CELL_TEXT_PADDING_X 5.0f
#define HIDE_ROWID_COLUMN_FOR_VALUES_TABLE 1
#define HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE 1 // 1 - скрыть, 0 - показать

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

// --- InitApp (с загрузкой шрифта и инициализацией RayGUI) ---
static void InitApp(AppData_Raylib *data) {
    if (!data) return;
    memset(data, 0, sizeof(AppData_Raylib));
    data->screen_width = 1280;
    data->screen_height = 720;
    data->current_view = APP_VIEW_MAIN;
    data->l1_active_tab = L1_TAB_INITIAL_DATA;

    strcpy(data->db_file_path_text_box, "Вариант28.sqlite");
    data->db_file_path_edit_mode = false;
    strcpy(data->values_table_name, "Значения");
    strcpy(data->schema_blob_column_name, "Схема");
    data->main_h_split_pos = (float)data->screen_width * 0.7f;
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

    InitWindow(data->screen_width, data->screen_height, "Анализ данных временных рядов (Raylib)");
    SetTargetFPS(60);
    GuiLoadStyleDefault(); 

    #define MAX_CODEPOINTS_INITAPP_FINAL 512 
    int codepoints[MAX_CODEPOINTS_INITAPP_FINAL] = { 0 };
    int currentGlyphCount = 0;
    for (int i = 0; i < 95; i++) { if (currentGlyphCount < MAX_CODEPOINTS_INITAPP_FINAL) codepoints[currentGlyphCount++] = 32 + i; else break; }
    for (int i = 0; i < 256; i++) { if (currentGlyphCount < MAX_CODEPOINTS_INITAPP_FINAL) codepoints[currentGlyphCount++] = 0x0400 + i; else break; }
    
    TraceLog(LOG_INFO, "InitApp: Загрузка шрифта с %d кодовыми точками.", currentGlyphCount);
    data->app_font = LoadFontEx("C:/Windows/Fonts/arial.ttf", 20, codepoints, currentGlyphCount);

    if (data->app_font.texture.id == 0) {
        TraceLog(LOG_WARNING, "InitApp: ШРИФТ: Не удалось загрузить пользовательский шрифт! Будет использован стандартный.");
        data->app_font_loaded = false;
        data->app_font = GetFontDefault(); 
        GuiSetFont(data->app_font);      
    } else {
        TraceLog(LOG_INFO, "InitApp: ШРИФТ: Загружен успешно (%d глифов)!", data->app_font.glyphCount);
        data->app_font_loaded = true;
        GuiSetFont(data->app_font); 
    }
    srand(time(NULL));
    TraceLog(LOG_INFO, "InitApp: Приложение инициализировано.");
}

// --- ShutdownApp (полная версия с очисткой) ---
static void ShutdownApp(AppData_Raylib *data) {
    if (!data) return;
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Завершение работы приложения...");

    if (data->db_opened && data->db_handle) {
        sqlite3_close(data->db_handle);
        data->db_handle = NULL;
        data->db_opened = false;
    }
    free_table_data_content_rl(&data->table_data_main);
    free_table_data_content_rl(&data->table_data_values);

    if (data->schema_texture_loaded) {
        UnloadTexture(data->schema_texture);
        data->schema_texture_loaded = false;
    }
    
#define SAFE_FREE_ARRAY(arr_ptr) do { if(data->arr_ptr) { free(data->arr_ptr); data->arr_ptr = NULL; } } while(0)
#define SAFE_FREE_TABLE(table_ptr_member, row_count_member) \
    do { \
        if(data->table_ptr_member) { \
            for(int i_clean = 0; i_clean < data->row_count_member; i_clean++) { \
                if(data->table_ptr_member[i_clean]) free(data->table_ptr_member[i_clean]); \
            } \
            free(data->table_ptr_member); \
            data->table_ptr_member = NULL; \
            data->row_count_member = 0; \
        } \
    } while(0)

    SAFE_FREE_ARRAY(mu_data); data->mu_data_count = 0; // Используем имя поля из структуры data
    SAFE_FREE_ARRAY(alpha_data); data->alpha_count = 0;
    SAFE_FREE_ARRAY(mu_prime_data); data->mu_prime_data_count = 0;
    SAFE_FREE_ARRAY(alpha_prime_data); data->alpha_prime_data_count = 0;
    SAFE_FREE_ARRAY(mu_double_prime_data); data->mu_double_prime_data_count = 0;
    SAFE_FREE_ARRAY(alpha_double_prime_data); data->alpha_double_prime_data_count = 0;

    SAFE_FREE_TABLE(error_adjusted_table_data, error_adjusted_rows_alloc);
    SAFE_FREE_TABLE(error_subtracted_table_data, error_subtracted_rows_alloc);
    
    SAFE_FREE_TABLE(smoothing_data, smoothing_row_count);
    SAFE_FREE_TABLE(smoothing_alpha_data, smoothing_alpha_row_count);
    SAFE_FREE_TABLE(smoothing_mu_prime_data, smoothing_mu_prime_row_count);
    SAFE_FREE_TABLE(smoothing_alpha_prime_data, smoothing_alpha_prime_row_count);
    SAFE_FREE_TABLE(smoothing_mu_double_prime_data, smoothing_mu_double_prime_row_count);
    SAFE_FREE_TABLE(smoothing_alpha_double_prime_data, smoothing_alpha_double_prime_row_count);

#undef SAFE_FREE_ARRAY
#undef SAFE_FREE_TABLE

    if (data->app_font_loaded) {
        UnloadFont(data->app_font); 
        data->app_font_loaded = false;
    }
    CloseWindow();
    TraceLog(LOG_INFO, "APP_SHUTDOWN: Приложение завершено.");
}

static void DrawDataTable_RL(AppData_Raylib *data, TableData *tableData, const char* title, 
                             Rectangle panel_bounds, Vector2 *scroll, int *selectedRow, bool isValuesTableType) {
    if (!data) return;

    Font current_font = data->app_font_loaded ? data->app_font : GetFontDefault();
    float base_font_size = data->app_font_loaded ? data->app_font.baseSize : 20;
    if (base_font_size < 10) base_font_size = 10;

    if (!tableData) {
        DrawTextEx(current_font, "TableData is NULL", (Vector2){panel_bounds.x + PADDING, panel_bounds.y + PADDING}, base_font_size, 1, RED);
        return;
    }

    int data_cols_to_draw = tableData->column_count;
    int first_col_width = TABLE_EPOCH_COL_WIDTH;
    bool hide_first_col = (isValuesTableType && HIDE_ROWID_COLUMN_FOR_VALUES_TABLE); 

    if (title && title[0] != '\0') {
         DrawTextEx(current_font, title, (Vector2){panel_bounds.x, panel_bounds.y - TABLE_HEADER_HEIGHT - 2}, base_font_size, 1, BLACK);
    }

    Rectangle contentRect = {0};
    contentRect.width = (hide_first_col ? 0 : first_col_width) + (float)data_cols_to_draw * TABLE_DATA_COL_WIDTH;
    contentRect.height = TABLE_HEADER_HEIGHT + (float)tableData->row_count * TABLE_ROW_HEIGHT;
    
    Rectangle viewArea = {0}; 
    GuiScrollPanel(panel_bounds, NULL, contentRect, scroll, &viewArea);

    BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

    float content_rel_x = 0;
    float content_rel_header_y = 0; 

    // --- Отрисовка Заголовков ---
    if (!hide_first_col) {
        const char *first_col_header = isValuesTableType ? "ROWID" : ((tableData->column_names && tableData->column_names[0]) ? tableData->column_names[0] : "Эпоха");
        Rectangle headerFirstScr = { viewArea.x + content_rel_x + scroll->x, viewArea.y + content_rel_header_y + scroll->y, first_col_width, TABLE_HEADER_HEIGHT };
        GuiDrawRectangle(headerFirstScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
        GuiDrawText(first_col_header, GetTextBounds(TEXTBOX, headerFirstScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL)));
        content_rel_x += first_col_width;
    }

    for (int c = 0; c < data_cols_to_draw; c++) {
        const char *data_col_name = isValuesTableType ? 
                                    ((tableData->column_names && c < tableData->column_count) ? tableData->column_names[c] : TextFormat("Val%d", c)) :
                                    ((tableData->column_names && (c + 1) <= tableData->column_count) ? tableData->column_names[c + 1] : TextFormat("X%d", c + 1));
        if (isValuesTableType && strcmp(data_col_name, data->schema_blob_column_name) == 0 && HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) { // HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE - новый макрос
            continue; 
        }
        Rectangle headerDataScr = { viewArea.x + content_rel_x + scroll->x, viewArea.y + content_rel_header_y + scroll->y, TABLE_DATA_COL_WIDTH, TABLE_HEADER_HEIGHT };
        GuiDrawRectangle(headerDataScr, GuiGetStyle(DEFAULT, BORDER_WIDTH), GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)), GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
        GuiDrawText(data_col_name, GetTextBounds(TEXTBOX, headerDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL)));
        content_rel_x += TABLE_DATA_COL_WIDTH;
    }

    // --- Отрисовка Строк Данных ---
    if (tableData->row_count == 0) { 
         DrawTextEx(current_font, "Нет данных", 
                   (Vector2){viewArea.x + PADDING + scroll->x, viewArea.y + TABLE_HEADER_HEIGHT + PADDING + scroll->y}, 
                   base_font_size * 0.9f, 1, GRAY);
    }

    for (int r = 0; r < tableData->row_count; r++) {
        content_rel_x = 0; 
        float content_rel_current_row_y = TABLE_HEADER_HEIGHT + (float)r * TABLE_ROW_HEIGHT;
        float screen_y_current_row = viewArea.y + content_rel_current_row_y + scroll->y;


        Color row_bg_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, BASE_COLOR_FOCUSED) : GuiGetStyle(LISTVIEW, BASE_COLOR_NORMAL));
        if (r % 2 != 0 && *selectedRow != r) row_bg_color = ColorAlphaBlend(row_bg_color, GetColor(GuiGetStyle(LISTVIEW, BASE_COLOR_DISABLED)), Fade(WHITE, 0.1f));
        Color row_text_color = GetColor((*selectedRow == r) ? GuiGetStyle(LISTVIEW, TEXT_COLOR_FOCUSED) : GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));

        if (!hide_first_col) {
            Rectangle cellFirstScr = { viewArea.x + content_rel_x + scroll->x, screen_y_current_row, first_col_width, TABLE_ROW_HEIGHT };
            GuiDrawRectangle(cellFirstScr, 0, BLANK, row_bg_color);
            if (tableData->epochs) GuiDrawText(TextFormat("%d", tableData->epochs[r]), GetTextBounds(TEXTBOX, cellFirstScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
            content_rel_x += first_col_width;
        }

        if (tableData->table && tableData->table[r]) {
            for (int c = 0; c < data_cols_to_draw; c++) {
                const char *header_name = "";
                if (isValuesTableType && tableData->column_names && c < tableData->column_count) {
                     header_name = tableData->column_names[c];
                }
                // Если это таблица "Значения" и колонка "Схема", пропускаем ее отрисовку
                if (isValuesTableType && strcmp(header_name, data->schema_blob_column_name) == 0 && HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE) {
                    continue; 
                }

                Rectangle cellDataScr = { viewArea.x + content_rel_x + scroll->x, screen_y_current_row, TABLE_DATA_COL_WIDTH, TABLE_ROW_HEIGHT };
                GuiDrawRectangle(cellDataScr, 0, BLANK, row_bg_color);
                
                if (isValuesTableType && strcmp(header_name, data->schema_blob_column_name) == 0) { // Эта проверка теперь избыточна из-за continue выше, но оставим для ясности
                    GuiDrawText("BLOB", GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                } else {
                    GuiDrawText(TextFormat("%.4f", tableData->table[r][c]), GetTextBounds(TEXTBOX, cellDataScr), GuiGetStyle(DEFAULT, TEXT_ALIGNMENT), row_text_color);
                }
                content_rel_x += TABLE_DATA_COL_WIDTH;
            }
        }
        
        Rectangle row_rect_on_screen = {
            viewArea.x + 0 + scroll->x, // Относительный X начала строки = 0
            screen_y_current_row,       // Y строки уже экранный
            contentRect.width,          // Общая ширина строки контента
            TABLE_ROW_HEIGHT
        };

        // --- ИЗМЕНЕНИЕ ЗДЕСЬ ---
        if (!data->show_main_edit_dialog /* && !другие_флаги_диалогов */ ) {
            if (CheckCollisionPointRec(GetMousePosition(), viewArea)) { // Клик должен быть внутри видимой части панели
                if (CheckCollisionPointRec(GetMousePosition(), row_rect_on_screen) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                     *selectedRow = r;
                     TraceLog(LOG_INFO, "Выбрана строка: %d в таблице '%s'", r, title ? title : "N/A");
                }
            }
        }
    }
    EndScissorMode();
}

// --- DrawMainView (с элементами RayGUI) ---
static void DrawMainView(AppData_Raylib *data) {
    if (!data) return;

    Rectangle top_bar_rect = { PADDING, PADDING, (float)data->screen_width - 2 * PADDING, BUTTON_HEIGHT };

    // Кнопка "Открыть SQLite файл"
    if (GuiButton((Rectangle){ top_bar_rect.x, top_bar_rect.y, 180, BUTTON_HEIGHT }, "Открыть SQLite файл")) {
        TraceLog(LOG_INFO,"Кнопка 'Открыть SQLite файл' нажата.");

        const char * aFilterPatterns[3] = { "*.sqlite", "*.db", "*.sqlite3" };
        char const *selectedFilePath = tinyfd_openFileDialog(
            "Выберите SQLite базу данных",
            data->current_db_path[0] != '\0' ? data->current_db_path : "", 
            3, aFilterPatterns, "SQLite файлы (*.sqlite, *.db, *.sqlite3)", 0);

        if (selectedFilePath == NULL) {
            TraceLog(LOG_INFO, "Диалог выбора файла: Файл не выбран.");
        } else {
            TraceLog(LOG_INFO, "Выбран файл: %s", selectedFilePath);
            strncpy(data->db_file_path_text_box, selectedFilePath, sizeof(data->db_file_path_text_box) - 1);
            data->db_file_path_text_box[sizeof(data->db_file_path_text_box) - 1] = '\0';

            if (data->db_opened && data->db_handle) {
                sqlite3_close(data->db_handle);
                data->db_handle = NULL;
                data->db_opened = false;
            }
            free_table_data_content_rl(&data->table_data_main);
            free_table_data_content_rl(&data->table_data_values);
            if (data->schema_texture_loaded) {
                UnloadTexture(data->schema_texture);
                data->schema_texture_loaded = false;
                memset(&data->schema_texture, 0, sizeof(Texture2D));
            }
            data->current_main_table_name[0] = '\0';
            data->main_table_selected_row = -1; 
            data->values_table_selected_row = -1;
            data->main_table_scroll = (Vector2){0,0}; 
            data->values_table_scroll = (Vector2){0,0}; 

            data->db_handle = open_database_rl(data->db_file_path_text_box);

            if (data->db_handle) {
                data->db_opened = true;
                strncpy(data->current_db_path, data->db_file_path_text_box, sizeof(data->current_db_path) - 1);
                data->current_db_path[sizeof(data->current_db_path) - 1] = '\0';
                
                sprintf(data->status_message, "БД '%s' открыта.", data->current_db_path);
                data->status_message_timer = 3.0f;
                TraceLog(LOG_INFO, "БД '%s' успешно открыта.", data->current_db_path);

                if (load_table_rl(data->db_handle, &data->table_data_values, data->values_table_name)) {
                    TraceLog(LOG_INFO, "Таблица '%s' загружена: %d строк, %d колонок.",
                               data->values_table_name, data->table_data_values.row_count, data->table_data_values.column_count);
                    if (data->table_data_values.row_count > 0) {
                        BlobData_RL blob = load_blob_from_table_by_rowid_rl(data->db_handle, data->values_table_name, data->schema_blob_column_name, 1);
                        if (blob.loaded && blob.data && blob.size > 0) {
                            Image img = LoadImageFromMemory(".png", blob.data, blob.size);
                            if (img.data) {
                                data->schema_texture = LoadTextureFromImage(img);
                                UnloadImage(img);
                                if (data->schema_texture.id != 0) data->schema_texture_loaded = true;
                                else TraceLog(LOG_WARNING, "Не удалось создать текстуру из BLOB схемы.");
                            } else TraceLog(LOG_WARNING, "Не удалось загрузить Image из BLOB схемы (возможно, не PNG или поврежден).");
                            free_blob_data_rl(&blob);
                        } else TraceLog(LOG_INFO, "BLOB схемы не найден или пуст в '%s' для ROWID=1.", data->values_table_name);
                    }
                } else TraceLog(LOG_WARNING, "Не удалось загрузить таблицу '%s'.", data->values_table_name);

                TableList_RL table_list = get_table_list_rl(data->db_handle);
                char *main_analysis_table_to_load = NULL;
                if (table_list.count > 0) {
                    for (int i = 0; i < table_list.count; i++) {
                        if (strcmp(table_list.names[i], data->values_table_name) != 0) {
                            main_analysis_table_to_load = table_list.names[i]; break; }
                    }
                    if (!main_analysis_table_to_load && table_list.count > 0) {
                        if (strcmp(table_list.names[0], data->values_table_name) != 0) main_analysis_table_to_load = table_list.names[0];
                        else if (table_list.count > 1) main_analysis_table_to_load = table_list.names[1];
                    }
                    if (main_analysis_table_to_load) {
                        strncpy(data->current_main_table_name, main_analysis_table_to_load, sizeof(data->current_main_table_name) - 1);
                        data->current_main_table_name[sizeof(data->current_main_table_name) - 1] = '\0';
                        if (load_table_rl(data->db_handle, &data->table_data_main, data->current_main_table_name)) {
                            sprintf(data->status_message, "Загружена: %s (%d строк)", data->current_main_table_name, data->table_data_main.row_count);
                            data->status_message_timer = 3.0f;
                        } else {
                            sprintf(data->status_message, "Ошибка загрузки: %s", data->current_main_table_name);
                            data->status_message_timer = 3.0f; data->current_main_table_name[0] = '\0';
                        }
                    } else TraceLog(LOG_WARNING, "Не найдено подходящей основной таблицы для анализа.");
                    free_table_list_rl(&table_list);
                } else {
                    strcpy(data->status_message, "В БД нет таблиц!"); data->status_message_timer = 3.0f;
                }
            } else {
                data->db_opened = false;
                sprintf(data->status_message, "Ошибка открытия БД: %s", data->db_file_path_text_box);
                data->status_message_timer = 3.0f;
            }
        }
    }

    Rectangle level1_button_rect = { top_bar_rect.x + 180 + PADDING, top_bar_rect.y, 120, BUTTON_HEIGHT };
    if (GuiButton(level1_button_rect, "Уровень 1")) {
        if (data->db_opened && data->table_data_main.row_count > 0) {
            data->current_view = APP_VIEW_LEVEL1;
        } else {
            strcpy(data->status_message, "БД не открыта или основная таблица пуста!");
            data->status_message_timer = 3.0f;
        }
    }

    // --- Определение областей для панелей ---
    float panel_y_start = top_bar_rect.y + BUTTON_HEIGHT + PADDING;
    float panel_height = (float)data->screen_height - panel_y_start - PADDING - 20; 

    if (data->main_h_split_pos == 0) data->main_h_split_pos = data->screen_width * 0.65f;
    float left_panel_width = data->main_h_split_pos - PADDING - (PADDING / 2.0f);
    if (left_panel_width < 200) left_panel_width = 200;

    float right_panel_x_start = data->main_h_split_pos + (PADDING / 2.0f);
    float right_panel_width = data->screen_width - right_panel_x_start - PADDING;
    if (right_panel_width < 150) right_panel_width = 150;


    // --- Левая панель: Основная таблица ---
    Rectangle main_table_container_bounds = { PADDING, panel_y_start, left_panel_width, panel_height };
    GuiDrawRectangle(main_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.1f) );

    if (data->db_opened && data->table_data_main.column_count > 0) {
        Rectangle table_actual_bounds = {
            main_table_container_bounds.x + 1, main_table_container_bounds.y + 1,
            main_table_container_bounds.width - 2, main_table_container_bounds.height - 2
        };
        DrawDataTable_RL(data, &data->table_data_main, 
                         data->current_main_table_name[0] != '\0' ? data->current_main_table_name : "Основная таблица", 
                         table_actual_bounds, &data->main_table_scroll, &data->main_table_selected_row,
                         false); // isValuesTableType = false
    } else if (data->db_opened) {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        GuiDrawText("Основная таблица не загружена или пуста.", GetTextBounds(DEFAULT, text_bounds), TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
    } else {
        Rectangle text_bounds = {main_table_container_bounds.x + PADDING, main_table_container_bounds.y + PADDING, main_table_container_bounds.width - 2*PADDING, 30};
        GuiDrawText("Откройте файл базы данных.", GetTextBounds(DEFAULT, text_bounds), TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
    }

    // --- Правая панель: Таблица "Значения" и Схема ---
    if (data->show_right_panel_main_view && right_panel_width > PADDING * 2) { 
        Rectangle right_panel_bounds = { right_panel_x_start, panel_y_start, right_panel_width, panel_height };
        
        float values_table_height_ratio = 0.35f; 
        float values_table_height = panel_height * values_table_height_ratio;
        if (values_table_height < TABLE_HEADER_HEIGHT + TABLE_ROW_HEIGHT * 3) values_table_height = TABLE_HEADER_HEIGHT + TABLE_ROW_HEIGHT * 3; 
        if (values_table_height > panel_height - PADDING - (TABLE_HEADER_HEIGHT + 50) ) values_table_height = panel_height - PADDING - (TABLE_HEADER_HEIGHT + 50); 

        Rectangle values_table_container_bounds = {
            right_panel_bounds.x, 
            right_panel_bounds.y,
            right_panel_bounds.width,
            values_table_height
        };
        GuiDrawRectangle(values_table_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.1f));

        if (data->db_opened && data->table_data_values.column_count > 0) {
             Rectangle values_table_actual_bounds = {
                values_table_container_bounds.x + 1, values_table_container_bounds.y + 1,
                values_table_container_bounds.width - 2, values_table_container_bounds.height - 2
            };
            DrawDataTable_RL(data, &data->table_data_values, 
                             data->values_table_name, 
                             values_table_actual_bounds, &data->values_table_scroll, &data->values_table_selected_row,
                             true); // isValuesTableType = true
        } else if (data->db_opened) {
            Rectangle text_bounds_val = {values_table_container_bounds.x + PADDING, values_table_container_bounds.y + PADDING, values_table_container_bounds.width - 2*PADDING, 30};
            GuiDrawText("Таблица 'Значения' не загружена или пуста.", GetTextBounds(DEFAULT, text_bounds_val), TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
        }

        float schema_y_start = values_table_container_bounds.y + values_table_container_bounds.height + PADDING;
        Rectangle schema_container_bounds = {
            right_panel_bounds.x,
            schema_y_start,
            right_panel_bounds.width,
            right_panel_bounds.height - (schema_y_start - right_panel_bounds.y) 
        };
        if (schema_container_bounds.height > TABLE_HEADER_HEIGHT) { 
            GuiDrawRectangle(schema_container_bounds, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)), ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.02f));
            
            Font font_to_draw_schema_title = data->app_font_loaded ? data->app_font : GetFontDefault();
            float font_size_schema_title = (data->app_font_loaded ? data->app_font.baseSize : 20) * 0.9f;
            if(font_size_schema_title < 10) font_size_schema_title = 10;

            DrawTextEx(font_to_draw_schema_title, "Схема:", 
                       (Vector2){schema_container_bounds.x + PADDING, schema_container_bounds.y + PADDING/2.0f}, 
                       font_size_schema_title, 1, BLACK);

            if (data->schema_texture_loaded && data->schema_texture.id != 0) {
                Rectangle image_draw_area = {
                    schema_container_bounds.x + PADDING,
                    schema_container_bounds.y + PADDING + font_size_schema_title + PADDING/2.0f, 
                    schema_container_bounds.width - 2 * PADDING,
                    schema_container_bounds.height - (2 * PADDING + font_size_schema_title + PADDING/2.0f)
                };
                if (image_draw_area.width > 0 && image_draw_area.height > 0) {
                    float scale = 1.0f;
                    if (data->schema_texture.width > image_draw_area.width) {
                        scale = image_draw_area.width / (float)data->schema_texture.width;
                    }
                    if (data->schema_texture.height * scale > image_draw_area.height) {
                        scale = image_draw_area.height / (float)data->schema_texture.height;
                    }
                    
                    float dest_width = (float)data->schema_texture.width * scale;
                    float dest_height = (float)data->schema_texture.height * scale;
                    
                    Vector2 texture_pos = {
                        image_draw_area.x + (image_draw_area.width - dest_width) / 2.0f,
                        image_draw_area.y + (image_draw_area.height - dest_height) / 2.0f
                    };
                    DrawTextureEx(data->schema_texture, texture_pos, 0.0f, scale, WHITE);
                }
            } else if (data->db_opened) {
                Rectangle text_bounds_sch = {schema_container_bounds.x + PADDING, schema_container_bounds.y + PADDING + font_size_schema_title + PADDING, schema_container_bounds.width - 2*PADDING, 30};
                GuiDrawText("Схема не загружена.", GetTextBounds(DEFAULT, text_bounds_sch), TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
            }
        }
    }

    float control_button_y = main_table_container_bounds.y + main_table_container_bounds.height + PADDING;
    float control_button_x = main_table_container_bounds.x;

    // Кнопка "Редактировать строку" (для основной таблицы)
    // Активируем, если строка выбрана и таблица не пуста
    bool can_edit_main = (data->main_table_selected_row >= 0 && 
                          data->main_table_selected_row < data->table_data_main.row_count);
    
    GuiSetState(can_edit_main ? STATE_NORMAL : STATE_DISABLED);
    if (GuiButton((Rectangle){control_button_x, control_button_y, 180, BUTTON_HEIGHT}, "Редактировать строку")) {
        if (can_edit_main) {
            data->main_edit_dialog_row_index = data->main_table_selected_row;
            data->show_main_edit_dialog = true;
            TraceLog(LOG_INFO, "Открытие диалога редактирования для строки %d", data->main_edit_dialog_row_index);
            data->main_edit_dialog_scroll = (Vector2){0,0};

            // Заполняем буферы диалога текущими значениями из выбранной строки
            if (data->table_data_main.table && data->table_data_main.table[data->main_edit_dialog_row_index]) {
                for (int c = 0; c < data->table_data_main.column_count; c++) {
                    if (c < MAX_DATA_COLUMNS_MAIN) {
                        // Форматируем double в строку для TextBox
                        sprintf(data->main_edit_dialog_field_buffers[c], "%.4f", data->table_data_main.table[data->main_edit_dialog_row_index][c]);
                        data->main_edit_dialog_field_edit_mode[c] = false; // Сбрасываем режим редактирования
                    }
                }
            }
        }
    }
    GuiSetState(STATE_NORMAL);
    control_button_x += 180 + PADDING;

    // TODO: Кнопка "Добавить строку" будет здесь же
}

// --- DrawLevel1View (с кнопкой RayGUI и текстом DrawTextEx) ---
static void DrawLevel1View(AppData_Raylib *data) {
    if (GuiButton((Rectangle){ PADDING, PADDING, 100, BUTTON_HEIGHT }, "< Назад")) {
        data->current_view = APP_VIEW_MAIN;
        TraceLog(LOG_DEBUG,"Возврат в основной вид из Уровня 1.");
        return;
    }
    
    Font font_to_draw = data->app_font_loaded ? data->app_font : GetFontDefault();
    float font_size_to_draw = data->app_font_loaded ? data->app_font.baseSize : GetFontDefault().baseSize;
     if (font_size_to_draw < 10) font_size_to_draw = 10;

    DrawTextEx(font_to_draw, TextFormat("DrawTextEx: Уровень 1 - Вкладка: %d", data->l1_active_tab), 
               (Vector2){ PADDING, PADDING + BUTTON_HEIGHT + PADDING }, 
               font_size_to_draw, 1, DARKGRAY);
}

static void DrawEditMainTableRowDialog(AppData_Raylib *data) {
    if (!data || !data->show_main_edit_dialog || data->main_edit_dialog_row_index < 0 || 
        data->main_edit_dialog_row_index >= data->table_data_main.row_count) { // Добавил проверку на выход за пределы
        data->show_main_edit_dialog = false; // Закрываем, если индекс некорректен
        return;
    }

    // --- Размеры и позиция "окна" диалога ---
    int dialog_width = 450; // Немного шире для удобства
    int max_dialog_height = data->screen_height - 100;
    int min_dialog_height = 150;

    // Высота, необходимая для всех полей и кнопок
    int title_bar_height = 24; // Примерная высота заголовка GuiWindowBox
    int fields_area_padding_top = PADDING;
    int fields_area_padding_bottom = PADDING;
    int buttons_area_height = BUTTON_HEIGHT + PADDING;
    
    int num_fields = data->table_data_main.column_count;
    int required_fields_height = num_fields * (BUTTON_HEIGHT + PADDING);

    int calculated_dialog_height = title_bar_height + fields_area_padding_top + 
                                   required_fields_height + 
                                   fields_area_padding_bottom + buttons_area_height;

    int dialog_height = calculated_dialog_height;
    if (dialog_height > max_dialog_height) dialog_height = max_dialog_height;
    if (dialog_height < min_dialog_height) dialog_height = min_dialog_height;

    Rectangle dialog_rect = {
        (float)(data->screen_width - dialog_width) / 2.0f,
        (float)(data->screen_height - dialog_height) / 2.0f,
        (float)dialog_width,
        (float)dialog_height
    };

    // Затемняем фон
    DrawRectangle(0, 0, data->screen_width, data->screen_height, Fade(BLACK, 0.65f));

    bool close_dialog_request = false; // Флаг для закрытия диалога по кнопкам
    int window_button_pressed = GuiWindowBox(dialog_rect, TextFormat("Редактировать строку (Эпоха: %d)", data->table_data_main.epochs[data->main_edit_dialog_row_index]));
    
    if (window_button_pressed == 1) { // 1 - код для кнопки закрытия окна (крестик)
        close_dialog_request = true;
    }

    // --- Область для полей ввода с прокруткой ---
    Rectangle fields_panel_bounds = {
        dialog_rect.x + PADDING,
        dialog_rect.y + title_bar_height + fields_area_padding_top,
        dialog_rect.width - 2 * PADDING,
        dialog_rect.height - title_bar_height - fields_area_padding_top - fields_area_padding_bottom - buttons_area_height
    };

    // Общий размер содержимого для полей ввода
    Rectangle fields_content_rect = { 
        0, 0, 
        fields_panel_bounds.width - 20, // -20 для возможного вертикального скроллбара
        (float)required_fields_height 
    };
    
    Rectangle fields_view_area = {0};
    // Убедись, что data->main_edit_dialog_scroll инициализирован в AppData и сбрасывается при открытии диалога
    GuiScrollPanel(fields_panel_bounds, NULL, fields_content_rect, &data->main_edit_dialog_scroll, &fields_view_area);

    BeginScissorMode((int)fields_view_area.x, (int)fields_view_area.y, (int)fields_view_area.width, (int)fields_view_area.height);

        float current_y_fields_content = 0; // Относительно начала контента ScrollPanel
        float label_width = 100; // Ширина для меток названий колонок
        float textbox_x_fields_content = label_width + PADDING;
        float textbox_width_fields = fields_content_rect.width - textbox_x_fields_content - PADDING; // -PADDING для отступа справа
        if (textbox_width_fields < 100) textbox_width_fields = 100; // Минимальная ширина

        for (int c = 0; c < num_fields; c++) {
            if (c >= MAX_DATA_COLUMNS_MAIN) break;

            const char *col_name = (data->table_data_main.column_names && 
                                    data->table_data_main.column_names[c + 1]) ?
                                   data->table_data_main.column_names[c + 1] : TextFormat("X%d", c + 1);
            
            // Экранные координаты для текущего поля
            float screen_y_field = fields_view_area.y + current_y_fields_content + data->main_edit_dialog_scroll.y;

            GuiLabel((Rectangle){fields_view_area.x + PADDING + data->main_edit_dialog_scroll.x, screen_y_field, label_width, BUTTON_HEIGHT}, col_name);
            
            if (GuiTextBox((Rectangle){fields_view_area.x + textbox_x_fields_content + data->main_edit_dialog_scroll.x, screen_y_field, textbox_width_fields, BUTTON_HEIGHT}, 
                           data->main_edit_dialog_field_buffers[c], TEXT_FIELD_BUFFER_SIZE, 
                           data->main_edit_dialog_field_edit_mode[c])) {
                data->main_edit_dialog_field_edit_mode[c] = !data->main_edit_dialog_field_edit_mode[c];
            }
            current_y_fields_content += BUTTON_HEIGHT + PADDING;
        }

    EndScissorMode();

    // --- Кнопки "Сохранить" и "Отмена" ---
    float button_y_dialog = dialog_rect.y + dialog_rect.height - BUTTON_HEIGHT - PADDING;
    if (GuiButton((Rectangle){dialog_rect.x + PADDING, button_y_dialog, 120, BUTTON_HEIGHT}, "Сохранить")) {
        bool all_valid = true;
        double new_values[MAX_DATA_COLUMNS_MAIN]; 

        for (int c = 0; c < num_fields; c++) {
            if (c >= MAX_DATA_COLUMNS_MAIN) break;
            char *endptr;
            char temp_buffer[TEXT_FIELD_BUFFER_SIZE];
            strncpy(temp_buffer, data->main_edit_dialog_field_buffers[c], TEXT_FIELD_BUFFER_SIZE -1);
            temp_buffer[TEXT_FIELD_BUFFER_SIZE-1] = '\0';
            for(char *p = temp_buffer; *p; ++p) if (*p == ',') *p = '.';

            new_values[c] = strtod(temp_buffer, &endptr);
            if (endptr == temp_buffer || (*endptr != '\0' && !isspace((unsigned char)*endptr))) {
            all_valid = false;
                sprintf(data->status_message, "Ошибка: неверное число в поле '%s'", 
                        (data->table_data_main.column_names && data->table_data_main.column_names[c+1]) ? data->table_data_main.column_names[c+1] : TextFormat("X%d",c+1) );
                data->status_message_timer = 3.0f;
                TraceLog(LOG_WARNING, "Ошибка конвертации для поля %d: '%s'", c, data->main_edit_dialog_field_buffers[c]);
                break;
            }
        }

        if (all_valid) {
            bool db_updated_all = true;
            for (int c = 0; c < num_fields; c++) {
                if (c >= MAX_DATA_COLUMNS_MAIN) break;
                const char *db_col_name = data->table_data_main.column_names[c + 1];
                const char *id_col_name = data->table_data_main.column_names[0];
                int row_id_value = data->table_data_main.epochs[data->main_edit_dialog_row_index];

                if (!update_table_cell_rl(data->db_handle, data->current_main_table_name, db_col_name, id_col_name, new_values[c], row_id_value)) {
                    db_updated_all = false; /* ... обработка ошибки ... */ break;
                }
            }

            if (db_updated_all) {
                for (int c = 0; c < num_fields; c++) {
                     if (c >= MAX_DATA_COLUMNS_MAIN) break;
                    data->table_data_main.table[data->main_edit_dialog_row_index][c] = new_values[c];
                }
                TraceLog(LOG_INFO, "Строка %d успешно обновлена.", data->main_edit_dialog_row_index);
                strcpy(data->status_message, "Строка успешно обновлена.");
                data->status_message_timer = 2.0f;
                close_dialog_request = true;
            }
        }
    }

    if (GuiButton((Rectangle){dialog_rect.x + dialog_rect.width - PADDING - 120, button_y_dialog, 120, BUTTON_HEIGHT}, "Отмена")) {
        close_dialog_request = true;
    }

    if (close_dialog_request) {
        data->show_main_edit_dialog = false;
        data->main_edit_dialog_row_index = -1;
        // Сброс буферов и edit_mode не обязателен, т.к. они перезаполнятся при следующем открытии
    }
}

// --- UpdateDrawAppFrame (основная версия) ---
static void UpdateDrawAppFrame(AppData_Raylib *data) {
    if (!data) return;

    if (data->status_message_timer > 0) {
        data->status_message_timer -= GetFrameTime();
        if (data->status_message_timer <= 0) {
            data->status_message[0] = '\0';
        }
    }

    // Если диалог редактирования активен, блокируем остальной UI
    // Это предотвратит клики "сквозь" диалог
    if (data->show_main_edit_dialog) {
        GuiLock(); 
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Отрисовка основного вида приложения
    switch (data->current_view) {
        case APP_VIEW_MAIN:
            DrawMainView(data); // DrawMainView теперь будет рисовать "под" GuiLock, если диалог активен
            break;
        case APP_VIEW_LEVEL1:
            DrawLevel1View(data); // Аналогично
            break;
        default:
            {
                Font font_to_draw_err = data->app_font_loaded ? data->app_font : GetFontDefault();
                float font_size_to_draw_err = data->app_font_loaded ? data->app_font.baseSize : 20;
                if (font_size_to_draw_err < 10) font_size_to_draw_err = 10;
                DrawTextEx(font_to_draw_err, "Неизвестное состояние приложения!", (Vector2){20, 20}, font_size_to_draw_err, 1, RED);
            }
            break;
    }
    
    // Если диалог был активен, разблокируем UI ПОСЛЕ отрисовки основного вида,
    // чтобы сам диалог мог обрабатывать ввод.
    // Но перед отрисовкой самого диалога.
    if (data->show_main_edit_dialog) {
        GuiUnlock(); 
    }

    // Отрисовка диалога редактирования (если активен)
    // Он будет рисоваться поверх основного UI и теперь должен корректно обрабатывать ввод,
    // так как основной UI был заблокирован на момент его обработки.
    if (data->show_main_edit_dialog) {
        DrawEditMainTableRowDialog(data);
    }

    if (data->status_message_timer > 0 && data->status_message[0] != '\0') {
        GuiStatusBar((Rectangle){0, (float)data->screen_height - 20, (float)data->screen_width, 20}, data->status_message);
    }
    DrawFPS(data->screen_width - 90, 10);

    EndDrawing();
}