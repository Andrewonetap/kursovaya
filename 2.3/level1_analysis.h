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
    sqlite3 **db;
    GtkWidget *main_box;

    TableData *table_data_main;
    GtkWidget *tree_view_main;
    GtkListStore *store_main;
    GtkWidget *scrolled_window_main;
    char *current_main_table_name;

    TableData *table_data_values;
    GtkWidget *tree_view_values;
    GtkListStore *store_values;
    GtkWidget *scrolled_window_values;
    const char *values_table_name;

    GtkWidget *picture_schema;
    GdkPixbuf *pixbuf_schema;
    const char *schema_blob_column_name;

    GtkWidget *open_button;
    GtkWidget *add_row_button;
    GtkWidget *edit_row_button;
    GtkWidget *edit_values_button;
    GtkWidget *level1_button;
    gboolean db_opened;

    GtkWidget *level1_window;

    // --- Вкладка: Исходные данные ---
    GtkWidget *level1_tree_view;
    GtkListStore *level1_store;
    GtkWidget *level1_smoothing_view;
    GtkListStore *level1_smoothing_store;
    GtkWidget *level1_drawing_area;
    GtkWidget *level1_alpha_smoothing_view;
    GtkListStore *level1_alpha_smoothing_store;
    GtkWidget *level1_alpha_drawing_area;

    // --- Вкладка: Данные +E ---
    GtkWidget *level1_error_adj_tree_view;
    GtkListStore *level1_error_adj_store;
    // Удалены виджеты для сглаживания X0+E
    GtkWidget *level1_mu_prime_smoothing_view;
    GtkListStore *level1_mu_prime_smoothing_store;
    GtkWidget *level1_mu_prime_drawing_area;
    GtkWidget *level1_alpha_prime_smoothing_view;
    GtkListStore *level1_alpha_prime_smoothing_store;
    GtkWidget *level1_alpha_prime_drawing_area;

    // --- Вкладка: Данные -E ---
    GtkWidget *level1_error_sub_tree_view;
    GtkListStore *level1_error_sub_store;
    // Удалены виджеты для сглаживания X0-E
    GtkWidget *level1_mu_double_prime_smoothing_view;
    GtkListStore *level1_mu_double_prime_smoothing_store;
    GtkWidget *level1_mu_double_prime_drawing_area;
    GtkWidget *level1_alpha_double_prime_smoothing_view;
    GtkListStore *level1_alpha_double_prime_smoothing_store;
    GtkWidget *level1_alpha_double_prime_drawing_area;

    double **error_adjusted_table_data;
    double **error_subtracted_table_data;

    double *mu_data;
    double *alpha_data;
    double *mu_prime_data;
    double *alpha_prime_data;
    double *mu_double_prime_data;
    double *alpha_double_prime_data;

    double **smoothing_data;
    double **smoothing_alpha_data;
    // Удалены данные сглаживания X0+E
    // double **smoothing_error_adjusted_data_x0;
    double **smoothing_mu_prime_data;
    double **smoothing_alpha_prime_data;
    // Удалены данные сглаживания X0-E
    // double **smoothing_error_subtracted_data_x0;
    double **smoothing_mu_double_prime_data;
    double **smoothing_alpha_double_prime_data;

    int alpha_count;
    int mu_prime_data_count;
    int alpha_prime_data_count;
    int mu_double_prime_data_count;
    int alpha_double_prime_data_count;

    int smoothing_row_count;
    int smoothing_alpha_row_count;
    // Удалены счетчики строк для сглаживания X0+E
    // int smoothing_error_adjusted_data_x0_row_count;
    int smoothing_mu_prime_row_count;
    int smoothing_alpha_prime_row_count;
    // Удалены счетчики строк для сглаживания X0-E
    // int smoothing_error_subtracted_data_x0_row_count;
    int smoothing_mu_double_prime_row_count;
    int smoothing_alpha_double_prime_row_count;

    // --- НОВЫЕ ПОЛЯ: Вкладка Заключение ---
    GtkWidget *conclusion_alpha_mu_table_view;   // Таблица α(μ)
    GtkListStore *conclusion_alpha_mu_store;     // Модель для таблицы α(μ)
    GtkWidget *conclusion_alpha_mu_drawing_area; // График α(μ)

} AppData;



void level1_show_window(AppData *data);
void level1_update_content_if_visible(AppData *data);
void level1_cleanup_calculated_data(AppData *data);
void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);
gboolean on_inner_scroll_event (GtkEventController *controller_generic, double dx, double dy, gpointer user_data_tab_scroller);
void conclusion_draw_alpha_mu_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);


#endif // LEVEL1_ANALYSIS_H