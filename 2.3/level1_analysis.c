#include "level1_analysis.h" // Содержит AppData и, возможно, GraphDataType (если не в ui_template_builders.h)
#include "calculations.h"
#include "sqlite_utils.h"
#include "ui_template_builders.h" // НОВЫЙ INCLUDE

#include <gtk/gtk.h>
#include <glib.h>
#include <cairo.h>
#include <math.h> // Для M_PI, isnan, isinf
#include <stdio.h>
#include <stdarg.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Extern объявления функций из main.c
extern void update_tree_view_columns(AppData *data,
                              gboolean is_main_table_context,             // 1
                              gboolean is_level1_main_context,            // 2
                              gboolean is_level1_error_adj_context,       // 3
                              gboolean is_level1_error_sub_context,       // 4
                              gboolean is_smoothing_mu_table,             // 5
                              gboolean is_smoothing_alpha_table,          // 6
                              gboolean is_smoothing_mu_prime_table,       // 7
                              gboolean is_smoothing_alpha_prime_table,    // 8
                              gboolean is_smoothing_mu_double_prime_table,// 9
                              gboolean is_smoothing_alpha_double_prime_table // 10
                             );

extern void show_message_dialog(GtkWindow *parent, const char *message);
extern void cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);

// Локальный логгер
static FILE *debug_log_l1 = NULL;

static void init_l1_debug_log() {
    if (!debug_log_l1) {
        debug_log_l1 = fopen("debug.log", "a");
        if (!debug_log_l1) { fprintf(stderr, "Не удалось открыть debug.log (level1_analysis.c)\n"); }
        else { fseek(debug_log_l1, 0, SEEK_END); if (ftell(debug_log_l1) == 0) fprintf(debug_log_l1, "\xEF\xBB\xBF"); fflush(debug_log_l1); }
    }
}
static void debug_print_l1(const char *format, ...) {
    init_l1_debug_log();
    if (debug_log_l1) { va_list args; va_start(args, format); vfprintf(debug_log_l1, format, args); va_end(args); fflush(debug_log_l1); }
}

// Прототипы статических функций этого файла
static void clear_error_adjusted_data(AppData *data); // Уже была
static void level1_clear_internal_data(AppData *data); // Уже была, но обновится
void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data); // Уже была, но обновится
static void level1_perform_calculations_and_update_ui(AppData *data); // Уже была, но обновится
static void level1_on_window_destroy(GtkWidget *widget, gpointer user_data); // Уже была, но обновится
gboolean on_inner_scroll_event (GtkEventController *controller_generic, double dx, double dy, gpointer user_data_tab_scroller);


// --- Реализации статических функций ---

static void clear_error_adjusted_data(AppData *data) { // Эта функция остается, т.к. error_adjusted_table_data используется для X+E
    if (data->error_adjusted_table_data) {
        if (data->table_data_main && data->table_data_main->row_count > 0) {
            for (int i = 0; i < data->table_data_main->row_count; i++) {
                if (data->error_adjusted_table_data[i]) {
                    g_free(data->error_adjusted_table_data[i]);
                }
            }
        }
        g_free(data->error_adjusted_table_data);
        data->error_adjusted_table_data = NULL;
    }
}

static void level1_clear_internal_data(AppData *data) {
    debug_print_l1("level1_clear_internal_data: Очистка всех вычисленных данных Уровня 1 (без X0)...\n");

    // 1. Основной анализ (μ, α и их сглаживание)
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; i++) {
            if (data->smoothing_data[i]) g_free(data->smoothing_data[i]);
        }
        g_free(data->smoothing_data);
        data->smoothing_data = NULL;
        data->smoothing_row_count = 0;
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; i++) {
            if (data->smoothing_alpha_data[i]) g_free(data->smoothing_alpha_data[i]);
        }
        g_free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL;
        data->smoothing_alpha_row_count = 0;
    }
    if (data->mu_data) {
        g_free(data->mu_data);
        data->mu_data = NULL;
    }
    if (data->alpha_data) {
        g_free(data->alpha_data);
        data->alpha_data = NULL;
        data->alpha_count = 0;
    }

    // 2. Данные X+E и их производные (μ', α')
    clear_error_adjusted_data(data); // Очищает data->error_adjusted_table_data

    if (data->mu_prime_data) {
        g_free(data->mu_prime_data);
        data->mu_prime_data = NULL;
        data->mu_prime_data_count = 0;
    }
    if (data->alpha_prime_data) {
        g_free(data->alpha_prime_data);
        data->alpha_prime_data = NULL;
        data->alpha_prime_data_count = 0;
    }    

    // 3. Сглаживание для X0+E (первая колонка данных с погрешностью)
    if (data->smoothing_mu_prime_data) {
        for (int i = 0; i < data->smoothing_mu_prime_row_count; i++) {
            if (data->smoothing_mu_prime_data[i]) g_free(data->smoothing_mu_prime_data[i]);
        }
        g_free(data->smoothing_mu_prime_data);
        data->smoothing_mu_prime_data = NULL;
        data->smoothing_mu_prime_row_count = 0;
    }
    
    // 4. Сглаживание для μ' (мю с погрешностью E)
    if (data->smoothing_alpha_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_prime_row_count; i++) {
            if (data->smoothing_alpha_prime_data[i]) g_free(data->smoothing_alpha_prime_data[i]);
        }
        g_free(data->smoothing_alpha_prime_data);
        data->smoothing_alpha_prime_data = NULL;
        data->smoothing_alpha_prime_row_count = 0;
    }
    
    // 5. Данные X-E и их производные (μ'', α'')
    if (data->error_subtracted_table_data) {
        // Используем data->table_data_main->row_count, если он доступен и соответствует
        // количеству строк, для которых было выделено error_subtracted_table_data.
        // Если table_data_main может быть NULL на этом этапе, нужна другая логика или сохранение размера.
        // Для безопасности, проверим data->table_data_main.
        if (data->table_data_main && data->table_data_main->row_count > 0) {
             for (int i = 0; i < data->table_data_main->row_count; i++) {
                if (data->error_subtracted_table_data[i]) {
                    g_free(data->error_subtracted_table_data[i]);
                }
            }
        } else { // Если нет main_row_count, пытаемся оценить или просто освобождаем основной указатель
             // Это менее безопасно, если строки не были очищены индивидуально.
             // Но если data->error_subtracted_table_data[i] были аллоцированы, а main_row_count нет,
             // то это уже проблема консистентности данных.
             // В текущей логике main_row_count всегда должен быть известен при создании этих данных.
        }
        g_free(data->error_subtracted_table_data);
        data->error_subtracted_table_data = NULL;
    }

    if (data->mu_double_prime_data) {
        g_free(data->mu_double_prime_data);
        data->mu_double_prime_data = NULL;
        data->mu_double_prime_data_count = 0;
    }
    if (data->alpha_double_prime_data) {
        g_free(data->alpha_double_prime_data);
        data->alpha_double_prime_data = NULL;
        data->alpha_double_prime_data_count = 0;
    }
    
    // 6. Сглаживание для μ'' (мю с погрешностью -E)
    if (data->smoothing_mu_double_prime_data) {
        for (int i = 0; i < data->smoothing_mu_double_prime_row_count; i++) {
            if (data->smoothing_mu_double_prime_data[i]) g_free(data->smoothing_mu_double_prime_data[i]);
        }
        g_free(data->smoothing_mu_double_prime_data);
        data->smoothing_mu_double_prime_data = NULL;
        data->smoothing_mu_double_prime_row_count = 0;
    }

    // 7. Сглаживание для α'' (альфа с погрешностью -E)
    if (data->smoothing_alpha_double_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; i++) {
            if (data->smoothing_alpha_double_prime_data[i]) g_free(data->smoothing_alpha_double_prime_data[i]);
        }
        g_free(data->smoothing_alpha_double_prime_data);
        data->smoothing_alpha_double_prime_data = NULL;
        data->smoothing_alpha_double_prime_row_count = 0;
    }
    
    debug_print_l1("level1_clear_internal_data: Очистка завершена.\n");
}

static void level1_on_window_destroy(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    debug_print_l1("level1_on_window_destroy: Окно уровня 1 закрывается (без X0)...\n");

    data->level1_window = NULL; 

    // Вкладка "Исходные данные"
    data->level1_tree_view = NULL; 
    if (data->level1_store) data->level1_store = NULL; // Модель управляется TreeView, просто обнуляем указатель
    
    data->level1_smoothing_view = NULL; 
    if (data->level1_smoothing_store) data->level1_smoothing_store = NULL;
    data->level1_drawing_area = NULL;
    
    data->level1_alpha_smoothing_view = NULL; 
    if (data->level1_alpha_smoothing_store) data->level1_alpha_smoothing_store = NULL;
    data->level1_alpha_drawing_area = NULL;

    // Вкладка "Данные +E"
    data->level1_error_adj_tree_view = NULL; 
    if (data->level1_error_adj_store) data->level1_error_adj_store = NULL;
    
    // Виджеты для сглаживания X0+E УДАЛЕНЫ из AppData
    
    data->level1_mu_prime_smoothing_view = NULL; 
    if (data->level1_mu_prime_smoothing_store) data->level1_mu_prime_smoothing_store = NULL;
    data->level1_mu_prime_drawing_area = NULL;
    
    data->level1_alpha_prime_smoothing_view = NULL; 
    if (data->level1_alpha_prime_smoothing_store) data->level1_alpha_prime_smoothing_store = NULL;
    data->level1_alpha_prime_drawing_area = NULL;

    // Вкладка "Данные -E"
    data->level1_error_sub_tree_view = NULL; 
    if (data->level1_error_sub_store) data->level1_error_sub_store = NULL;

    // Виджеты для сглаживания X0-E УДАЛЕНЫ из AppData

    data->level1_mu_double_prime_smoothing_view = NULL; 
    if (data->level1_mu_double_prime_smoothing_store) data->level1_mu_double_prime_smoothing_store = NULL;
    data->level1_mu_double_prime_drawing_area = NULL;
    
    data->level1_alpha_double_prime_smoothing_view = NULL; 
    if (data->level1_alpha_double_prime_smoothing_store) data->level1_alpha_double_prime_smoothing_store = NULL;
    data->level1_alpha_double_prime_drawing_area = NULL;

    // Очищаем все вычисленные массивы данных
    level1_clear_internal_data(data);

    debug_print_l1("level1_on_window_destroy: Обнуление указателей и очистка данных завершена.\n");
}

gboolean
on_inner_scroll_event (GtkEventController *controller_generic,
                       double             dx,
                       double             dy,
                       gpointer           user_data_tab_scroller) // Теперь это GtkScrolledWindow вкладки
{
    // AppData *data = user_data; // data больше не передается напрямую, если не нужна здесь
    GtkAdjustment *tab_vadjustment;
    double current_value;
    double new_value;
    double step_increment;
    GtkWidget *scrolled_window_widget_source = gtk_event_controller_get_widget(controller_generic);
    GtkScrolledWindow *tab_scroller = GTK_SCROLLED_WINDOW(user_data_tab_scroller); // Скроллер текущей вкладки

    if (!tab_scroller || !GTK_IS_SCROLLED_WINDOW(scrolled_window_widget_source) ||
        !gtk_widget_get_ancestor(scrolled_window_widget_source, GTK_TYPE_WINDOW)) {
        return FALSE;
    }
    
    if (fabs(dy) > 0.00001) {
        GtkPolicyType vpolicy, hpolicy;
        gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(scrolled_window_widget_source), &hpolicy, &vpolicy);

        if (vpolicy == GTK_POLICY_NEVER) { // Только если у текущего дочернего скроллера нет своей верт. прокрутки
            tab_vadjustment = gtk_scrolled_window_get_vadjustment(tab_scroller);
            if (!tab_vadjustment) {
                return FALSE;
            }
            current_value = gtk_adjustment_get_value(tab_vadjustment);
            step_increment = gtk_adjustment_get_step_increment(tab_vadjustment);
            if (step_increment < 1.0) step_increment = 30.0; 
            new_value = current_value + (dy * step_increment);
            double lower = gtk_adjustment_get_lower(tab_vadjustment);
            double upper = gtk_adjustment_get_upper(tab_vadjustment) - gtk_adjustment_get_page_size(tab_vadjustment);
            if (new_value < lower) new_value = lower;
            if (new_value > upper) new_value = upper;
            if (fabs(new_value - current_value) > 0.00001) {
                 gtk_adjustment_set_value(tab_vadjustment, new_value);
            }
            return TRUE; 
        }
    }
    return FALSE; 
}


void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    GraphDataType graph_type = GRAPH_DATA_TYPE_UNKNOWN;

    gpointer type_ptr = g_object_get_data(G_OBJECT(drawing_area), "graph_type_enum_key");
    if (type_ptr != NULL) {
        graph_type = (GraphDataType)GPOINTER_TO_INT(type_ptr);
    } else {
        // Аварийный вариант определения типа графика по указателю на drawing_area
        // (лучше всегда полагаться на "graph_type_enum_key")
        if (drawing_area == GTK_DRAWING_AREA(data->level1_drawing_area)) graph_type = GRAPH_DATA_TYPE_MU;
        else if (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_drawing_area)) graph_type = GRAPH_DATA_TYPE_ALPHA;
        // Удалены проверки для data->level1_error_adj_drawing_area и data->level1_error_sub_drawing_area,
        // так как они соответствовали сглаживанию X0, которое мы убрали.
        // Если для X+E и X-E будут основные графики (не сглаживания), им нужны свои drawing_area и типы.
        else if (drawing_area == GTK_DRAWING_AREA(data->level1_mu_prime_drawing_area)) graph_type = GRAPH_DATA_TYPE_MU_PRIME;
        else if (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_prime_drawing_area)) graph_type = GRAPH_DATA_TYPE_ALPHA_PRIME;
        else if (drawing_area == GTK_DRAWING_AREA(data->level1_mu_double_prime_drawing_area)) graph_type = GRAPH_DATA_TYPE_MU_DOUBLE_PRIME;
        else if (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_double_prime_drawing_area)) graph_type = GRAPH_DATA_TYPE_ALPHA_DOUBLE_PRIME;
    }

    double **graph_smoothing_data = NULL;
    int graph_smoothing_row_count = 0;
    double *real_plot_data = NULL;
    int real_plot_data_count = 0;
    const char *y_axis_label_str = "Значение";
    const char *title_str_graph = "График сглаживания";
    const char *real_data_legend_label = "Реальные данные";
    gboolean format_as_alpha = FALSE;
    GArray *temp_real_data_array = NULL; // Для временного хранения данных, если нужно их извлечь (например, для X0+E)

    switch (graph_type) {
        case GRAPH_DATA_TYPE_MU:
            graph_smoothing_data = data->smoothing_data;
            graph_smoothing_row_count = data->smoothing_row_count;
            if (data->mu_data && data->table_data_main) real_plot_data = data->mu_data;
            if (data->table_data_main) real_plot_data_count = data->table_data_main->row_count;
            y_axis_label_str = "Значение μ"; title_str_graph = "Прогнозные функции Zпр(μ)"; real_data_legend_label = "Реальные μ";
            break;
        case GRAPH_DATA_TYPE_ALPHA:
            graph_smoothing_data = data->smoothing_alpha_data;
            graph_smoothing_row_count = data->smoothing_alpha_row_count;
            if (data->alpha_data && data->alpha_count > 1) { real_plot_data = data->alpha_data + 1; real_plot_data_count = data->alpha_count - 1; }
            y_axis_label_str = "Значение α"; title_str_graph = "Прогнозные функции Zпр(α)"; real_data_legend_label = "Реальные α";
            format_as_alpha = TRUE;
            break;
        // GRAPH_DATA_TYPE_X0_PLUS_E УДАЛЕН
        case GRAPH_DATA_TYPE_MU_PRIME:
            graph_smoothing_data = data->smoothing_mu_prime_data;
            graph_smoothing_row_count = data->smoothing_mu_prime_row_count;
            if (data->mu_prime_data) { real_plot_data = data->mu_prime_data; real_plot_data_count = data->mu_prime_data_count; }
            y_axis_label_str = "Значение μ'"; title_str_graph = "Прогнозные функции Zпр(μ')"; real_data_legend_label = "Реальные μ'";
            break;
        case GRAPH_DATA_TYPE_ALPHA_PRIME:
            graph_smoothing_data = data->smoothing_alpha_prime_data;
            graph_smoothing_row_count = data->smoothing_alpha_prime_row_count;
            if (data->alpha_prime_data && data->alpha_prime_data_count > 1) { real_plot_data = data->alpha_prime_data + 1; real_plot_data_count = data->alpha_prime_data_count - 1; }
            y_axis_label_str = "Значение α'"; title_str_graph = "Прогнозные функции Zпр(α')"; real_data_legend_label = "Реальные α'";
            format_as_alpha = TRUE;
            break;
        // GRAPH_DATA_TYPE_X0_MINUS_E УДАЛЕН
        case GRAPH_DATA_TYPE_MU_DOUBLE_PRIME:
            graph_smoothing_data = data->smoothing_mu_double_prime_data;
            graph_smoothing_row_count = data->smoothing_mu_double_prime_row_count;
            if (data->mu_double_prime_data) { real_plot_data = data->mu_double_prime_data; real_plot_data_count = data->mu_double_prime_data_count; }
            y_axis_label_str = "Значение μ''"; title_str_graph = "Прогнозные функции Zпр(μ'')"; real_data_legend_label = "Реальные μ''";
            break;
        case GRAPH_DATA_TYPE_ALPHA_DOUBLE_PRIME:
            graph_smoothing_data = data->smoothing_alpha_double_prime_data;
            graph_smoothing_row_count = data->smoothing_alpha_double_prime_row_count;
            if (data->alpha_double_prime_data && data->alpha_double_prime_data_count > 1) { real_plot_data = data->alpha_double_prime_data + 1; real_plot_data_count = data->alpha_double_prime_data_count - 1; }
            y_axis_label_str = "Значение α''"; title_str_graph = "Прогнозные функции Zпр(α'')"; real_data_legend_label = "Реальные α''";
            format_as_alpha = TRUE;
            break;
        default:
            debug_print_l1("level1_draw_graph: Неизвестный GraphDataType (%d) для DrawingArea %p!\n", graph_type, (void*)drawing_area);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
            cairo_set_source_rgb(cr, 0.7, 0.0, 0.0); // Красный цвет для ошибки
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 14);
            cairo_text_extents_t extents; 
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg),"Ошибка: Неизвестный тип графика (%d)", graph_type);
            cairo_text_extents(cr, err_msg, &extents);
            cairo_move_to(cr, (width - extents.width) / 2.0, (height / 2.0) - (extents.height / 2.0) + extents.height);
            cairo_show_text(cr, err_msg);
            if (temp_real_data_array) g_array_free(temp_real_data_array, TRUE); // Хотя он теперь не должен использоваться
            return;
    }

    gboolean has_smoothing_data = graph_smoothing_data && graph_smoothing_row_count > 0;
    gboolean has_real_data = real_plot_data && real_plot_data_count > 0;

    if (!has_smoothing_data && !has_real_data) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t extents;
        char no_data_msg[128];
        snprintf(no_data_msg, sizeof(no_data_msg), "Нет данных для: %s", title_str_graph);
        cairo_text_extents(cr, no_data_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height / 2.0) - (extents.height / 2.0) + extents.height);
        cairo_show_text(cr, no_data_msg);
        // if (temp_real_data_array) g_array_free(temp_real_data_array, TRUE); // temp_real_data_array больше не используется здесь
        return;
    }

    double margin_left = 70.0;
    double margin_right = 20.0;
    double margin_top = 40.0;
    double margin_bottom = 90.0;
    double plot_width = width - margin_left - margin_right;
    double plot_height = height - margin_top - margin_bottom;

    if (plot_width <= 1 || plot_height <= 1) {
        if (temp_real_data_array) g_array_free(temp_real_data_array, TRUE);
        return;
    }

    double y_min_val = G_MAXDOUBLE, y_max_val = -G_MAXDOUBLE;
    gboolean data_found_for_yrange = FALSE;

    if(has_smoothing_data){
        for (int i = 0; i < graph_smoothing_row_count; i++) {
            if (graph_smoothing_data[i] == NULL) continue;
            for (int j_idx = 0; j_idx < 4; j_idx++) {
                if (isnan(graph_smoothing_data[i][j_idx]) || isinf(graph_smoothing_data[i][j_idx])) continue;
                if (graph_smoothing_data[i][j_idx] < y_min_val) y_min_val = graph_smoothing_data[i][j_idx];
                if (graph_smoothing_data[i][j_idx] > y_max_val) y_max_val = graph_smoothing_data[i][j_idx];
                data_found_for_yrange = TRUE;
            }
        }
    }
    if(has_real_data){
        for (int i = 0; i < real_plot_data_count; i++) {
            if (isnan(real_plot_data[i]) || isinf(real_plot_data[i])) continue;
            if (real_plot_data[i] < y_min_val) y_min_val = real_plot_data[i];
            if (real_plot_data[i] > y_max_val) y_max_val = real_plot_data[i];
            data_found_for_yrange = TRUE;
        }
    }

    if (!data_found_for_yrange) { y_min_val = -1.0; y_max_val = 1.0; }
    double y_range_val_calc = y_max_val - y_min_val;
    if (fabs(y_range_val_calc) < 1e-9) { 
        double abs_val = fabs(y_min_val); if (abs_val < 1e-9) abs_val = 1.0; 
        y_min_val -= 0.5 * abs_val; y_max_val += 0.5 * abs_val; 
        if (fabs(y_max_val - y_min_val) < 1e-9) { y_min_val = -1.0; y_max_val = 1.0; }
    } else { 
        y_min_val -= y_range_val_calc * 0.1; y_max_val += y_range_val_calc * 0.1; 
    }
    y_range_val_calc = y_max_val - y_min_val; 
    if (fabs(y_range_val_calc) < 1e-9) y_range_val_calc = 1.0;

    int x_num_points_axis = 1;
    int max_m_on_axis = 0;
    if (has_smoothing_data && graph_smoothing_row_count > 0) {
        max_m_on_axis = graph_smoothing_row_count - 1;
    }
    if (has_real_data && real_plot_data_count > 0) {
        if ((real_plot_data_count - 1) > max_m_on_axis) {
            max_m_on_axis = real_plot_data_count - 1;
        }
    }
    x_num_points_axis = max_m_on_axis + 1;
    if (x_num_points_axis <= 0) x_num_points_axis = 1;

    double x_max_ticks_value = max_m_on_axis;
    double x_scale = (x_max_ticks_value > 1e-9) ? (plot_width / x_max_ticks_value) : plot_width;
    if (x_num_points_axis == 1) x_scale = plot_width;
    double y_scale = plot_height / y_range_val_calc;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, margin_left, height - margin_bottom); cairo_line_to(cr, margin_left + plot_width, height - margin_bottom); cairo_stroke(cr);
    cairo_move_to(cr, margin_left, margin_top); cairo_line_to(cr, margin_left, margin_top + plot_height); cairo_stroke(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(cr, 10);
    for (int i = 0; i < x_num_points_axis; i++) { 
        int tick_frequency = 1; if (x_num_points_axis > 15) tick_frequency = (int)ceil((double)x_num_points_axis / 10.0); 
        if (i == 0 || i == (x_num_points_axis -1) || (i % tick_frequency == 0) || x_num_points_axis <= 5 ) { 
            double x_pos = margin_left + i * x_scale; 
            if (x_num_points_axis == 1) x_pos = margin_left + plot_width / 2.0;
            else if (i == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left;
            char label[16]; snprintf(label, sizeof(label), "%d", i); 
            cairo_text_extents_t extents; cairo_text_extents(cr, label, &extents); 
            cairo_move_to(cr, x_pos - extents.width / 2.0, height - margin_bottom + 15); cairo_show_text(cr, label); 
            cairo_move_to(cr, x_pos, height - margin_bottom); cairo_line_to(cr, x_pos, height - margin_bottom + 3); cairo_stroke(cr); 
        } 
    }
    cairo_text_extents_t x_label_extents_calc; cairo_text_extents(cr, "M (период)", &x_label_extents_calc); 
    cairo_move_to(cr, margin_left + plot_width / 2.0 - x_label_extents_calc.width / 2.0, height - margin_bottom + 35); cairo_show_text(cr, "M (период)");
    
    int num_y_ticks = 5; 
    for (int i = 0; i <= num_y_ticks; i++) { 
        double y_value = y_min_val + (y_range_val_calc * i / num_y_ticks); 
        double y_pos = height - margin_bottom - (y_value - y_min_val) * y_scale; 
        char label[32]; 
        snprintf(label, sizeof(label), format_as_alpha ? "%.1E" : "%.2f", y_value); 
        cairo_text_extents_t extents; cairo_text_extents(cr, label, &extents); 
        cairo_move_to(cr, margin_left - extents.width - 5, y_pos + extents.height / 2.0); cairo_show_text(cr, label); 
        cairo_move_to(cr, margin_left - 3, y_pos); cairo_line_to(cr, margin_left, y_pos); cairo_stroke(cr); 
    }
    cairo_text_extents_t y_label_extents_val; cairo_text_extents(cr, y_axis_label_str, &y_label_extents_val); 
    cairo_save(cr); cairo_move_to(cr, margin_left - 55, margin_top + plot_height / 2.0 + y_label_extents_val.width / 2.0); 
    cairo_rotate(cr, -M_PI / 2.0); cairo_show_text(cr, y_axis_label_str); cairo_restore(cr);

    double line_colors[5][3] = {{1.0,0.0,0.0},{0.0,0.7,0.0},{0.0,0.0,1.0},{0.8,0.6,0.0},{0.3,0.3,0.3}}; 
    const char *legend_labels_A[] = {"A=0.1","A=0.4","A=0.7","A=0.9"};

    if(has_smoothing_data){ 
        for (int j_color = 0; j_color < 4; j_color++) { 
            cairo_set_source_rgb(cr, line_colors[j_color][0], line_colors[j_color][1], line_colors[j_color][2]); 
            cairo_set_line_width(cr, 1.5); 
            gboolean first = TRUE; 
            for (int i = 0; i < graph_smoothing_row_count; i++) { 
                if (graph_smoothing_data[i] == NULL) {first=TRUE; continue;} 
                double val_y = graph_smoothing_data[i][j_color]; 
                if (isnan(val_y)||isinf(val_y)) {first=TRUE; continue;} 
                double x_pos = margin_left + i * x_scale; 
                if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2.0;
                else if (i == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left;
                double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; 
                if(y_pos < margin_top) y_pos=margin_top; if(y_pos > height-margin_bottom) y_pos=height-margin_bottom; 
                if(first){cairo_move_to(cr,x_pos,y_pos);first=FALSE;}else{cairo_line_to(cr,x_pos,y_pos);}
            } 
            if(!first)cairo_stroke(cr); 
            for (int i = 0; i < graph_smoothing_row_count; i++) { 
                if (graph_smoothing_data[i] == NULL) continue; 
                double val_y = graph_smoothing_data[i][j_color]; 
                if(isnan(val_y)||isinf(val_y))continue; 
                double x_pos = margin_left + i * x_scale; 
                if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2.0;
                else if (i == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left;
                double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; 
                if(y_pos<margin_top-3||y_pos>height-margin_bottom+3)continue; 
                cairo_arc(cr,x_pos,y_pos,2.2,0,2*M_PI);cairo_fill(cr);
            }
        }
    }
    if(has_real_data){ 
        int color_idx = 4; 
        cairo_set_source_rgb(cr, line_colors[color_idx][0], line_colors[color_idx][1], line_colors[color_idx][2]); 
        cairo_set_line_width(cr, 2.0); 
        gboolean first = TRUE; 
        for(int i = 0; i < real_plot_data_count; ++i){ 
            int m_val = i; 
            double val_y = real_plot_data[i]; 
            if(isnan(val_y)||isinf(val_y)){first=TRUE;continue;} 
            double x_pos = margin_left + m_val * x_scale; 
            if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2.0;
            else if (m_val == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left;
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; 
            if(y_pos < margin_top) y_pos=margin_top; if(y_pos > height-margin_bottom) y_pos=height-margin_bottom; 
            if(first){cairo_move_to(cr,x_pos,y_pos);first=FALSE;}else{cairo_line_to(cr,x_pos,y_pos);}
        } 
        if(!first)cairo_stroke(cr); 
        for(int i = 0; i < real_plot_data_count; ++i){ 
            int m_val = i; 
            double val_y = real_plot_data[i]; 
            if(isnan(val_y)||isinf(val_y))continue; 
            double x_pos = margin_left + m_val * x_scale; 
            if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2.0;
            else if (m_val == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left;
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; 
            if(y_pos<margin_top-3||y_pos>height-margin_bottom+3)continue; 
            cairo_arc(cr,x_pos,y_pos,2.8,0,2*M_PI);cairo_fill(cr);
        }
    }
    
    double legend_item_width = 110; 
    double legend_x_start = margin_left; 
    double legend_y_start = height - margin_bottom + 50; 
    double legend_x_current = legend_x_start; 
    double legend_y_current = legend_y_start; 
    cairo_set_font_size(cr, 10);
    for (int j = 0; j < 4; j++) { 
        if (!has_smoothing_data) continue;
        if (legend_x_current + legend_item_width > width - margin_right) { legend_y_current += 15; legend_x_current = legend_x_start; if (legend_x_current + legend_item_width > width - margin_right) break; } 
        cairo_set_source_rgb(cr, line_colors[j][0], line_colors[j][1], line_colors[j][2]); 
        cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10); cairo_fill(cr); 
        cairo_set_source_rgb(cr, 0, 0, 0); 
        cairo_text_extents_t text_ext_legend; cairo_text_extents(cr, legend_labels_A[j], &text_ext_legend); 
        cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1); cairo_show_text(cr, legend_labels_A[j]); 
        legend_x_current += legend_item_width; 
    }
    if (has_real_data) { 
        if (legend_x_current + legend_item_width > width - margin_right) { legend_y_current += 15; legend_x_current = legend_x_start; }
        if (legend_x_current + legend_item_width <= width - margin_right) {
            cairo_set_source_rgb(cr, line_colors[4][0], line_colors[4][1], line_colors[4][2]); 
            cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10); cairo_fill(cr); 
            cairo_set_source_rgb(cr, 0, 0, 0); 
            cairo_text_extents_t text_ext_legend; cairo_text_extents(cr, real_data_legend_label, &text_ext_legend); 
            cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1); cairo_show_text(cr, real_data_legend_label);
        }
    }

    cairo_set_font_size(cr, 14); 
    cairo_text_extents_t title_extents_val_graph; 
    cairo_text_extents(cr, title_str_graph, &title_extents_val_graph); 
    cairo_move_to(cr, margin_left + plot_width / 2.0 - title_extents_val_graph.width / 2.0, margin_top - 15); 
    cairo_show_text(cr, title_str_graph);

    if (temp_real_data_array) {
        g_array_free(temp_real_data_array, TRUE);
    }
}

void level1_update_content_if_visible(AppData *data) {
    // Проверяем, существует ли объект окна И видимо ли оно
    if (data && data->level1_window && gtk_widget_get_visible(data->level1_window)) {
        debug_print_l1("level1_update_content_if_visible: Окно L1 видимо, запускаем обновление...\n");
        level1_perform_calculations_and_update_ui(data); // Вызываем основную функцию обновления
    } else {
       // debug_print_l1("level1_update_content_if_visible: Окно L1 не существует или не видимо, обновление пропущено.\n");
    }
}

void level1_cleanup_calculated_data(AppData *data) {
    if (data) {
        debug_print_l1("level1_cleanup_calculated_data: Запрошена очистка данных Уровня 1.\n");
        level1_clear_internal_data(data); // Вызываем внутреннюю функцию очистки
    }
}

// --- Публичные функции ---

void level1_show_window(AppData *data) {
    debug_print_l1("level1_show_window: Запрошено окно Уровня 1 (с вкладкой Заключение).\n");

    if (!data->level1_window) {
        debug_print_l1("level1_show_window: Окно не существует, создаем новое...\n");
        data->level1_window = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(data->level1_window), "Уровень 1: Анализ данных");
        gtk_window_set_default_size(GTK_WINDOW(data->level1_window), 1250, 850); // Немного увеличил высоту для заключения
        if(data->parent) gtk_window_set_transient_for(GTK_WINDOW(data->level1_window), data->parent);
        g_signal_connect(data->level1_window, "destroy", G_CALLBACK(level1_on_window_destroy), data);
        
        GtkWidget *notebook = gtk_notebook_new();
        gtk_window_set_child(GTK_WINDOW(data->level1_window), notebook);

        // --- ВКЛАДКА 1: Исходные данные ---
        GtkWidget *page1_scrolled_window = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page1_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        GtkWidget *page1_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_margin_start(page1_vbox, 10); gtk_widget_set_margin_end(page1_vbox, 10);
        gtk_widget_set_margin_top(page1_vbox, 10); gtk_widget_set_margin_bottom(page1_vbox, 10);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page1_scrolled_window), page1_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page1_scrolled_window, gtk_label_new("Исходные данные"));

        GtkWidget *main_l1_table_label = gtk_label_new("<b>Данные основной таблицы (с μ и α)</b>");
        gtk_label_set_use_markup(GTK_LABEL(main_l1_table_label), TRUE); gtk_widget_set_halign(main_l1_table_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(page1_vbox), main_l1_table_label);
        GtkWidget *main_l1_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_l1_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(main_l1_table_scrolled, FALSE); gtk_widget_set_hexpand(main_l1_table_scrolled, TRUE);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(main_l1_table_scrolled), TRUE);
        data->level1_tree_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_tree_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_tree_view)) g_object_set_data(G_OBJECT(data->level1_tree_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_l1_table_scrolled), data->level1_tree_view);
        gtk_box_append(GTK_BOX(page1_vbox), main_l1_table_scrolled);
        GtkEventController *sc_main_l1 = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (sc_main_l1, "scroll", G_CALLBACK (on_inner_scroll_event), page1_scrolled_window);
        gtk_widget_add_controller(main_l1_table_scrolled, sc_main_l1);

        ui_template_create_smoothing_section(data, GTK_BOX(page1_vbox), "Анализ μ (мю)", GRAPH_DATA_TYPE_MU,
                                            &data->level1_smoothing_view, &data->level1_smoothing_store,
                                            &data->level1_drawing_area, page1_scrolled_window);
        ui_template_create_smoothing_section(data, GTK_BOX(page1_vbox), "Анализ α (альфа)", GRAPH_DATA_TYPE_ALPHA,
                                            &data->level1_alpha_smoothing_view, &data->level1_alpha_smoothing_store,
                                            &data->level1_alpha_drawing_area, page1_scrolled_window);

        // --- ВКЛАДКА 2: Данные +E ---
        GtkWidget *page2_scrolled_window = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page2_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        GtkWidget *page2_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_margin_start(page2_vbox, 10); gtk_widget_set_margin_end(page2_vbox, 10);
        gtk_widget_set_margin_top(page2_vbox, 10); gtk_widget_set_margin_bottom(page2_vbox, 10);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page2_scrolled_window), page2_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page2_scrolled_window, gtk_label_new("Данные +E"));

        GtkWidget *err_adj_table_label = gtk_label_new("<b>Основная таблица с учетом погрешности +E (X+E, μ', α')</b>");
        gtk_label_set_use_markup(GTK_LABEL(err_adj_table_label), TRUE); gtk_widget_set_halign(err_adj_table_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(page2_vbox), err_adj_table_label);
        GtkWidget *err_adj_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(err_adj_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(err_adj_table_scrolled, FALSE); gtk_widget_set_hexpand(err_adj_table_scrolled, TRUE);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(err_adj_table_scrolled), TRUE);
        data->level1_error_adj_tree_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_error_adj_tree_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_error_adj_tree_view)) g_object_set_data(G_OBJECT(data->level1_error_adj_tree_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(err_adj_table_scrolled), data->level1_error_adj_tree_view);
        gtk_box_append(GTK_BOX(page2_vbox), err_adj_table_scrolled);
        GtkEventController *sc_err_adj = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (sc_err_adj, "scroll", G_CALLBACK (on_inner_scroll_event), page2_scrolled_window);
        gtk_widget_add_controller(err_adj_table_scrolled, sc_err_adj);

        ui_template_create_smoothing_section(data, GTK_BOX(page2_vbox), "Анализ μ' (мю с погрешностью E)", GRAPH_DATA_TYPE_MU_PRIME,
                                            &data->level1_mu_prime_smoothing_view, &data->level1_mu_prime_smoothing_store,
                                            &data->level1_mu_prime_drawing_area, page2_scrolled_window);
        ui_template_create_smoothing_section(data, GTK_BOX(page2_vbox), "Анализ α' (альфа с погрешностью E)", GRAPH_DATA_TYPE_ALPHA_PRIME,
                                            &data->level1_alpha_prime_smoothing_view, &data->level1_alpha_prime_smoothing_store,
                                            &data->level1_alpha_prime_drawing_area, page2_scrolled_window);

        // --- ВКЛАДКА 3: Данные -E ---
        GtkWidget *page3_scrolled_window = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page3_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        GtkWidget *page3_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_margin_start(page3_vbox, 10); gtk_widget_set_margin_end(page3_vbox, 10);
        gtk_widget_set_margin_top(page3_vbox, 10); gtk_widget_set_margin_bottom(page3_vbox, 10);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page3_scrolled_window), page3_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page3_scrolled_window, gtk_label_new("Данные -E"));

        GtkWidget *error_sub_table_label = gtk_label_new("<b>Основная таблица с учетом погрешности -E (X-E, μ'', α'')</b>");
        gtk_label_set_use_markup(GTK_LABEL(error_sub_table_label), TRUE); gtk_widget_set_halign(error_sub_table_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(page3_vbox), error_sub_table_label);
        GtkWidget *error_sub_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(error_sub_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(error_sub_table_scrolled, FALSE); gtk_widget_set_hexpand(error_sub_table_scrolled, TRUE);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(error_sub_table_scrolled), TRUE);
        data->level1_error_sub_tree_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_error_sub_tree_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_error_sub_tree_view)) g_object_set_data(G_OBJECT(data->level1_error_sub_tree_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(error_sub_table_scrolled), data->level1_error_sub_tree_view);
        gtk_box_append(GTK_BOX(page3_vbox), error_sub_table_scrolled);
        GtkEventController *sc_err_sub = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (sc_err_sub, "scroll", G_CALLBACK (on_inner_scroll_event), page3_scrolled_window);
        gtk_widget_add_controller(error_sub_table_scrolled, sc_err_sub);
        
        ui_template_create_smoothing_section(data, GTK_BOX(page3_vbox), "Анализ μ'' (мю с погрешностью -E)", GRAPH_DATA_TYPE_MU_DOUBLE_PRIME,
                                            &data->level1_mu_double_prime_smoothing_view, &data->level1_mu_double_prime_smoothing_store,
                                            &data->level1_mu_double_prime_drawing_area, page3_scrolled_window);
        ui_template_create_smoothing_section(data, GTK_BOX(page3_vbox), "Анализ α'' (альфа с погрешностью -E)", GRAPH_DATA_TYPE_ALPHA_DOUBLE_PRIME,
                                            &data->level1_alpha_double_prime_smoothing_view, &data->level1_alpha_double_prime_smoothing_store,
                                            &data->level1_alpha_double_prime_drawing_area, page3_scrolled_window);

        // --- ВКЛАДКА 4: Заключение ---
        GtkWidget *page4_scrolled_window_container = gtk_scrolled_window_new(); // Это внешний скроллер для всей вкладки
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page4_scrolled_window_container), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        GtkWidget *page4_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_margin_start(page4_vbox, 10); gtk_widget_set_margin_end(page4_vbox, 10);
        gtk_widget_set_margin_top(page4_vbox, 10); gtk_widget_set_margin_bottom(page4_vbox, 10);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page4_scrolled_window_container), page4_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page4_scrolled_window_container, gtk_label_new("Заключение"));

        // Используем новый шаблон для секции α(μ)
        ui_template_create_conclusion_alpha_mu_section(data, GTK_BOX(page4_vbox), 
                                                   "Сводный анализ зависимости α от μ", 
                                                   CONCLUSION_GRAPH_TYPE_ALPHA_VS_MU, // Тип из ui_template_builders.h
                                                   &data->conclusion_alpha_mu_table_view,
                                                   &data->conclusion_alpha_mu_store,
                                                   &data->conclusion_alpha_mu_drawing_area,
                                                   page4_scrolled_window_container); // Передаем скроллер вкладки "Заключение"

        // --- Настройка колонок для ВСЕХ таблиц через update_tree_view_columns ---
        // Сигнатура: is_main_table, is_l1_main, is_l1_err_adj, is_l1_err_sub, 
        //            is_sm_mu, is_sm_alpha, is_sm_mu_p, is_sm_alpha_p, is_sm_mu_dp, is_sm_alpha_dp
        
        debug_print_l1("Настройка колонок для таблиц Уровня 1...\n");
        // 1. Основная таблица L1 (Исходные данные: X, μ, α)
        update_tree_view_columns(data, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
        // 2. Основная таблица L1 (Данные +E: X+E, μ', α')
        update_tree_view_columns(data, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
        // 3. Основная таблица L1 (Данные -E: X-E, μ'', α'')
        update_tree_view_columns(data, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
        
        // Таблицы сглаживания (6 штук)
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE); // SM_MU
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE); // SM_ALPHA
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE); // SM_MU_PRIME
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE); // SM_ALPHA_PRIME
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE); // SM_MU_DOUBLE_PRIME
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE);  // SM_ALPHA_DOUBLE_PRIME
        
        debug_print_l1("   Структура окна L1 создана, колонки настроены.\n");
        level1_perform_calculations_and_update_ui(data); // Первичное заполнение данными
    } else {
        debug_print_l1("level1_show_window: Окно уже существует. Обновление содержимого...\n");
        level1_perform_calculations_and_update_ui(data); 
    }
    gtk_window_present(GTK_WINDOW(data->level1_window));
}


static void level1_perform_calculations_and_update_ui(AppData *data) {
    debug_print_l1("level1_perform_calculations_and_update_ui: НАЧАЛО (полная версия).\n");
    
    level1_clear_internal_data(data); // Очищаем все предыдущие расчетные данные

    if (data->table_data_main == NULL || data->table_data_main->row_count == 0) {
        debug_print_l1("level1_perform_calculations_and_update_ui: НЕТ ДАННЫХ в table_data_main. Очистка UI.\n");
        // Очистка всех ListStore
        if (data->level1_store) gtk_list_store_clear(data->level1_store);
        if (data->level1_error_adj_store) gtk_list_store_clear(data->level1_error_adj_store);
        if (data->level1_error_sub_store) gtk_list_store_clear(data->level1_error_sub_store);
        if (data->conclusion_alpha_mu_store) gtk_list_store_clear(data->conclusion_alpha_mu_store);
        
        if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
        if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
        if (data->level1_mu_prime_smoothing_store) gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
        if (data->level1_alpha_prime_smoothing_store) gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
        if (data->level1_mu_double_prime_smoothing_store) gtk_list_store_clear(data->level1_mu_double_prime_smoothing_store);
        if (data->level1_alpha_double_prime_smoothing_store) gtk_list_store_clear(data->level1_alpha_double_prime_smoothing_store);
        
        // Перерисовка всех DrawingArea
        if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
        if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
        if (data->level1_mu_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_prime_drawing_area);
        if (data->level1_alpha_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_prime_drawing_area);
        if (data->level1_mu_double_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_double_prime_drawing_area);
        if (data->level1_alpha_double_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_double_prime_drawing_area);
        if (data->conclusion_alpha_mu_drawing_area) gtk_widget_queue_draw(data->conclusion_alpha_mu_drawing_area);
        return;
    }

    int main_row_count = data->table_data_main->row_count;
    int main_col_count = data->table_data_main->column_count;
    double coefficients[] = {0.1, 0.4, 0.7, 0.9};
    int coeff_count = sizeof(coefficients) / sizeof(coefficients[0]);

    // ==========================================================================
    // == БЛОК 1: ИСХОДНЫЕ ДАННЫЕ (Вкладка 1) ===================================
    // ==========================================================================
    debug_print_l1("БЛОК 1: Расчеты для исходных данных...\n");

    data->mu_data = compute_mu(data->table_data_main->table, main_row_count, main_col_count);
    if (data->mu_data) {
        data->alpha_data = compute_alpha(data->table_data_main->table, main_row_count, main_col_count, data->mu_data);
        data->alpha_count = data->alpha_data ? main_row_count : 0;
    } else { 
        data->alpha_data = NULL; data->alpha_count = 0;
        debug_print_l1(" ! Ошибка вычисления mu_data для основной таблицы, alpha не будет вычислено.\n");
    }

    if (data->level1_store && data->level1_tree_view) { // Предполагаем, что store уже создан и колонки настроены
        gtk_list_store_clear(data->level1_store);
        for (int i = 0; i < main_row_count; i++) {
            GtkTreeIter iter; gtk_list_store_append(data->level1_store, &iter);
            gtk_list_store_set(data->level1_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
            if (data->table_data_main->table && data->table_data_main->table[i]) {
                for (int j = 0; j < main_col_count; j++) {
                    gtk_list_store_set(data->level1_store, &iter, j + 1, data->table_data_main->table[i][j], -1);
                }
            } else { for (int j = 0; j < main_col_count; j++) gtk_list_store_set(data->level1_store, &iter, j + 1, NAN, -1); }
            gtk_list_store_set(data->level1_store, &iter, main_col_count + 1, (data->mu_data && i < main_row_count) ? data->mu_data[i] : NAN, -1);
            gtk_list_store_set(data->level1_store, &iter, main_col_count + 2, (data->alpha_data && i < data->alpha_count) ? data->alpha_data[i] : NAN, -1);
        }
        debug_print_l1("   Таблица L1 (исходные, mu, alpha) заполнена: %d строк.\n", main_row_count);
    }

    if (data->level1_smoothing_store && data->mu_data && main_row_count > 0) {
        data->smoothing_data = compute_smoothing(data->mu_data, main_row_count, &data->smoothing_row_count, coefficients, coeff_count);
        if (data->smoothing_data && data->smoothing_row_count > 0) {
            gtk_list_store_clear(data->level1_smoothing_store);
            for (int i = 0; i < data->smoothing_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_smoothing_store, &iter);
                gtk_list_store_set(data->level1_smoothing_store, &iter, 0, i,
                                  1, (data->smoothing_data[i]) ? data->smoothing_data[i][0] : NAN,
                                  2, (data->smoothing_data[i]) ? data->smoothing_data[i][1] : NAN,
                                  3, (data->smoothing_data[i]) ? data->smoothing_data[i][2] : NAN,
                                  4, (data->smoothing_data[i]) ? data->smoothing_data[i][3] : NAN,
                                  5, (i < main_row_count && data->mu_data) ? data->mu_data[i] : NAN, -1);
            }
            debug_print_l1("   Таблица сглаживания mu заполнена: %d строк.\n", data->smoothing_row_count);
        } else if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
    }
    
    if (data->level1_alpha_smoothing_store && data->alpha_data && data->alpha_count > 1) {
        data->smoothing_alpha_data = compute_smoothing(data->alpha_data + 1, data->alpha_count - 1, &data->smoothing_alpha_row_count, coefficients, coeff_count);
        if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
            gtk_list_store_clear(data->level1_alpha_smoothing_store);
            for (int i = 0; i < data->smoothing_alpha_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_alpha_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_smoothing_store, &iter, 0, i,
                                  1, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][0] : NAN,
                                  2, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][1] : NAN,
                                  3, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][2] : NAN,
                                  4, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][3] : NAN,
                                  5, ((i + 1) < data->alpha_count && data->alpha_data) ? data->alpha_data[i + 1] : NAN, -1);
            }
            debug_print_l1("   Таблица сглаживания alpha заполнена: %d строк.\n", data->smoothing_alpha_row_count);
        } else if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
    }

    // ==========================================================================
    // == БЛОК 2: ДАННЫЕ +E (Вкладка 2) ========================================
    // ==========================================================================
    debug_print_l1("БЛОК 2: Расчеты для данных +E...\n");
    if (data->table_data_values && data->table_data_values->table && data->table_data_values->row_count > 0 && data->table_data_values->column_names && data->table_data_main && data->table_data_main->table) {
        int e_col_idx = -1; 
        for (int k = 0; k < data->table_data_values->column_count; ++k) {
            if (data->table_data_values->column_names[k] && strcmp(data->table_data_values->column_names[k], "E") == 0) { e_col_idx = k; break; }
        }
        if (e_col_idx != -1) {
            data->error_adjusted_table_data = g_new0(double*, main_row_count);
            for (int i = 0; i < main_row_count; ++i) {
                if (data->table_data_main->table[i]) {
                    data->error_adjusted_table_data[i] = g_new0(double, main_col_count > 0 ? main_col_count : 1);
                    double error_e_value = 0.0; 
                    if (i < data->table_data_values->row_count && data->table_data_values->table[i]) { 
                        error_e_value = data->table_data_values->table[i][e_col_idx]; 
                        if (isnan(error_e_value)) error_e_value = 0.0; 
                    } else if (data->table_data_values->row_count > 0 && data->table_data_values->table[0]){ 
                        error_e_value = data->table_data_values->table[0][e_col_idx]; 
                        if (isnan(error_e_value)) error_e_value = 0.0;
                    }
                    for (int j = 0; j < main_col_count; ++j) data->error_adjusted_table_data[i][j] = data->table_data_main->table[i][j] + error_e_value;
                } else data->error_adjusted_table_data[i] = NULL;
            }
             debug_print_l1("   Данные X+E вычислены.\n");
        } else debug_print_l1(" ! Колонка 'E' не найдена для X+E.\n");
    } else debug_print_l1(" ! Недостаточно данных для X+E (table_data_values или main).\n");

    if (data->error_adjusted_table_data && main_row_count > 0) {
        data->mu_prime_data = compute_mu(data->error_adjusted_table_data, main_row_count, main_col_count); 
        data->mu_prime_data_count = data->mu_prime_data ? main_row_count : 0;
        if (data->mu_prime_data) { 
            data->alpha_prime_data = compute_alpha(data->error_adjusted_table_data, main_row_count, main_col_count, data->mu_prime_data); 
            data->alpha_prime_data_count = data->alpha_prime_data ? main_row_count : 0; 
        } else { data->alpha_prime_data = NULL; data->alpha_prime_data_count = 0; debug_print_l1(" ! Ошибка вычисления mu_prime_data.\n"); }
    } else { data->mu_prime_data = NULL; data->mu_prime_data_count = 0; data->alpha_prime_data = NULL; data->alpha_prime_data_count = 0; debug_print_l1(" ! error_adjusted_table_data не вычислены.\n");}

    if (data->level1_error_adj_store && data->level1_error_adj_tree_view) { // Предполагаем, что store уже создан и колонки настроены
        gtk_list_store_clear(data->level1_error_adj_store);
        if (data->table_data_main && main_row_count > 0) { 
            for (int i = 0; i < main_row_count; ++i) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_error_adj_store, &iter);
                gtk_list_store_set(data->level1_error_adj_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
                if (data->error_adjusted_table_data && i < main_row_count && data->error_adjusted_table_data[i]) { 
                    for (int j = 0; j < main_col_count; ++j) gtk_list_store_set(data->level1_error_adj_store, &iter, j + 1, data->error_adjusted_table_data[i][j], -1); 
                } else { for (int j = 0; j < main_col_count; ++j) gtk_list_store_set(data->level1_error_adj_store, &iter, j + 1, NAN, -1); }
                gtk_list_store_set(data->level1_error_adj_store, &iter, main_col_count + 1, (data->mu_prime_data && i < data->mu_prime_data_count) ? data->mu_prime_data[i] : NAN, -1);
                gtk_list_store_set(data->level1_error_adj_store, &iter, main_col_count + 2, (data->alpha_prime_data && i < data->alpha_prime_data_count) ? data->alpha_prime_data[i] : NAN, -1);
            }
            debug_print_l1("   Таблица L1 (X+E, mu', alpha') заполнена: %d строк.\n", main_row_count);
        }
    }

    if (data->level1_mu_prime_smoothing_store && data->mu_prime_data && data->mu_prime_data_count > 0) {
        data->smoothing_mu_prime_data = compute_smoothing(data->mu_prime_data, data->mu_prime_data_count, &data->smoothing_mu_prime_row_count, coefficients, coeff_count);
        if (data->smoothing_mu_prime_data && data->smoothing_mu_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_mu_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_mu_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_mu_prime_smoothing_store, &iter, 0, i, 
                                  1, (data->smoothing_mu_prime_data[i])?data->smoothing_mu_prime_data[i][0]:NAN, 
                                  2, (data->smoothing_mu_prime_data[i])?data->smoothing_mu_prime_data[i][1]:NAN, 
                                  3, (data->smoothing_mu_prime_data[i])?data->smoothing_mu_prime_data[i][2]:NAN, 
                                  4, (data->smoothing_mu_prime_data[i])?data->smoothing_mu_prime_data[i][3]:NAN, 
                                  5, (i < data->mu_prime_data_count && data->mu_prime_data)?data->mu_prime_data[i]:NAN, -1);
            }
             debug_print_l1("   Таблица сглаживания mu' заполнена: %d строк.\n", data->smoothing_mu_prime_row_count);
        } else if (data->level1_mu_prime_smoothing_store) gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
    }

    if (data->level1_alpha_prime_smoothing_store && data->alpha_prime_data && data->alpha_prime_data_count > 1) {
        data->smoothing_alpha_prime_data = compute_smoothing(data->alpha_prime_data + 1, data->alpha_prime_data_count - 1, &data->smoothing_alpha_prime_row_count, coefficients, coeff_count);
        if (data->smoothing_alpha_prime_data && data->smoothing_alpha_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_alpha_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_alpha_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_prime_smoothing_store, &iter, 0, i, 
                                  1, (data->smoothing_alpha_prime_data[i])?data->smoothing_alpha_prime_data[i][0]:NAN, 
                                  2, (data->smoothing_alpha_prime_data[i])?data->smoothing_alpha_prime_data[i][1]:NAN, 
                                  3, (data->smoothing_alpha_prime_data[i])?data->smoothing_alpha_prime_data[i][2]:NAN, 
                                  4, (data->smoothing_alpha_prime_data[i])?data->smoothing_alpha_prime_data[i][3]:NAN, 
                                  5, ((i+1) < data->alpha_prime_data_count && data->alpha_prime_data)?data->alpha_prime_data[i+1]:NAN, -1);
            }
            debug_print_l1("   Таблица сглаживания alpha' заполнена: %d строк.\n", data->smoothing_alpha_prime_row_count);
        } else if (data->level1_alpha_prime_smoothing_store) gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
    }


    // ==========================================================================
    // == БЛОК 3: ДАННЫЕ -E (Вкладка 3) ========================================
    // ==========================================================================
    debug_print_l1("БЛОК 3: Расчеты для данных -E...\n");
    if (data->table_data_values && data->table_data_values->table && data->table_data_values->row_count > 0 && data->table_data_values->column_names && data->table_data_main && data->table_data_main->table) {
        int e_col_idx = -1; 
        for (int k = 0; k < data->table_data_values->column_count; ++k) {
            if (data->table_data_values->column_names[k] && strcmp(data->table_data_values->column_names[k], "E") == 0) { e_col_idx = k; break; }
        }
        if (e_col_idx != -1) {
            data->error_subtracted_table_data = g_new0(double*, main_row_count);
            for (int i = 0; i < main_row_count; ++i) {
                if (data->table_data_main->table[i]) {
                    data->error_subtracted_table_data[i] = g_new0(double, main_col_count > 0 ? main_col_count : 1);
                    double error_e_value = 0.0; 
                    if (i < data->table_data_values->row_count && data->table_data_values->table[i]) { 
                        error_e_value = data->table_data_values->table[i][e_col_idx]; 
                        if (isnan(error_e_value)) error_e_value = 0.0; 
                    } else if (data->table_data_values->row_count > 0 && data->table_data_values->table[0]){ 
                        error_e_value = data->table_data_values->table[0][e_col_idx]; 
                        if (isnan(error_e_value)) error_e_value = 0.0;
                    }
                    for (int j = 0; j < main_col_count; ++j) data->error_subtracted_table_data[i][j] = data->table_data_main->table[i][j] - error_e_value; // ВЫЧИТАЕМ E
                } else data->error_subtracted_table_data[i] = NULL;
            }
             debug_print_l1("   Данные X-E вычислены.\n");
        } else debug_print_l1(" ! Колонка 'E' не найдена для X-E.\n");
    } else debug_print_l1(" ! Недостаточно данных для X-E (table_data_values или main).\n");
    
    if (data->error_subtracted_table_data && main_row_count > 0) {
        data->mu_double_prime_data = compute_mu(data->error_subtracted_table_data, main_row_count, main_col_count); 
        data->mu_double_prime_data_count = data->mu_double_prime_data ? main_row_count : 0;
        if (data->mu_double_prime_data) { 
            data->alpha_double_prime_data = compute_alpha(data->error_subtracted_table_data, main_row_count, main_col_count, data->mu_double_prime_data); 
            data->alpha_double_prime_data_count = data->alpha_double_prime_data ? main_row_count : 0;
        } else { 
            data->alpha_double_prime_data = NULL; data->alpha_double_prime_data_count = 0;
            debug_print_l1(" ! Ошибка вычисления mu_double_prime_data.\n");
        }
    } else { 
        data->mu_double_prime_data = NULL; data->mu_double_prime_data_count = 0; 
        data->alpha_double_prime_data = NULL; data->alpha_double_prime_data_count = 0;
        debug_print_l1(" ! error_subtracted_table_data не вычислены.\n");
    }

    if (data->level1_error_sub_store && data->level1_error_sub_tree_view) { // Предполагаем, что store уже создан и колонки настроены
        gtk_list_store_clear(data->level1_error_sub_store);
        if (data->table_data_main && main_row_count > 0) {
            for (int i = 0; i < main_row_count; ++i) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_error_sub_store, &iter);
                gtk_list_store_set(data->level1_error_sub_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
                if (data->error_subtracted_table_data && i < main_row_count && data->error_subtracted_table_data[i]) { 
                    for (int j = 0; j < main_col_count; ++j) gtk_list_store_set(data->level1_error_sub_store, &iter, j + 1, data->error_subtracted_table_data[i][j], -1); 
                } else { 
                    for (int j = 0; j < main_col_count; ++j) gtk_list_store_set(data->level1_error_sub_store, &iter, j + 1, NAN, -1); 
                }
                gtk_list_store_set(data->level1_error_sub_store, &iter, main_col_count + 1, (data->mu_double_prime_data && i < data->mu_double_prime_data_count) ? data->mu_double_prime_data[i] : NAN, -1);
                gtk_list_store_set(data->level1_error_sub_store, &iter, main_col_count + 2, (data->alpha_double_prime_data && i < data->alpha_double_prime_data_count) ? data->alpha_double_prime_data[i] : NAN, -1);
            }
            debug_print_l1("   Таблица L1 (X-E, mu'', alpha'') заполнена: %d строк.\n", main_row_count);
        }
    }

    if (data->level1_mu_double_prime_smoothing_store && data->mu_double_prime_data && data->mu_double_prime_data_count > 0) {
        data->smoothing_mu_double_prime_data = compute_smoothing(data->mu_double_prime_data, data->mu_double_prime_data_count, &data->smoothing_mu_double_prime_row_count, coefficients, coeff_count);
        if (data->smoothing_mu_double_prime_data && data->smoothing_mu_double_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_mu_double_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_mu_double_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_mu_double_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_mu_double_prime_smoothing_store, &iter, 0, i, 
                                  1, (data->smoothing_mu_double_prime_data[i])?data->smoothing_mu_double_prime_data[i][0]:NAN, 
                                  2, (data->smoothing_mu_double_prime_data[i])?data->smoothing_mu_double_prime_data[i][1]:NAN, 
                                  3, (data->smoothing_mu_double_prime_data[i])?data->smoothing_mu_double_prime_data[i][2]:NAN, 
                                  4, (data->smoothing_mu_double_prime_data[i])?data->smoothing_mu_double_prime_data[i][3]:NAN, 
                                  5, (i < data->mu_double_prime_data_count && data->mu_double_prime_data)?data->mu_double_prime_data[i]:NAN, -1);
            }
            debug_print_l1("   Таблица сглаживания mu'' заполнена: %d строк.\n", data->smoothing_mu_double_prime_row_count);
        } else if (data->level1_mu_double_prime_smoothing_store) gtk_list_store_clear(data->level1_mu_double_prime_smoothing_store);
    }

    if (data->level1_alpha_double_prime_smoothing_store && data->alpha_double_prime_data && data->alpha_double_prime_data_count > 1) {
        data->smoothing_alpha_double_prime_data = compute_smoothing(data->alpha_double_prime_data + 1, data->alpha_double_prime_data_count - 1, &data->smoothing_alpha_double_prime_row_count, coefficients, coeff_count);
        if (data->smoothing_alpha_double_prime_data && data->smoothing_alpha_double_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_alpha_double_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_alpha_double_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_alpha_double_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_double_prime_smoothing_store, &iter, 0, i, 
                                  1, (data->smoothing_alpha_double_prime_data[i])?data->smoothing_alpha_double_prime_data[i][0]:NAN, 
                                  2, (data->smoothing_alpha_double_prime_data[i])?data->smoothing_alpha_double_prime_data[i][1]:NAN, 
                                  3, (data->smoothing_alpha_double_prime_data[i])?data->smoothing_alpha_double_prime_data[i][2]:NAN, 
                                  4, (data->smoothing_alpha_double_prime_data[i])?data->smoothing_alpha_double_prime_data[i][3]:NAN, 
                                  5, ((i+1) < data->alpha_double_prime_data_count && data->alpha_double_prime_data)?data->alpha_double_prime_data[i+1]:NAN, -1);
            }
            debug_print_l1("   Таблица сглаживания alpha'' заполнена: %d строк.\n", data->smoothing_alpha_double_prime_row_count);
        } else if (data->level1_alpha_double_prime_smoothing_store) gtk_list_store_clear(data->level1_alpha_double_prime_smoothing_store);
    }

    // ==========================================================================
    // == БЛОК 4: ВКЛАДКА ЗАКЛЮЧЕНИЕ (таблица α(μ)) ============================
    // ==========================================================================

    // ВНИМАНИЕ БЛЯТЬ! ЭТОТ БЛОК КОДА ЛОМАЕТ ПРОГРАММУ К ХЕРАМ! ПРИЧИНА: ПОВРЕЖДЕНИЕ ПАМЯТИ!


    debug_print_l1("БЛОК 4: Формирование данных для вкладки Заключение α(μ)...\n");
    
    debug_print_l1("   Шаг 4.0: data->conclusion_alpha_mu_table_view = %p\n", 
                   (void*)data->conclusion_alpha_mu_table_view);

    if (data->conclusion_alpha_mu_table_view) { // Проверяем, что указатель на TreeView существует
        if (!GTK_IS_TREE_VIEW(data->conclusion_alpha_mu_table_view)) {
            debug_print_l1("   Шаг 4.1: КРИТИЧЕСКАЯ ОШИБКА: data->conclusion_alpha_mu_table_view (%p) НЕ ЯВЛЯЕТСЯ GtkTreeView!\n", 
                           (void*)data->conclusion_alpha_mu_table_view);
            // Если это не TreeView, дальнейшие операции с ним приведут к падению.
            // Пропускаем всю логику для этой таблицы.
        } else { // data->conclusion_alpha_mu_table_view является валидным GtkTreeView
            debug_print_l1("   Шаг 4.1: data->conclusion_alpha_mu_table_view является GtkTreeView.\n");

            // Создаем ListStore и колонки только если store еще не существует
            if (!data->conclusion_alpha_mu_store) { 
                debug_print_l1("   Шаг 4.2: Начало создания GtkListStore для таблицы Заключения α(μ).\n");
                
                GType types_for_conclusion[4]; 
                types_for_conclusion[0] = G_TYPE_STRING; // Источник
                types_for_conclusion[1] = G_TYPE_INT;    // Эпоха
                types_for_conclusion[2] = G_TYPE_DOUBLE; // mu
                types_for_conclusion[3] = G_TYPE_DOUBLE; // alpha
                debug_print_l1("   Шаг 4.2.1: Массив GType types_for_conclusion инициализирован. ...\n");

                debug_print_l1("   ПРОВЕРКА: Попытка создать ОЧЕНЬ простой store...\n");
                GType simple_type[] = {G_TYPE_STRING};
                GtkListStore *test_store = gtk_list_store_new(1, simple_type);
                debug_print_l1("   ПРОВЕРКА: test_store создан: %p\n", (void*)test_store);
                if (test_store) {
                    g_object_unref(test_store); // Не забываем освободить, если создался
                    debug_print_l1("   ПРОВЕРКА: test_store освобожден.\n");
                } else {
                    debug_print_l1("   ПРОВЕРКА: ОШИБКА создания test_store!\n");
                }
                debug_print_l1("   ПРОВЕРКА: Завершена.\n");

                debug_print_l1("   Шаг 4.2.2: Вызов gtk_list_store_new(4, types_for_conclusion)...\n");
                data->conclusion_alpha_mu_store = gtk_list_store_new(4, types_for_conclusion); 
                debug_print_l1("   Шаг 4.2.3: gtk_list_store_new ВЫПОЛНЕН.\n");
            
                debug_print_l1("   Шаг 4.3: data->conclusion_alpha_mu_store после создания: %p\n", (void*)data->conclusion_alpha_mu_store);
                if (!data->conclusion_alpha_mu_store) {
                    debug_print_l1("   Шаг 4.3.1: КРИТИЧЕСКАЯ ОШИБКА: gtk_list_store_new вернул NULL для conclusion_alpha_mu_store!\n");
                    // Нет смысла продолжать, если store не создан
                } else {
                    // Store успешно создан, теперь устанавливаем модель и создаем колонки
                    debug_print_l1("   Шаг 4.4: Попытка gtk_tree_view_set_model...\n");
                    gtk_tree_view_set_model(GTK_TREE_VIEW(data->conclusion_alpha_mu_table_view), 
                                            GTK_TREE_MODEL(data->conclusion_alpha_mu_store));
                    debug_print_l1("   Шаг 4.5: gtk_tree_view_set_model ВЫПОЛНЕН.\n");

                    GtkCellRenderer *renderer; GtkTreeViewColumn *col;
                    
                    debug_print_l1("   Шаг 4.6: Создание колонки 'Источник'...\n");
                    renderer = gtk_cell_renderer_text_new();
                    col = gtk_tree_view_column_new_with_attributes("Источник", renderer, "text", 0, NULL);
                    gtk_tree_view_column_set_min_width(col, 110); 
                    // gtk_tree_view_column_set_sort_column_id(col, 0); // Закомментировано для отладки
                    gtk_tree_view_append_column(GTK_TREE_VIEW(data->conclusion_alpha_mu_table_view), col);
                    debug_print_l1("   Шаг 4.6.1: Колонка 'Источник' создана.\n");
                    
                    debug_print_l1("   Шаг 4.7: Создание колонки 'Эпоха'...\n");
                    renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
                    col = gtk_tree_view_column_new_with_attributes("Эпоха", renderer, "text", 1, NULL);
                    gtk_tree_view_column_set_fixed_width(col, 70); 
                    // gtk_tree_view_column_set_sort_column_id(col, 1); // Закомментировано для отладки
                    gtk_tree_view_append_column(GTK_TREE_VIEW(data->conclusion_alpha_mu_table_view), col);
                    debug_print_l1("   Шаг 4.7.1: Колонка 'Эпоха' создана.\n");
                    
                    debug_print_l1("   Шаг 4.8: Создание колонки 'μ'...\n");
                    renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
                    col = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col, "μ");
                    gtk_tree_view_column_pack_start(col, renderer, TRUE);
                    gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(2), NULL);
                    g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1)); 
                    gtk_tree_view_column_set_fixed_width(col, 100); 
                    // gtk_tree_view_column_set_sort_column_id(col, 2); // Закомментировано для отладки
                    gtk_tree_view_append_column(GTK_TREE_VIEW(data->conclusion_alpha_mu_table_view), col);
                    debug_print_l1("   Шаг 4.8.1: Колонка 'μ' создана.\n");
                    
                    debug_print_l1("   Шаг 4.9: Создание колонки 'α'...\n");
                    renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
                    col = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col, "α");
                    gtk_tree_view_column_pack_start(col, renderer, TRUE);
                    gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(3), NULL);
                    g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1)); 
                    gtk_tree_view_column_set_fixed_width(col, 120); 
                    // gtk_tree_view_column_set_sort_column_id(col, 3); // Закомментировано для отладки
                    gtk_tree_view_append_column(GTK_TREE_VIEW(data->conclusion_alpha_mu_table_view), col);
                    debug_print_l1("   Шаг 4.9.1: Колонка 'α' создана. Все колонки для Заключения созданы.\n");
                } // Конец else: Store был успешно создан
            } // Конец if (!data->conclusion_alpha_mu_store): Создание store и колонок

            // Заполнение store (выполняется только если store существует после предыдущего блока)
            if (data->conclusion_alpha_mu_store) { 
                debug_print_l1("   Шаг 4.10: Заполнение data->conclusion_alpha_mu_store...\n");
                gtk_list_store_clear(data->conclusion_alpha_mu_store);
                GtkTreeIter iter; 
                int current_epoch;

                // 1. Исходные данные
                if (data->mu_data && data->alpha_data && data->table_data_main && data->table_data_main->row_count > 0) {
                    int count_to_iterate = MIN(main_row_count, data->alpha_count); // Безопасное количество итераций
                    for (int i = 0; i < count_to_iterate; ++i) {
                        current_epoch = data->table_data_main->epochs ? data->table_data_main->epochs[i] : (i + 1);
                        gtk_list_store_append(data->conclusion_alpha_mu_store, &iter);
                        gtk_list_store_set(data->conclusion_alpha_mu_store, &iter, 
                                           0, "Исходные", 
                                           1, current_epoch, 
                                           2, data->mu_data[i], 
                                           3, data->alpha_data[i], 
                                           -1);
                    }
                }
                // 2. Данные +E
                if (data->mu_prime_data && data->alpha_prime_data && data->table_data_main && data->table_data_main->row_count > 0) {
                    int count_to_iterate = MIN(main_row_count, MIN(data->mu_prime_data_count, data->alpha_prime_data_count));
                     for (int i = 0; i < count_to_iterate; ++i) {
                        current_epoch = data->table_data_main->epochs ? data->table_data_main->epochs[i] : (i + 1);
                        gtk_list_store_append(data->conclusion_alpha_mu_store, &iter);
                        gtk_list_store_set(data->conclusion_alpha_mu_store, &iter, 
                                           0, "Данные +E", 
                                           1, current_epoch, 
                                           2, data->mu_prime_data[i], 
                                           3, data->alpha_prime_data[i], 
                                           -1);
                    }
                }
                // 3. Данные -E
                if (data->mu_double_prime_data && data->alpha_double_prime_data && data->table_data_main && data->table_data_main->row_count > 0) {
                    int count_to_iterate = MIN(main_row_count, MIN(data->mu_double_prime_data_count, data->alpha_double_prime_data_count));
                     for (int i = 0; i < count_to_iterate; ++i) {
                        current_epoch = data->table_data_main->epochs ? data->table_data_main->epochs[i] : (i + 1);
                        gtk_list_store_append(data->conclusion_alpha_mu_store, &iter);
                        gtk_list_store_set(data->conclusion_alpha_mu_store, &iter, 
                                           0, "Данные -E", 
                                           1, current_epoch, 
                                           2, data->mu_double_prime_data[i], 
                                           3, data->alpha_double_prime_data[i], 
                                           -1);
                    }
                }
                debug_print_l1("   Таблица Заключения α(μ) заполнена (или попытка заполнения сделана).\n");
            } else {
                 debug_print_l1("   Шаг 4.10.1: data->conclusion_alpha_mu_store все еще NULL (или не был создан успешно), заполнение пропущено.\n");
            }
        } // Конец else для if (!GTK_IS_TREE_VIEW)
    } else { 
        debug_print_l1("   UI для таблицы Заключения α(μ) (data->conclusion_alpha_mu_table_view) не создано или NULL.\n");
    }

    // ==========================================================================
    // == ЗАВЕРШЕНИЕ ============================================================
    // ==========================================================================
    // ... (код записи в файл, как был) ...
    if (data->mu_data && data->alpha_data && data->table_data_main && data->table_data_main->epochs) {
        if (!write_results_to_file("results_level1.txt", data->mu_data, main_row_count, data->alpha_data, data->alpha_count, data->smoothing_data, data->smoothing_row_count, data->smoothing_alpha_data, data->smoothing_alpha_row_count, data->table_data_main->epochs, main_row_count)) {
            if(data->level1_window) show_message_dialog(GTK_WINDOW(data->level1_window), "Ошибка записи основных результатов в файл results_level1.txt");
        }
    }


    debug_print_l1(" - Обновление всех графиков...\n");
    if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
    if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
    if (data->level1_mu_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_prime_drawing_area);
    if (data->level1_alpha_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_prime_drawing_area);
    if (data->level1_mu_double_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_double_prime_drawing_area);
    if (data->level1_alpha_double_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_double_prime_drawing_area);
    if (data->conclusion_alpha_mu_drawing_area) gtk_widget_queue_draw(data->conclusion_alpha_mu_drawing_area);

    debug_print_l1("level1_perform_calculations_and_update_ui: ЗАВЕРШЕНО\n");
}


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

// НОВАЯ ФУНКЦИЯ для отрисовки графика α(μ) на вкладке "Заключение"
void conclusion_draw_alpha_mu_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    // ... (начало функции, проверка данных, определение диапазонов, отрисовка осей и серий данных - как в предыдущем ответе) ...
    
    // --- Начало кода из предыдущего ответа ---
    debug_print_l1("CONCLUSION_DRAW_GRAPH: Отрисовка α(μ) для DA %p, W=%d, H=%d\n", (void*)drawing_area, width, height);

    int count_initial = 0;
    if (data->mu_data && data->alpha_data && data->table_data_main) {
        count_initial = MIN(data->table_data_main->row_count, data->alpha_count);
        if (count_initial <=0) count_initial=0;
    }
    gboolean has_initial_data = count_initial > 0;

    int count_plus_e = 0;
    if (data->mu_prime_data && data->alpha_prime_data) {
        count_plus_e = MIN(data->mu_prime_data_count, data->alpha_prime_data_count);
         if (count_plus_e <=0) count_plus_e=0;
    }
    gboolean has_plus_e_data = count_plus_e > 0;

    int count_minus_e = 0;
    if (data->mu_double_prime_data && data->alpha_double_prime_data) {
        count_minus_e = MIN(data->mu_double_prime_data_count, data->alpha_double_prime_data_count);
        if (count_minus_e <=0) count_minus_e=0;
    }
    gboolean has_minus_e_data = count_minus_e > 0;
    
    if (!has_initial_data && !has_plus_e_data && !has_minus_e_data) {
        // ... (код отрисовки "Нет данных") ...
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t extents;
        const char* no_data_msg = "Нет данных для графика α(μ)";
        cairo_text_extents(cr, no_data_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height / 2.0) - (extents.height / 2.0) + extents.height);
        cairo_show_text(cr, no_data_msg);
        return;
    }

    double mu_min = G_MAXDOUBLE, mu_max = -G_MAXDOUBLE;
    double alpha_min = G_MAXDOUBLE, alpha_max = -G_MAXDOUBLE;
    gboolean data_found_for_range = FALSE;

    void update_conclusion_ranges(double* mu_arr, double* alpha_arr, int arr_count, 
                                  double* current_mu_min, double* current_mu_max, 
                                  double* current_alpha_min, double* current_alpha_max,
                                  gboolean* found_flag) {
        // ... (код update_conclusion_ranges) ...
        if (!mu_arr || !alpha_arr || arr_count == 0) return;
        for (int i = 0; i < arr_count; ++i) {
            if (!isnan(mu_arr[i]) && !isinf(mu_arr[i])) {
                if (mu_arr[i] < *current_mu_min) *current_mu_min = mu_arr[i];
                if (mu_arr[i] > *current_mu_max) *current_mu_max = mu_arr[i];
                *found_flag = TRUE;
            }
            if (!isnan(alpha_arr[i]) && !isinf(alpha_arr[i])) {
                if (alpha_arr[i] < *current_alpha_min) *current_alpha_min = alpha_arr[i];
                if (alpha_arr[i] > *current_alpha_max) *current_alpha_max = alpha_arr[i];
                *found_flag = TRUE;
            }
        }
    }

    if (has_initial_data) update_conclusion_ranges(data->mu_data, data->alpha_data, count_initial, &mu_min, &mu_max, &alpha_min, &alpha_max, &data_found_for_range);
    if (has_plus_e_data) update_conclusion_ranges(data->mu_prime_data, data->alpha_prime_data, count_plus_e, &mu_min, &mu_max, &alpha_min, &alpha_max, &data_found_for_range);
    if (has_minus_e_data) update_conclusion_ranges(data->mu_double_prime_data, data->alpha_double_prime_data, count_minus_e, &mu_min, &mu_max, &alpha_min, &alpha_max, &data_found_for_range);
    if (!data_found_for_range) { mu_min = 0; mu_max = 1; alpha_min = 0; alpha_max = 0.1; }
    
    double mu_range = mu_max - mu_min;
    if (fabs(mu_range) < 1e-9) { mu_min -= 0.5 * (fabs(mu_min)>1e-9 ? fabs(mu_min) : 1.0); mu_max += 0.5 * (fabs(mu_max)>1e-9 ? fabs(mu_max) : 1.0); if(fabs(mu_max-mu_min)<1e-9){mu_min=0; mu_max=1;} }
    else { mu_min -= mu_range * 0.1; mu_max += mu_range * 0.1; }
    mu_range = mu_max - mu_min; if (fabs(mu_range) < 1e-9) mu_range = 1.0;

    double alpha_range = alpha_max - alpha_min;
    if (fabs(alpha_range) < 1e-9) { alpha_min -= 0.00005 * (fabs(alpha_min)>1e-9 ? fabs(alpha_min) : 1.0); alpha_max += 0.00005 * (fabs(alpha_max)>1e-9 ? fabs(alpha_max) : 1.0); if(fabs(alpha_max-alpha_min)<1e-9){alpha_min=0; alpha_max=0.0001;} }
    else { alpha_min -= alpha_range * 0.1; alpha_max += alpha_range * 0.1; }
    alpha_range = alpha_max - alpha_min; if (fabs(alpha_range) < 1e-9) alpha_range = 1e-5;

    double margin_left = 70.0, margin_right = 30.0, margin_top = 40.0, margin_bottom = 90.0;
    double plot_width = width - margin_left - margin_right;
    double plot_height = height - margin_top - margin_bottom;
    if (plot_width <= 10 || plot_height <= 10) return;

    double x_scale_mu = plot_width / mu_range;
    double y_scale_alpha = plot_height / alpha_range;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, margin_left, height - margin_bottom); cairo_line_to(cr, margin_left + plot_width, height - margin_bottom); cairo_stroke(cr);
    cairo_move_to(cr, margin_left, margin_top); cairo_line_to(cr, margin_left, margin_top + plot_height); cairo_stroke(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(cr, 10);
    
    int num_x_ticks = 5;
    for (int i = 0; i <= num_x_ticks; ++i) { /* ... (код отрисовки меток X) ... */ 
        double val = mu_min + i * (mu_range / num_x_ticks);
        double xpos = margin_left + (val - mu_min) * x_scale_mu;
        char label[32]; snprintf(label, sizeof(label), "%.2f", val);
        cairo_text_extents_t ext; cairo_text_extents(cr, label, &ext);
        cairo_move_to(cr, xpos - ext.width / 2.0, height - margin_bottom + 15); cairo_show_text(cr, label);
        cairo_move_to(cr, xpos, height-margin_bottom); cairo_line_to(cr, xpos, height-margin_bottom+4);cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.7); cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, xpos, height - margin_bottom); cairo_line_to(cr, xpos, margin_top); cairo_stroke(cr);
        cairo_set_source_rgb(cr, 0,0,0); cairo_set_line_width(cr, 1);
    }
    cairo_text_extents_t x_label_ext; cairo_text_extents(cr, "μ", &x_label_ext);
    cairo_move_to(cr, margin_left + plot_width/2 - x_label_ext.width/2, height - margin_bottom + 35); cairo_show_text(cr, "μ");

    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; ++i) { /* ... (код отрисовки меток Y) ... */ 
        double val = alpha_min + i * (alpha_range / num_y_ticks);
        double ypos = height - margin_bottom - (val - alpha_min) * y_scale_alpha;
        char label[32]; snprintf(label, sizeof(label), "%.1E", val);
        cairo_text_extents_t ext; cairo_text_extents(cr, label, &ext);
        cairo_move_to(cr, margin_left - ext.width - 5, ypos + ext.height / 2.0); cairo_show_text(cr, label);
        cairo_move_to(cr, margin_left-4, ypos); cairo_line_to(cr, margin_left, ypos);cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.7); cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, margin_left, ypos); cairo_line_to(cr, margin_left + plot_width, ypos); cairo_stroke(cr);
        cairo_set_source_rgb(cr, 0,0,0); cairo_set_line_width(cr, 1);
    }
    cairo_text_extents_t y_label_ext; cairo_text_extents(cr, "α", &y_label_ext);
    cairo_save(cr); cairo_move_to(cr, margin_left - 55, margin_top + plot_height/2 + y_label_ext.width/2);
    cairo_rotate(cr, -M_PI/2.0); cairo_show_text(cr, "α"); cairo_restore(cr);

    const char* title = "Зависимость α от μ";
    cairo_text_extents_t title_ext; cairo_set_font_size(cr, 14); cairo_text_extents(cr, title, &title_ext);
    cairo_move_to(cr, margin_left + (plot_width - title_ext.width) / 2.0, margin_top - 15); cairo_show_text(cr, title);
    cairo_set_font_size(cr, 10);

    double line_colors[3][3] = {{0.8, 0.2, 0.2}, {0.2, 0.7, 0.2}, {0.2, 0.2, 0.8}}; 
    
    void draw_alpha_mu_series(double* mu_arr, double* alpha_arr, int arr_count, int color_idx_local) {
        // ... (код draw_alpha_mu_series) ...
        if (!mu_arr || !alpha_arr || arr_count == 0) return;
        cairo_set_source_rgb(cr, line_colors[color_idx_local][0], line_colors[color_idx_local][1], line_colors[color_idx_local][2]);
        cairo_set_line_width(cr, 1.8);
        gboolean first_point = TRUE;
        for (int i = 0; i < arr_count; ++i) {
            if (isnan(mu_arr[i]) || isinf(mu_arr[i]) || isnan(alpha_arr[i]) || isinf(alpha_arr[i])) {
                first_point = TRUE; continue;
            }
            double xpos = margin_left + (mu_arr[i] - mu_min) * x_scale_mu;
            double ypos = height - margin_bottom - (alpha_arr[i] - alpha_min) * y_scale_alpha;
            double effective_xpos = MAX(margin_left, MIN(xpos, margin_left + plot_width));
            double effective_ypos = MAX(margin_top, MIN(ypos, margin_top + plot_height));
            if (first_point) { cairo_move_to(cr, effective_xpos, effective_ypos); first_point = FALSE; } 
            else { cairo_line_to(cr, effective_xpos, effective_ypos); }
        }
        if (!first_point) cairo_stroke(cr); 
        for (int i = 0; i < arr_count; ++i) {
            if (isnan(mu_arr[i]) || isinf(mu_arr[i]) || isnan(alpha_arr[i]) || isinf(alpha_arr[i])) continue;
            double xpos = margin_left + (mu_arr[i] - mu_min) * x_scale_mu;
            double ypos = height - margin_bottom - (alpha_arr[i] - alpha_min) * y_scale_alpha;
            if(xpos >= margin_left - 2 && xpos <= margin_left + plot_width + 2 && ypos >= margin_top - 2 && ypos <= height - margin_bottom + 2) {
                cairo_arc(cr, xpos, ypos, 2.8, 0, 2 * M_PI); cairo_fill(cr);
            }
        }
    }

    if (has_initial_data) draw_alpha_mu_series(data->mu_data, data->alpha_data, count_initial, 0);
    if (has_plus_e_data) draw_alpha_mu_series(data->mu_prime_data, data->alpha_prime_data, count_plus_e, 1);
    if (has_minus_e_data) draw_alpha_mu_series(data->mu_double_prime_data, data->alpha_double_prime_data, count_minus_e, 2);
    // --- КОНЕЦ КОДА ИЗ ПРЕДЫДУЩЕГО ОТВЕТА ---

    // 5. Отрисовка легенды (продолжение)
    const char* series_labels[] = {"Исходные α(μ)", "α'(μ') для X+E", "α''(μ'') для X-E"};
    double legend_x_start = margin_left + 10; // Небольшой отступ от левого края графика
    double legend_y_start = height - margin_bottom + 55; // Ниже подписей оси X
    double legend_item_height = 15;
    double legend_color_box_width = 20;
    double legend_text_spacing = 5;
    
    cairo_set_font_size(cr, 10);

    for (int i = 0; i < 3; ++i) {
        gboolean current_series_has_data = FALSE;
        if (i == 0 && has_initial_data) current_series_has_data = TRUE;
        else if (i == 1 && has_plus_e_data) current_series_has_data = TRUE;
        else if (i == 2 && has_minus_e_data) current_series_has_data = TRUE;

        if (!current_series_has_data) continue;

        // Прямоугольник цвета
        cairo_set_source_rgb(cr, line_colors[i][0], line_colors[i][1], line_colors[i][2]);
        cairo_rectangle(cr, legend_x_start, legend_y_start + i * legend_item_height, legend_color_box_width, 10);
        cairo_fill(cr);

        // Текст легенды
        cairo_set_source_rgb(cr, 0, 0, 0); // Черный текст
        cairo_text_extents_t leg_ext;
        cairo_text_extents(cr, series_labels[i], &leg_ext);
        cairo_move_to(cr, legend_x_start + legend_color_box_width + legend_text_spacing, 
                      legend_y_start + i * legend_item_height + leg_ext.height - (leg_ext.height - 10)/2.0); // Центрируем текст по высоте с квадратом
        cairo_show_text(cr, series_labels[i]);
    }
    debug_print_l1("CONCLUSION_DRAW_GRAPH: Отрисовка α(μ) завершена.\n");
}


#pragma GCC diagnostic pop 