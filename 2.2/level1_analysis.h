#ifndef LEVEL1_ANALYSIS_H
#define LEVEL1_ANALYSIS_H

#include <gtk/gtk.h>
#include <glib.h>
#include <sqlite3.h> // <-- Включаем, так как AppData содержит sqlite3**
#include "sqlite_utils.h" // <-- Включаем, так как AppData содержит TableData

// =============================================================
// == ОПРЕДЕЛЕНИЕ СТРУКТУРЫ AppData ПЕРЕНЕСЕНО СЮДА из main.c ==
// =============================================================
// В level1_analysis.h, внутри структуры AppData
typedef struct AppData {
    GtkWindow *parent;
    sqlite3 **db;
    GtkWidget *main_box;

    // Основная таблица
    TableData *table_data_main;
    GtkWidget *tree_view_main;
    GtkListStore *store_main;
    GtkWidget *scrolled_window_main;
    char *current_main_table_name;

    // Таблица "Значения"
    TableData *table_data_values;
    GtkWidget *tree_view_values;
    GtkListStore *store_values;
    GtkWidget *scrolled_window_values;
    const char *values_table_name;

    // Схема
    GtkWidget *picture_schema;
    GdkPixbuf *pixbuf_schema;
    const char *schema_blob_column_name;

    // Кнопки и флаги
    GtkWidget *open_button;
    GtkWidget *add_row_button;
    GtkWidget *level1_button;
    gboolean db_opened;

    // === Поля для Уровня 1 (управляются из level1_analysis.c) ===
    GtkWidget *level1_window;
    GtkWidget *level1_container; // Можно будет удалить, если больше не используется

    // Таблица L1 (данные, mu, alpha)
    GtkWidget *level1_tree_view;
    GtkListStore *level1_store;

    // Сглаживание mu
    GtkWidget *level1_smoothing_view;
    GtkListStore *level1_smoothing_store;
    GtkWidget *level1_drawing_area; // График для mu

    // Сглаживание alpha
    GtkWidget *level1_alpha_smoothing_view;
    GtkListStore *level1_alpha_smoothing_store;
    GtkWidget *level1_alpha_drawing_area; // График для alpha

    // Таблица X+E (данные с погрешностью, mu_prime, alpha_prime)
    GtkWidget *level1_error_adj_tree_view;
    GtkListStore *level1_error_adj_store;
    
    // Сглаживание X0+E (первая колонка из X+E)
    GtkWidget *level1_error_adj_smoothing_view;
    GtkListStore *level1_error_adj_smoothing_store;
    GtkWidget *level1_error_adj_drawing_area; // График для X0+E

    // === НОВЫЕ ПОЛЯ: Для сглаживания μ' (мю с погрешностью E) ===
    GtkWidget *level1_mu_prime_smoothing_view;
    GtkListStore *level1_mu_prime_smoothing_store;
    GtkWidget *level1_mu_prime_drawing_area; // График для mu_prime
    
    // === НОВЫЕ ПОЛЯ: Для сглаживания α' (альфа с погрешностью E) ===
    GtkWidget *level1_alpha_prime_smoothing_view;
    GtkListStore *level1_alpha_prime_smoothing_store;
    GtkWidget *level1_alpha_prime_drawing_area; // График для alpha_prime
    
    // Данные расчетов
    double **table_data_main_table_copy_for_l1; // Если нужна копия для L1
    double **error_adjusted_table_data; // X+E

    double *mu_data;
    double *alpha_data;
    double *mu_prime_data;
    double *alpha_prime_data;

    double **smoothing_data; // для mu
    double **smoothing_alpha_data; // для alpha
    double **smoothing_error_adjusted_data_x0; // для X0+E
    double **smoothing_mu_prime_data;    // НОВОЕ: для mu_prime
    double **smoothing_alpha_prime_data; // НОВОЕ: для alpha_prime

    // Размеры данных
    int alpha_count;
    int mu_prime_data_count;
    int alpha_prime_data_count;
    int smoothing_row_count;
    int smoothing_alpha_row_count;
    int smoothing_error_adjusted_data_x0_row_count;
    int smoothing_mu_prime_row_count;    // НОВОЕ
    int smoothing_alpha_prime_row_count; // НОВОЕ

} AppData;

// === Публичные функции для level1 ===

/**
 * @brief Создает (если не существует) и показывает окно Уровня 1.
 */
void level1_show_window(AppData *data);

/**
 * @brief Обновляет содержимое окна Уровня 1 (расчеты, таблицы, графики),
 *        НО только если окно существует и видимо.
 */
void level1_update_content_if_visible(AppData *data);

/**
 * @brief Выполняет очистку ресурсов, связанных ТОЛЬКО с данными расчетов уровня 1.
 */
void level1_cleanup_calculated_data(AppData *data);


#endif // LEVEL1_ANALYSIS_H