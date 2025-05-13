#ifndef LEVEL1_ANALYSIS_H
#define LEVEL1_ANALYSIS_H

#include <gtk/gtk.h>
#include <glib.h>
#include <sqlite3.h> // <-- Включаем, так как AppData содержит sqlite3**
#include "sqlite_utils.h" // <-- Включаем, так как AppData содержит TableData

// =============================================================
// == ОПРЕДЕЛЕНИЕ СТРУКТУРЫ AppData ПЕРЕНЕСЕНО СЮДА из main.c ==
// =============================================================
typedef struct AppData {
    GtkWindow *parent;
    sqlite3 **db; // Требует sqlite3.h
    GtkWidget *main_box;

    // Основная таблица
    TableData *table_data_main; // Требует sqlite_utils.h
    GtkWidget *tree_view_main;
    GtkListStore *store_main; // <-- Старый тип GtkListStore (пока оставляем)
    GtkWidget *scrolled_window_main;
    char *current_main_table_name;

    // Таблица "Значения"
    TableData *table_data_values; // Требует sqlite_utils.h
    GtkWidget *tree_view_values;
    GtkListStore *store_values; // <-- Старый тип GtkListStore (пока оставляем)
    GtkWidget *scrolled_window_values;
    const char *values_table_name;

    // Схема
    GtkWidget *picture_schema;
    GdkPixbuf *pixbuf_schema;   // Требует gtk/gtk.h (через gdk)
    const char *schema_blob_column_name;

    // Кнопки и флаги
    GtkWidget *open_button;
    GtkWidget *add_row_button;
    GtkWidget *level1_button;
    gboolean db_opened; // Требует glib.h

    // === Поля для Уровня 1 (управляются из level1_analysis.c) ===
    GtkWidget *level1_window;
    GtkWidget *level1_container;
    GtkWidget *level1_tree_view;
    GtkListStore *level1_store; // <-- Старый тип GtkListStore
    GtkWidget *level1_smoothing_view;
    GtkListStore *level1_smoothing_store; // <-- Старый тип GtkListStore
    GtkWidget *level1_drawing_area;
    GtkWidget *level1_alpha_smoothing_view;
    GtkListStore *level1_alpha_smoothing_store; // <-- Старый тип GtkListStore
    GtkWidget *level1_alpha_drawing_area;
    // Данные L1
    double **smoothing_data;
    double **smoothing_alpha_data;
    double *mu_data;
    double *alpha_data;
    // Размеры данных L1
    int smoothing_row_count;
    int smoothing_alpha_row_count;
    int alpha_count;

} AppData; // <--- Имя типа здесь

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