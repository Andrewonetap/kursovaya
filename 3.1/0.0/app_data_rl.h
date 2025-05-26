#ifndef APP_DATA_RL_H
#define APP_DATA_RL_H

#include <sqlite3.h>
#include <stdbool.h>
#include "sqlite_utils_rl.h" // Наша новая версия
#include "raylib.h"          // Для Texture2D, Rectangle, Vector2 и т.д.
// #include "raygui.h"       // Если понадобятся специфичные типы RayGUI здесь (обычно нет)

#define MAX_DATA_COLUMNS_MAIN 30 // Пример, поставь свое максимальное значение
#define TEXT_FIELD_BUFFER_SIZE 64
#define MAX_DATA_COLUMNS_VALUES 5 // С запасом, если вдруг колонок больше
#define TOP_NAV_BAR_HEIGHT 40

// Enum для состояний UI (какое "окно" или "вид" активен)
typedef enum {
    APP_VIEW_MAIN,
    APP_VIEW_LEVEL1
    // Можно добавить другие, если будут отдельные "окна"
} AppActiveView;

// Enum для активной вкладки в представлении "Уровень 1"
typedef enum {
    L1_TAB_INITIAL_DATA,
    L1_TAB_DATA_PLUS_E,
    L1_TAB_DATA_MINUS_E,
    L1_TAB_CONCLUSION
} Level1ActiveTab;

// Enum для идентификации различных графиков, чтобы хранить их состояние
typedef enum {
    GRAPH_ID_L1_MU,
    GRAPH_ID_L1_ALPHA,
    GRAPH_ID_L1_MU_PRIME,
    GRAPH_ID_L1_ALPHA_PRIME,
    GRAPH_ID_L1_MU_DPRIME,
    GRAPH_ID_L1_ALPHA_DPRIME,
    GRAPH_ID_L1_CONCLUSION_ALPHA_MU,
    GRAPH_ID_COUNT // Общее количество графиков для массивов состояния
} GraphIdentifier;

// Состояние для одного графика (масштаб, смещение)
typedef struct {
    Vector2 offset;   // Смещение начала координат данных (в единицах данных)
    Vector2 scale;    // Масштаб (множитель для координат данных)
    Rectangle plot_bounds; // Экранные координаты области, где рисуется сам график
    bool is_active;   // Активен ли этот график для взаимодействия мышью
} GraphDisplayState;

typedef struct AppData_Raylib {
    // --- Основные данные и состояние ---
    sqlite3 *db_handle; // Указатель на открытую БД (было sqlite3**)
    bool db_opened;
    char current_db_path[1024]; // Путь к текущему файлу БД

    TableData table_data_main;   // Храним по значению. Инициализировать нулями!
    TableData table_data_values; // Храним по значению. Инициализировать нулями!

    char current_main_table_name[256]; // Буфер для имени таблицы
    char values_table_name[256];       // Имя таблицы "Значения" (можно задать по умолчанию)
    char schema_blob_column_name[256]; // Имя колонки BLOB (можно задать по умолчанию)

    Texture2D schema_texture;
    bool schema_texture_loaded;

    Font app_font; // Для хранения загруженного шрифта
    bool app_font_loaded;

    // --- Состояние UI ---
    AppActiveView current_view;
    Level1ActiveTab l1_active_tab;

    // Состояния для элементов управления RayGUI
    char db_file_path_text_box[1024]; // Буфер для GuiTextBox пути к файлу БД
    bool db_file_path_edit_mode;

    // Состояния для таблиц (выбранная строка, смещение прокрутки)
    int main_table_selected_row;
    Vector2 main_table_scroll;      // Смещение для GuiScrollPanel
    Rectangle main_table_view_rect; // Область, видимая через GuiScrollPanel

    int values_table_selected_row;
    Vector2 values_table_scroll;
    Rectangle values_table_view_rect;

    // Состояния для диалога редактирования строки (основная таблица)
    bool show_main_edit_dialog;
    int main_edit_dialog_row_index; // Индекс строки в TableData
    // Буферы для полей ввода в диалоге (размер зависит от макс. числа колонок)
    // Пример: char main_edit_dialog_field_buffers[MAX_COLS_MAIN][MAX_TEXT_FIELD_LEN];
    // Для простоты, можно использовать один общий буфер и флаги, или массив структур
    // Пока оставим это для детальной реализации диалогов

    // Состояния для диалога редактирования строки (таблица "Значения")
    bool show_values_edit_dialog;
    int values_edit_dialog_row_index;

    // Состояния для таблиц и графиков в "Окне Уровня 1"
    Vector2 l1_tables_scroll[L1_TAB_CONCLUSION + 1][3]; // [вкладка][тип_таблицы_на_вкладке]
                                                        // 0-основная, 1-сглаживание1, 2-сглаживание2
    GraphDisplayState graph_states[GRAPH_ID_COUNT];

    // --- Вычисленные данные (остаются как есть по структуре) ---
    double **error_adjusted_table_data; // X+E
    int error_adjusted_rows_alloc;
    double **error_subtracted_table_data; // X-E
    int error_subtracted_rows_alloc;

    double *mu_data;
    int mu_data_count; // Добавил счетчик, т.к. alpha_count был, а для mu нет
    double *alpha_data;
    int alpha_count;

    double *mu_prime_data;
    int mu_prime_data_count;
    double *alpha_prime_data;
    int alpha_prime_data_count;

    double *mu_double_prime_data;
    int mu_double_prime_data_count;
    double *alpha_double_prime_data;
    int alpha_double_prime_data_count;

    double **smoothing_data; // для mu
    int smoothing_row_count;
    double **smoothing_alpha_data;
    int smoothing_alpha_row_count;

    double **smoothing_mu_prime_data;
    int smoothing_mu_prime_row_count;
    double **smoothing_alpha_prime_data;
    int smoothing_alpha_prime_row_count;

    double **smoothing_mu_double_prime_data;
    int smoothing_mu_double_prime_row_count;
    double **smoothing_alpha_double_prime_data;
    int smoothing_alpha_double_prime_row_count;

    // Данные для таблицы "Заключение" (α от μ)
    // Будут собираться "на лету" или во временные структуры перед отрисовкой
    Vector2 l1_conclusion_table_scroll;

    // Размеры окна и панелей
    int screen_width;
    int screen_height;
    float main_h_split_pos; // Позиция горизонтального разделителя на главном экране

    // Флаги для управления видимостью панелей/секций (если нужно)
    bool show_right_panel_main_view;

    // Сообщение для пользователя (аналог show_message_dialog)
    char status_message[512];
    float status_message_timer; // Сколько времени показывать сообщение


    char main_edit_dialog_field_buffers[MAX_DATA_COLUMNS_MAIN][TEXT_FIELD_BUFFER_SIZE];
    bool main_edit_dialog_field_edit_mode[MAX_DATA_COLUMNS_MAIN]; // Для GuiTextBox

    // Состояния для диалога редактирования таблицы "Значения"
    // Предположим, у нас есть колонки A, E, Количество (3 числовых поля)
    // Имена колонок будут браться из data->table_data_values.column_names
    char values_edit_dialog_field_buffers[MAX_DATA_COLUMNS_VALUES][TEXT_FIELD_BUFFER_SIZE];
    bool values_edit_dialog_field_edit_mode[MAX_DATA_COLUMNS_VALUES];
    Vector2 values_edit_dialog_scroll;

    Vector2 main_edit_dialog_scroll;

} AppData_Raylib;

void DrawDataTable_RL(AppData_Raylib *data, TableData *tableData, const char* title, 
                      Rectangle panel_bounds, Vector2 *scroll, int *selectedRow, bool isValuesTableType);


#endif // APP_DATA_RL_H