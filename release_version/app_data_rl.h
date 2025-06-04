#ifndef APP_DATA_RL_H // защита от повторного включения
#define APP_DATA_RL_H

#include <sqlite3.h> // для работы с бд sqlite
#include <stdbool.h> // булевы типы
#include "sqlite_utils_rl.h" // наши утилиты для sqlite
#include "raylib.h"   // графика raylib
#include "raygui.h"     // гуи элементы raygui

#define MAX_DATA_COLUMNS_MAIN 30 // макс колонок в главной таблице
#define TEXT_FIELD_BUFFER_SIZE 64 // размер буфера для текстовых полей
#define MAX_DATA_COLUMNS_VALUES 5 // макс колонок в таблице "значения"
#define TOP_NAV_BAR_HEIGHT 40 // высота верхней навигационной панели
#define TABLE_HEADER_HEIGHT 25.0f // высота заголовка таблицы
#define TABLE_ROW_HEIGHT 22.0f // высота строки таблицы
#define TABLE_EPOCH_COL_WIDTH 70.0f // ширина колонки эпох в таблице
#define TABLE_DATA_COL_WIDTH 90.0f // ширина колонки данных в таблице
#define PADDING 10 // общие отступы
#define BUTTON_HEIGHT 30 // высота кнопок
#define L4_NUM_ZOOM_PRESETS 5 // количество предустановок зума для уровня 4

// флаги скрытия колонок
extern bool HIDE_ROWID_COLUMN_FOR_VALUES_TABLE; // скрывать столбец rowid в таблице "значения"
extern bool HIDE_SCHEMA_COLUMN_IN_VALUES_TABLE; // скрывать столбец схемы в таблице "значения"

// типы активных окон в приложении
typedef enum {
    APP_VIEW_MAIN,       // главный вид
    APP_VIEW_LEVEL1,     // уровень 1
    // APP_VIEW_LEVEL2,
    // APP_VIEW_LEVEL3,
    APP_VIEW_LEVEL4      // уровень 4
} AppActiveView;

// типы активных вкладок для уровня 1
typedef enum {
    L1_TAB_INITIAL_DATA,  // начальные данные
    L1_TAB_DATA_PLUS_E,   // данные + ошибка
    L1_TAB_DATA_MINUS_E,  // данные - ошибка
    L1_TAB_CONCLUSION     // заключение
} Level1ActiveTab;

// идентификаторы разных графиков для хранения состояния
typedef enum {
    GRAPH_ID_L1_MU,                // мю на уровне 1
    GRAPH_ID_L1_ALPHA,             // альфа на уровне 1
    GRAPH_ID_L1_MU_PRIME,          // мю штрих на уровне 1
    GRAPH_ID_L1_ALPHA_PRIME,       // альфа штрих на уровне 1
    GRAPH_ID_L1_MU_DPRIME,         // мю два штриха на уровне 1
    GRAPH_ID_L1_ALPHA_DPRIME,      // альфа два штриха на уровне 1
    GRAPH_ID_L1_CONCLUSION_ALPHA_MU, // альфа от мю в заключении
    GRAPH_ID_COUNT                 // общее количество графиков
} GraphIdentifier;

// состояние одного графика (масштаб смещение)
typedef struct {
    Vector2 offset;             // смещение
    Vector2 scale;              // масштаб
    Rectangle plot_bounds;      // границы области отрисовки
    bool is_active;             // активен ли (для интерактивности)

    bool is_panning;            // в процессе перетаскивания
    Vector2 pan_start_mouse_pos; // начальная позиция мыши при перетаскивании
    Vector2 pan_start_offset;   // начальное смещение при перетаскивании
} GraphDisplayState;

// набор цветов для темы оформления
typedef struct ThemeColors {
    // основные цвета интерфейса
    unsigned int window_background;         // фон окна
    unsigned int panel_background;          // фон панелей
    unsigned int header_toolbar_bg;         // фон заголовка/тулбара
    unsigned int text_primary;              // основной цвет текста
    unsigned int text_disabled;             // цвет неактивного текста
    unsigned int border_universal;          // цвет общих границ
    unsigned int border_focused_accent;     // цвет границы активного элемента

    // кнопки
    unsigned int button_base_normal;        // обычное состояние кнопки
    unsigned int button_base_focused;       // кнопка в фокусе
    unsigned int button_base_pressed;       // нажатая кнопка
    unsigned int button_base_disabled;      // неактивная кнопка
    unsigned int text_on_button_normal;     // текст на обычной кнопке
    unsigned int text_on_button_active;     // текст на активной/нажатой кнопке
    unsigned int button_text_disabled;      // текст на неактивной кнопке

    // списки таблицы
    unsigned int list_item_bg;              // фон элемента списка
    unsigned int list_item_alt_bg;          // чередующийся фон элемента списка
    unsigned int list_item_selected_bg;     // фон выбранного элемента
    unsigned int list_item_selected_text;   // текст выбранного элемента
    unsigned int list_header_bg;            // фон заголовка списка/таблицы
    unsigned int list_selected_border;      // рамка выбранного элемента

    // поля ввода
    unsigned int textbox_bg_normal;         // фон текстового поля

    // скроллбары
    unsigned int scrollbar_track_bg;        // фон трека скроллбара
    unsigned int scrollbar_thumb_bg;        // фон ползунка скроллбара
    unsigned int scrollbar_thumb_focused;   // ползунок в фокусе
    unsigned int scrollbar_thumb_pressed;   // нажатый ползунок

    // другие специфичные цвета
    unsigned int modal_overlay_color;       // цвет затемнения для модальных окон
    unsigned int error_text_color;          // цвет текста ошибок
    unsigned int placeholder_text_color;    // цвет плейсхолдера в полях ввода
} ThemeColors;

// основная структура с данными приложения
typedef struct AppData_Raylib {
    // --- основные данные и состояние ---
    sqlite3 *db_handle;                 // указатель на открытую бд
    bool db_opened;                     // флаг открытия бд
    char current_db_path[1024];         // путь к текущему файлу бд

    TableData table_data_main;          // данные главной таблицы
    TableData table_data_values;        // данные таблицы "значения"

    char current_main_table_name[256];  // имя текущей главной таблицы
    char values_table_name[256];        // имя таблицы "значения"
    char schema_blob_column_name[256];  // имя blob колонки со схемой


    TableList_RL available_main_tables;     // Список таблиц, подходящих на роль main_table (исключая "Значения")
    int selected_main_table_index;          // Индекс выбранной таблицы из available_main_tables (-1 если не выбрана)
    bool main_table_dropdown_active;        // Флаг для отображения выпадающего списка выбора главной таблицы
    Rectangle main_table_dropdown_button_rect; // Область кнопки для выпадающего списка
    Rectangle main_table_dropdown_panel_rect;  // Область самого выпадающего списка
    
    
    Texture2D schema_texture;           // текстура для схемы
    bool schema_texture_loaded;         // флаг загрузки текстуры схемы

    Font app_font;                      // загруженный шрифт приложения
    bool app_font_loaded;               // флаг загрузки шрифта

    // --- состояние ui ---
    AppActiveView current_view;         // текущее активное окно/вид
    Level1ActiveTab l1_active_tab;      // активная вкладка на уровне 1

    // состояния для элементов raygui
    char db_file_path_text_box[1024];   // буфер для поля ввода пути к файлу бд
    bool db_file_path_edit_mode;        // режим редактирования поля пути к бд

    // состояния таблиц (выбранная строка прокрутка)
    int main_table_selected_row;        // индекс выбранной строки в главной таблице
    Vector2 main_table_scroll;          // смещение прокрутки главной таблицы
    Rectangle main_table_view_rect;     // видимая область главной таблицы

    int values_table_selected_row;      // индекс выбранной строки в таблице "значения"
    Vector2 values_table_scroll;        // смещение прокрутки таблицы "значения"
    Rectangle values_table_view_rect;   // видимая область таблицы "значения"

    // состояние диалога редактирования строки главной таблицы
    bool show_main_edit_dialog;         // показывать ли диалог редактирования
    int main_edit_dialog_row_index;     // индекс редактируемой строки

    // состояние диалога редактирования строки таблицы "значения"
    bool show_values_edit_dialog;       // показывать ли диалог
    int values_edit_dialog_row_index;   // индекс строки

    // состояния таблиц и графиков для уровня 1
    Vector2 l1_tables_scroll[L1_TAB_CONCLUSION + 1][3]; // прокрутка таблиц уровня 1 [вкладка][тип_таблицы]
    int l1_conclusion_table_selected_row;               // выбранная строка в таблице заключение
    GraphDisplayState graph_states[GRAPH_ID_COUNT];     // состояния всех графиков



    double *mu_data;                    // данные мю
    int mu_data_count;                  // количество элементов мю
    double *alpha_data;                 // данные альфа
    int alpha_count;                    // количество элементов альфа

    double *mu_prime_data;              // данные мю штрих
    int mu_prime_data_count;            // количество
    double *alpha_prime_data;           // данные альфа штрих
    int alpha_prime_data_count;         // количество

    double *mu_double_prime_data;       // данные мю два штриха
    int mu_double_prime_data_count;     // количество
    double *alpha_double_prime_data;    // данные альфа два штриха
    int alpha_double_prime_data_count;  // количество

    double **smoothing_data;            // данные сглаживания для мю
    int smoothing_row_count;            // количество строк
    double **smoothing_alpha_data;      // данные сглаживания для альфа
    int smoothing_alpha_row_count;      // количество строк

    double **smoothing_mu_prime_data;   // сглаживание для мю штрих
    int smoothing_mu_prime_row_count;
    double **smoothing_alpha_prime_data; // сглаживание для альфа штрих
    int smoothing_alpha_prime_row_count;

    double **smoothing_mu_double_prime_data; // сглаживание для мю два штриха
    int smoothing_mu_double_prime_row_count;
    double **smoothing_alpha_double_prime_data; // сглаживание для альфа два штриха
    int smoothing_alpha_double_prime_row_count;

    // данные для таблицы заключение (альфа от мю)
    Vector2 l1_conclusion_table_scroll; // прокрутка таблицы заключение

    // размеры окна и панелей
    int screen_width;                   // ширина окна приложения
    int screen_height;                  // высота окна приложения
    float main_h_split_pos;             // позиция горизонтального разделителя на главном экране

    // флаги видимости панелей
    bool show_right_panel_main_view;    // показывать правую панель на главном виде

    // сообщение пользователю
    char status_message[512];           // текст статусного сообщения
    float status_message_timer;         // таймер отображения сообщения

    // буферы для полей ввода диалога редактирования главной таблицы
    char main_edit_dialog_field_buffers[MAX_DATA_COLUMNS_MAIN][TEXT_FIELD_BUFFER_SIZE];
    bool main_edit_dialog_field_edit_mode[MAX_DATA_COLUMNS_MAIN]; // режим редактирования для каждого поля

    // буферы и состояния для диалога редактирования таблицы "значения"
    char values_edit_dialog_field_buffers[MAX_DATA_COLUMNS_VALUES][TEXT_FIELD_BUFFER_SIZE];
    bool values_edit_dialog_field_edit_mode[MAX_DATA_COLUMNS_VALUES];
    Vector2 values_edit_dialog_scroll;      // прокрутка в диалоге

    Vector2 main_edit_dialog_scroll;        // прокрутка в диалоге главной таблицы

    bool levelsDropdownActive;              // активен ли выпадающий список уровней
    Rectangle levelsDropdownRect;           // геометрия выпадающего списка
    Rectangle levelsMainButtonRect;         // геометрия кнопки "уровни"

    // --- данные и состояние уровня 4 ---
    float l4_smoothing_alpha_coeff;         // коэф сглаживания альфа для уровня 4
    bool l4_alpha_coeff_loaded;             // флаг загрузки коэф

    double **l4_smoothed_series_data;       // данные сглаженных рядов [серия][эпоха]
    int l4_num_series;                      // количество временных рядов
    int l4_num_epochs_original;             // количество исходных эпох

    char **l4_series_names;                 // имена рядов (из колонок главной таблицы)
    bool *l4_series_selected_for_graph;     // флаги выбора рядов для графика

    Vector2 l4_series_list_scroll;          // прокрутка списка выбора рядов

    GraphDisplayState l4_main_graph_state;  // состояние основного графика уровня 4

    bool l4_data_calculated;                // флаг расчета данных для уровня 4

    ThemeColors current_theme_colors;       // текущие цвета темы
    int current_theme_id;                   // id текущей темы (0 светлая 1 темная и тд)

    bool show_help_window;                  // показывать окно справки

} AppData_Raylib;

// функция отрисовки таблицы данных
void DrawDataTable_RL(AppData_Raylib *data, TableData *tableData, const char* title, 
                      Rectangle panel_bounds, Vector2 *scroll, int *selectedRow, bool isValuesTableType);


#endif // APP_DATA_RL_H