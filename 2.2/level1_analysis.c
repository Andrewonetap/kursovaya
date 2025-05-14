// В файле level1_analysis.c

#include "level1_analysis.h"
#include "calculations.h"
#include "sqlite_utils.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Объявления функций из main.c, которые мы вызываем
extern void update_tree_view_columns(AppData *data,
                                     gboolean is_level1_context,
                                     gboolean is_smoothing_mu_table,
                                     gboolean is_smoothing_alpha_table,
                                     gboolean is_smoothing_error_adj_x0_table,
                                     gboolean is_smoothing_mu_prime_table,
                                     gboolean is_smoothing_alpha_prime_table);extern void show_message_dialog(GtkWindow *parent, const char *message);


// Добавляем extern объявление для cell_data_func
extern void cell_data_func(GtkTreeViewColumn *tree_column,
                           GtkCellRenderer *cell,
                           GtkTreeModel *tree_model,
                           GtkTreeIter *iter,
                           gpointer data);

static FILE *debug_log_l1 = NULL;

static void init_l1_debug_log() {
    if (!debug_log_l1) {
        debug_log_l1 = fopen("debug.log", "a");
        if (!debug_log_l1) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (level1_analysis.c)\n");
        } else {
            fseek(debug_log_l1, 0, SEEK_END);
            if (ftell(debug_log_l1) == 0) {
                fprintf(debug_log_l1, "\xEF\xBB\xBF");
            }
            fflush(debug_log_l1);
        }
    }
}

static void debug_print_l1(const char *format, ...) {
    init_l1_debug_log();
    if (debug_log_l1) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_l1, format, args);
        va_end(args);
        fflush(debug_log_l1);
    }
}

static void level1_clear_internal_data(AppData *data);
static void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);
static void level1_perform_calculations_and_update_ui(AppData *data);
static void level1_on_window_destroy(GtkWidget *widget, gpointer user_data);
static void level1_draw_error_adj_x0_smoothing_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);

// Добавим прототип для новой функции очистки
static void clear_error_adjusted_data(AppData *data);

// static void clear_error_adjusted_data(AppData *data) {
//     if (data->error_adjusted_table_data) {
//         // Предполагаем, что количество строк равно data->table_data_main->row_count
//         // и оно было установлено при создании error_adjusted_table_data
//         if (data->table_data_main && data->table_data_main->row_count > 0) {
//             for (int i = 0; i < data->table_data_main->row_count; i++) {
//                 if (data->error_adjusted_table_data[i]) {
//                     g_free(data->error_adjusted_table_data[i]);
//                 }
//             }
//         }
//         g_free(data->error_adjusted_table_data);
//         data->error_adjusted_table_data = NULL;
//         debug_print_l1(" - error_adjusted_table_data очищен.\n");
//     }
// }

// Прототип для обработчика событий скролла
static gboolean on_inner_scroll_event (GtkEventController *controller_generic, // Используем базовый тип
                                       double             dx,
                                       double             dy,
                                       gpointer           user_data);


// Вспомогательная функция-обработчик события прокрутки для GtkEventControllerScroll
static gboolean
on_inner_scroll_event (GtkEventController *controller_generic, // Используем базовый тип
                       double             dx,
                       double             dy,
                       gpointer           user_data)
{
    AppData *data = user_data;
    GtkAdjustment *main_vadjustment;
    double current_value;
    double new_value;
    double step_increment;
    // GtkEventControllerScroll *controller = GTK_EVENT_CONTROLLER_SCROLL(controller_generic); // Приведение, если нужны специфичные функции
    GtkWidget *scrolled_window_widget_source = gtk_event_controller_get_widget(controller_generic);


    if (!data || !data->level1_window || !GTK_IS_SCROLLED_WINDOW(scrolled_window_widget_source) ||
        !gtk_widget_get_ancestor(scrolled_window_widget_source, GTK_TYPE_WINDOW)) {
        return FALSE;
    }
    
    GtkWidget *main_scroller_widget = g_object_get_data(G_OBJECT(data->level1_window), "main_scroller_l1");
    if (!main_scroller_widget || !GTK_IS_SCROLLED_WINDOW(main_scroller_widget)) {
        debug_print_l1("on_inner_scroll_event: Главный скроллер не найден.\n");
        return FALSE;
    }
    GtkScrolledWindow *main_scroller = GTK_SCROLLED_WINDOW(main_scroller_widget);

    if (fabs(dy) > 0.00001) {
        GtkPolicyType vpolicy, hpolicy;
        gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(scrolled_window_widget_source), &hpolicy, &vpolicy);

        if (vpolicy == GTK_POLICY_NEVER) {
            main_vadjustment = gtk_scrolled_window_get_vadjustment(main_scroller);
            if (!main_vadjustment) {
                debug_print_l1("on_inner_scroll_event: VAdjustment главного скроллера не найден.\n");
                return FALSE;
            }

            current_value = gtk_adjustment_get_value(main_vadjustment);
            step_increment = gtk_adjustment_get_step_increment(main_vadjustment);
            if (step_increment < 1.0) step_increment = 30.0; 

            new_value = current_value + (dy * step_increment);

            double lower = gtk_adjustment_get_lower(main_vadjustment);
            double upper = gtk_adjustment_get_upper(main_vadjustment) - gtk_adjustment_get_page_size(main_vadjustment);

            if (new_value < lower) new_value = lower;
            if (new_value > upper) new_value = upper;
            
            if (fabs(new_value - current_value) > 0.00001) {
                 gtk_adjustment_set_value(main_vadjustment, new_value);
            }
            return TRUE; 
        }
    }
    return FALSE; 
}


void level1_show_window(AppData *data) {
    debug_print_l1("level1_show_window: Запрошено окно Уровня 1 (полная версия).\n");

    if (!data->level1_window) {
        debug_print_l1("level1_show_window: Окно не существует, создаем новое...\n");
        data->level1_window = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(data->level1_window), "Уровень 1: Анализ данных");
        gtk_window_set_default_size(GTK_WINDOW(data->level1_window), 1200, 900); // Увеличим немного стандартный размер
        if(data->parent) gtk_window_set_transient_for(GTK_WINDOW(data->level1_window), data->parent);
        g_signal_connect(data->level1_window, "destroy", G_CALLBACK(level1_on_window_destroy), data);

        GtkWidget *level1_scrolled_window_main_vertical = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(level1_scrolled_window_main_vertical),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_window_set_child(GTK_WINDOW(data->level1_window), level1_scrolled_window_main_vertical);
        g_object_set_data(G_OBJECT(data->level1_window), "main_scroller_l1", level1_scrolled_window_main_vertical);

        GtkWidget *main_vbox_l1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_margin_start(main_vbox_l1, 10);
        gtk_widget_set_margin_end(main_vbox_l1, 10);
        gtk_widget_set_margin_top(main_vbox_l1, 10);
        gtk_widget_set_margin_bottom(main_vbox_l1, 10);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(level1_scrolled_window_main_vertical), main_vbox_l1);

        // --- 1. Основная таблица L1 (данные, mu, alpha) ---
        GtkWidget *main_table_label = gtk_label_new("<b>Данные основной таблицы (с μ и α)</b>");
        gtk_label_set_use_markup(GTK_LABEL(main_table_label), TRUE); gtk_widget_set_halign(main_table_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(main_vbox_l1), main_table_label);
        GtkWidget *main_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(main_table_scrolled, FALSE); gtk_widget_set_hexpand(main_table_scrolled, TRUE);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(main_table_scrolled), TRUE);
        data->level1_tree_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_tree_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_tree_view)) g_object_set_data(G_OBJECT(data->level1_tree_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_table_scrolled), data->level1_tree_view);
        gtk_box_append(GTK_BOX(main_vbox_l1), main_table_scrolled);
        GtkEventController *scroll_controller_main = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_main, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(main_table_scrolled, scroll_controller_main);

        // --- 2. Блок Анализ μ ---
        GtkWidget *mu_block_label = gtk_label_new("<b>Анализ μ (мю)</b>");
        gtk_label_set_use_markup(GTK_LABEL(mu_block_label), TRUE); gtk_widget_set_halign(mu_block_label, GTK_ALIGN_START); gtk_widget_set_margin_top(mu_block_label, 10);
        gtk_box_append(GTK_BOX(main_vbox_l1), mu_block_label);
        GtkWidget *hbox_mu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_hexpand(hbox_mu, TRUE); gtk_widget_set_vexpand(hbox_mu, TRUE); 
        gtk_box_append(GTK_BOX(main_vbox_l1), hbox_mu);
        GtkWidget *smoothing_mu_scrolled = gtk_scrolled_window_new(); // Таблица сглаживания μ
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(smoothing_mu_scrolled, FALSE); gtk_widget_set_hexpand(smoothing_mu_scrolled, FALSE); 
        gtk_widget_set_size_request(smoothing_mu_scrolled, 480, -1); 
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), TRUE);
        data->level1_smoothing_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_smoothing_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_smoothing_view)) g_object_set_data(G_OBJECT(data->level1_smoothing_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), data->level1_smoothing_view);
        gtk_box_append(GTK_BOX(hbox_mu), smoothing_mu_scrolled);
        GtkEventController *scroll_controller_mu = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_mu, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(smoothing_mu_scrolled, scroll_controller_mu);
        data->level1_drawing_area = gtk_drawing_area_new(); // График μ
        gtk_widget_set_vexpand(data->level1_drawing_area, TRUE); gtk_widget_set_hexpand(data->level1_drawing_area, TRUE); 
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(hbox_mu), data->level1_drawing_area);

        // --- 3. Блок Анализ α ---
        GtkWidget *alpha_block_label = gtk_label_new("<b>Анализ α (альфа)</b>");
        gtk_label_set_use_markup(GTK_LABEL(alpha_block_label), TRUE); gtk_widget_set_halign(alpha_block_label, GTK_ALIGN_START); gtk_widget_set_margin_top(alpha_block_label, 10);
        gtk_box_append(GTK_BOX(main_vbox_l1), alpha_block_label);
        GtkWidget *hbox_alpha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_hexpand(hbox_alpha, TRUE); gtk_widget_set_vexpand(hbox_alpha, TRUE); 
        gtk_box_append(GTK_BOX(main_vbox_l1), hbox_alpha);
        GtkWidget *smoothing_alpha_scrolled = gtk_scrolled_window_new(); // Таблица сглаживания α
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_alpha_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(smoothing_alpha_scrolled, FALSE); gtk_widget_set_hexpand(smoothing_alpha_scrolled, FALSE); 
        gtk_widget_set_size_request(smoothing_alpha_scrolled, 480, -1); 
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(smoothing_alpha_scrolled), TRUE);
        data->level1_alpha_smoothing_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_alpha_smoothing_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_alpha_smoothing_view)) g_object_set_data(G_OBJECT(data->level1_alpha_smoothing_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_alpha_scrolled), data->level1_alpha_smoothing_view);
        gtk_box_append(GTK_BOX(hbox_alpha), smoothing_alpha_scrolled);
        GtkEventController *scroll_controller_alpha = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_alpha, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(smoothing_alpha_scrolled, scroll_controller_alpha);
        data->level1_alpha_drawing_area = gtk_drawing_area_new(); // График α
        gtk_widget_set_vexpand(data->level1_alpha_drawing_area, TRUE); gtk_widget_set_hexpand(data->level1_alpha_drawing_area, TRUE); 
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_alpha_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(hbox_alpha), data->level1_alpha_drawing_area);

        // --- 4. Основная таблица с учетом погрешности E (X+E, mu_prime, alpha_prime) ---
        GtkWidget *error_adj_table_label = gtk_label_new("<b>Основная таблица с учетом погрешности E (X+E, μ', α')</b>");
        gtk_label_set_use_markup(GTK_LABEL(error_adj_table_label), TRUE); gtk_widget_set_halign(error_adj_table_label, GTK_ALIGN_START); gtk_widget_set_margin_top(error_adj_table_label, 15);
        gtk_box_append(GTK_BOX(main_vbox_l1), error_adj_table_label);
        GtkWidget *error_adj_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(error_adj_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(error_adj_table_scrolled, FALSE); gtk_widget_set_hexpand(error_adj_table_scrolled, TRUE);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(error_adj_table_scrolled), TRUE);
        data->level1_error_adj_tree_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_error_adj_tree_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_error_adj_tree_view)) g_object_set_data(G_OBJECT(data->level1_error_adj_tree_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(error_adj_table_scrolled), data->level1_error_adj_tree_view);
        gtk_box_append(GTK_BOX(main_vbox_l1), error_adj_table_scrolled);
        GtkEventController *scroll_controller_error_adj = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_error_adj, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(error_adj_table_scrolled, scroll_controller_error_adj);
        // Колонки для этой таблицы создаются в level1_perform_calculations_and_update_ui

        // --- 5. Блок Анализ X0+E (сглаживание первой колонки X+E) ---
        GtkWidget *error_adj_x0_block_label = gtk_label_new("<b>Анализ X0+E (сглаживание первой колонки с погрешностью)</b>");
        gtk_label_set_use_markup(GTK_LABEL(error_adj_x0_block_label), TRUE); gtk_widget_set_halign(error_adj_x0_block_label, GTK_ALIGN_START); gtk_widget_set_margin_top(error_adj_x0_block_label, 10);
        gtk_box_append(GTK_BOX(main_vbox_l1), error_adj_x0_block_label);
        GtkWidget *hbox_error_adj_x0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_hexpand(hbox_error_adj_x0, TRUE); gtk_widget_set_vexpand(hbox_error_adj_x0, TRUE); 
        gtk_box_append(GTK_BOX(main_vbox_l1), hbox_error_adj_x0);
        GtkWidget *smoothing_error_adj_x0_scrolled = gtk_scrolled_window_new(); // Таблица сглаживания X0+E
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_error_adj_x0_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(smoothing_error_adj_x0_scrolled, FALSE); gtk_widget_set_hexpand(smoothing_error_adj_x0_scrolled, FALSE); 
        gtk_widget_set_size_request(smoothing_error_adj_x0_scrolled, 480, -1); 
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(smoothing_error_adj_x0_scrolled), TRUE);
        data->level1_error_adj_smoothing_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_error_adj_smoothing_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_error_adj_smoothing_view)) g_object_set_data(G_OBJECT(data->level1_error_adj_smoothing_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_error_adj_x0_scrolled), data->level1_error_adj_smoothing_view);
        gtk_box_append(GTK_BOX(hbox_error_adj_x0), smoothing_error_adj_x0_scrolled);
        GtkEventController *scroll_controller_err_adj_smooth = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_err_adj_smooth, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(smoothing_error_adj_x0_scrolled, scroll_controller_err_adj_smooth);
        data->level1_error_adj_drawing_area = gtk_drawing_area_new(); // График X0+E
        gtk_widget_set_vexpand(data->level1_error_adj_drawing_area, TRUE); gtk_widget_set_hexpand(data->level1_error_adj_drawing_area, TRUE); 
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_error_adj_drawing_area), level1_draw_graph, data, NULL); // Используем общую функцию
        gtk_box_append(GTK_BOX(hbox_error_adj_x0), data->level1_error_adj_drawing_area);

        // --- 6. НОВЫЙ БЛОК: Анализ μ' (мю с погрешностью E) ---
        GtkWidget *mu_prime_block_label = gtk_label_new("<b>Анализ μ' (мю с погрешностью E)</b>");
        gtk_label_set_use_markup(GTK_LABEL(mu_prime_block_label), TRUE); gtk_widget_set_halign(mu_prime_block_label, GTK_ALIGN_START); gtk_widget_set_margin_top(mu_prime_block_label, 10);
        gtk_box_append(GTK_BOX(main_vbox_l1), mu_prime_block_label);
        GtkWidget *hbox_mu_prime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_hexpand(hbox_mu_prime, TRUE); gtk_widget_set_vexpand(hbox_mu_prime, TRUE); 
        gtk_box_append(GTK_BOX(main_vbox_l1), hbox_mu_prime);
        GtkWidget *smoothing_mu_prime_scrolled = gtk_scrolled_window_new(); // Таблица сглаживания μ'
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_mu_prime_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(smoothing_mu_prime_scrolled, FALSE); gtk_widget_set_hexpand(smoothing_mu_prime_scrolled, FALSE); 
        gtk_widget_set_size_request(smoothing_mu_prime_scrolled, 480, -1); 
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(smoothing_mu_prime_scrolled), TRUE);
        data->level1_mu_prime_smoothing_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_mu_prime_smoothing_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_mu_prime_smoothing_view)) g_object_set_data(G_OBJECT(data->level1_mu_prime_smoothing_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_mu_prime_scrolled), data->level1_mu_prime_smoothing_view);
        gtk_box_append(GTK_BOX(hbox_mu_prime), smoothing_mu_prime_scrolled);
        GtkEventController *scroll_controller_mu_prime = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_mu_prime, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(smoothing_mu_prime_scrolled, scroll_controller_mu_prime);
        data->level1_mu_prime_drawing_area = gtk_drawing_area_new(); // График μ'
        gtk_widget_set_vexpand(data->level1_mu_prime_drawing_area, TRUE); gtk_widget_set_hexpand(data->level1_mu_prime_drawing_area, TRUE); 
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_mu_prime_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(hbox_mu_prime), data->level1_mu_prime_drawing_area);

        // --- 7. НОВЫЙ БЛОК: Анализ α' (альфа с погрешностью E) ---
        GtkWidget *alpha_prime_block_label = gtk_label_new("<b>Анализ α' (альфа с погрешностью E)</b>");
        gtk_label_set_use_markup(GTK_LABEL(alpha_prime_block_label), TRUE); gtk_widget_set_halign(alpha_prime_block_label, GTK_ALIGN_START); gtk_widget_set_margin_top(alpha_prime_block_label, 10);
        gtk_box_append(GTK_BOX(main_vbox_l1), alpha_prime_block_label);
        GtkWidget *hbox_alpha_prime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_hexpand(hbox_alpha_prime, TRUE); gtk_widget_set_vexpand(hbox_alpha_prime, TRUE); 
        gtk_box_append(GTK_BOX(main_vbox_l1), hbox_alpha_prime);
        GtkWidget *smoothing_alpha_prime_scrolled = gtk_scrolled_window_new(); // Таблица сглаживания α'
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_alpha_prime_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_widget_set_vexpand(smoothing_alpha_prime_scrolled, FALSE); gtk_widget_set_hexpand(smoothing_alpha_prime_scrolled, FALSE); 
        gtk_widget_set_size_request(smoothing_alpha_prime_scrolled, 480, -1); 
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(smoothing_alpha_prime_scrolled), TRUE);
        data->level1_alpha_prime_smoothing_view = gtk_tree_view_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->level1_alpha_prime_smoothing_view), TRUE);
        if (GTK_IS_WIDGET(data->level1_alpha_prime_smoothing_view)) g_object_set_data(G_OBJECT(data->level1_alpha_prime_smoothing_view), "app_data_ptr_for_debug", data);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_alpha_prime_scrolled), data->level1_alpha_prime_smoothing_view);
        gtk_box_append(GTK_BOX(hbox_alpha_prime), smoothing_alpha_prime_scrolled);
        GtkEventController *scroll_controller_alpha_prime = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
        g_signal_connect (scroll_controller_alpha_prime, "scroll", G_CALLBACK (on_inner_scroll_event), data);
        gtk_widget_add_controller(smoothing_alpha_prime_scrolled, scroll_controller_alpha_prime);
        data->level1_alpha_prime_drawing_area = gtk_drawing_area_new(); // График α'
        gtk_widget_set_vexpand(data->level1_alpha_prime_drawing_area, TRUE); gtk_widget_set_hexpand(data->level1_alpha_prime_drawing_area, TRUE); 
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_alpha_prime_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(hbox_alpha_prime), data->level1_alpha_prime_drawing_area);

        // Настройка колонок для всех таблиц через обновленную update_tree_view_columns
        debug_print_l1("   Вызов update_tree_view_columns для L1_MAIN...");
        update_tree_view_columns(data, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE); 
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_MU...");
        update_tree_view_columns(data, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE); 
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_ALPHA...");
        update_tree_view_columns(data, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE); 
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_ERROR_ADJ_X0...");
        update_tree_view_columns(data, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_MU_PRIME...");
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE);
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_ALPHA_PRIME...");
        update_tree_view_columns(data, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE);
        
        debug_print_l1("   Структура окна L1 создана.\n");
        level1_perform_calculations_and_update_ui(data);
    } else {
        debug_print_l1("level1_show_window: Окно уже существует. Обновление содержимого...\n");
        level1_perform_calculations_and_update_ui(data);
    }
    gtk_window_present(GTK_WINDOW(data->level1_window));
    debug_print_l1("level1_show_window: Окно показано/поднято.\n");
}

// ... (остальные статические функции: level1_clear_internal_data, level1_perform_calculations_and_update_ui, level1_draw_graph, level1_on_window_destroy) ...
// ... (их код не менялся и должен быть здесь) ...

// --- Вспомогательная функция для очистки данных таблицы с погрешностями ---
static void clear_error_adjusted_data(AppData *data) {
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
        debug_print_l1(" - error_adjusted_table_data очищен.\n");
    }
}

static void level1_clear_internal_data(AppData *data) {
    debug_print_l1("level1_clear_internal_data: Очистка массивов данных Уровня 1...\n");

    // Очистка данных основного анализа (mu, alpha и их сглаживание)
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; i++) {
            if (data->smoothing_data[i]) {
                g_free(data->smoothing_data[i]);
            }
        }
        g_free(data->smoothing_data);
        data->smoothing_data = NULL;
        data->smoothing_row_count = 0;
        debug_print_l1(" - smoothing_data (для mu) очищен.\n");
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; i++) {
            if (data->smoothing_alpha_data[i]) {
                g_free(data->smoothing_alpha_data[i]);
            }
        }
        g_free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL;
        data->smoothing_alpha_row_count = 0;
        debug_print_l1(" - smoothing_alpha_data очищен.\n");
    }
    if (data->mu_data) {
        g_free(data->mu_data);
        data->mu_data = NULL;
        // data->mu_count (если бы был) = 0; // Количество строк берется из table_data_main
        debug_print_l1(" - mu_data очищен.\n");
    }
    if (data->alpha_data) {
        g_free(data->alpha_data);
        data->alpha_data = NULL;
        data->alpha_count = 0;
        debug_print_l1(" - alpha_data очищен.\n");
    }

    // Очистка данных таблицы X+E и ее производных (mu_prime, alpha_prime)
    clear_error_adjusted_data(data); // Очищает data->error_adjusted_table_data

    if (data->mu_prime_data) {
        g_free(data->mu_prime_data);
        data->mu_prime_data = NULL;
        data->mu_prime_data_count = 0;
        debug_print_l1(" - mu_prime_data очищен.\n");
    }
    if (data->alpha_prime_data) {
        g_free(data->alpha_prime_data);
        data->alpha_prime_data = NULL;
        data->alpha_prime_data_count = 0;
        debug_print_l1(" - alpha_prime_data очищен.\n");
    }

    // Очистка данных сглаживания для X0+E
    if (data->smoothing_error_adjusted_data_x0) {
        for (int i = 0; i < data->smoothing_error_adjusted_data_x0_row_count; i++) {
            if (data->smoothing_error_adjusted_data_x0[i]) {
                g_free(data->smoothing_error_adjusted_data_x0[i]);
            }
        }
        g_free(data->smoothing_error_adjusted_data_x0);
        data->smoothing_error_adjusted_data_x0 = NULL;
        data->smoothing_error_adjusted_data_x0_row_count = 0;
        debug_print_l1(" - smoothing_error_adjusted_data_x0 очищен.\n");
    }

    // НОВОЕ: Очистка данных сглаживания для mu_prime
    if (data->smoothing_mu_prime_data) {
        for (int i = 0; i < data->smoothing_mu_prime_row_count; i++) {
            if (data->smoothing_mu_prime_data[i]) {
                g_free(data->smoothing_mu_prime_data[i]);
            }
        }
        g_free(data->smoothing_mu_prime_data);
        data->smoothing_mu_prime_data = NULL;
        data->smoothing_mu_prime_row_count = 0;
        debug_print_l1(" - smoothing_mu_prime_data очищен.\n");
    }

    // НОВОЕ: Очистка данных сглаживания для alpha_prime
    if (data->smoothing_alpha_prime_data) {
        for (int i = 0; i < data->smoothing_alpha_prime_row_count; i++) {
            if (data->smoothing_alpha_prime_data[i]) {
                g_free(data->smoothing_alpha_prime_data[i]);
            }
        }
        g_free(data->smoothing_alpha_prime_data);
        data->smoothing_alpha_prime_data = NULL;
        data->smoothing_alpha_prime_row_count = 0;
        debug_print_l1(" - smoothing_alpha_prime_data очищен.\n");
    }

    debug_print_l1("level1_clear_internal_data: Очистка всех данных Уровня 1 завершена.\n");
}

static void level1_perform_calculations_and_update_ui(AppData *data) {
    debug_print_l1("level1_perform_calculations_and_update_ui: НАЧАЛО (полная версия)\n");
    level1_clear_internal_data(data); // Очищает все вычисленные данные L1

    if(data->table_data_main == NULL || data->table_data_main->row_count == 0) {
        debug_print_l1("level1_perform_calculations_and_update_ui: НЕТ ДАННЫХ в table_data_main.\n");
        if (data->level1_store) gtk_list_store_clear(data->level1_store);
        if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
        if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
        if (data->level1_error_adj_store) gtk_list_store_clear(data->level1_error_adj_store);
        if (data->level1_error_adj_smoothing_store) gtk_list_store_clear(data->level1_error_adj_smoothing_store);
        if (data->level1_mu_prime_smoothing_store) gtk_list_store_clear(data->level1_mu_prime_smoothing_store); // НОВОЕ
        if (data->level1_alpha_prime_smoothing_store) gtk_list_store_clear(data->level1_alpha_prime_smoothing_store); // НОВОЕ

        if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
        if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
        if (data->level1_error_adj_drawing_area) gtk_widget_queue_draw(data->level1_error_adj_drawing_area);
        if (data->level1_mu_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_prime_drawing_area); // НОВОЕ
        if (data->level1_alpha_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_prime_drawing_area); // НОВОЕ
        return;
    }

    int main_row_count = data->table_data_main->row_count;
    int main_col_count = data->table_data_main->column_count;

    // 1. Расчеты mu и alpha для ОСНОВНОЙ таблицы
    debug_print_l1(" - Расчет mu и alpha для основной таблицы...\n");
    data->mu_data = compute_mu(data->table_data_main->table, main_row_count, main_col_count);
    if (data->mu_data) {
        data->alpha_data = compute_alpha(data->table_data_main->table, main_row_count, main_col_count, data->mu_data);
        data->alpha_count = data->alpha_data ? main_row_count : 0;
    } else {
        data->alpha_data = NULL; data->alpha_count = 0;
    }

    // 2. Заполнение основной таблицы L1 (level1_store)
    if (data->level1_store) {
        debug_print_l1(" - Заполнение level1_store (основная таблица L1)...\n");
        gtk_list_store_clear(data->level1_store);
        int store_col_count = gtk_tree_model_get_n_columns(GTK_TREE_MODEL(data->level1_store));
        for (int i = 0; i < main_row_count; i++) {
            GtkTreeIter iter; gtk_list_store_append(data->level1_store, &iter);
            gtk_list_store_set(data->level1_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
            if (data->table_data_main->table && data->table_data_main->table[i]) {
                for (int j = 0; j < main_col_count; j++) {
                    if ((j + 1) < store_col_count) gtk_list_store_set(data->level1_store, &iter, j + 1, data->table_data_main->table[i][j], -1);
                    else break;
                }
            }
            int mu_model_idx = main_col_count + 1;
            if (mu_model_idx < store_col_count) gtk_list_store_set(data->level1_store, &iter, mu_model_idx, (data->mu_data && i < main_row_count) ? data->mu_data[i] : NAN, -1);
            int alpha_model_idx = main_col_count + 2;
            if (alpha_model_idx < store_col_count) gtk_list_store_set(data->level1_store, &iter, alpha_model_idx, (data->alpha_data && i < data->alpha_count) ? data->alpha_data[i] : NAN, -1);
        }
    }

    // 3. ВЫЧИСЛЕНИЕ ДАННЫХ ДЛЯ ТАБЛИЦЫ С ПОГРЕШНОСТЯМИ E (error_adjusted_table_data)
    debug_print_l1(" - Вычисление данных для таблицы с погрешностями E...\n");
    if (data->table_data_values && data->table_data_values->table &&
        data->table_data_values->row_count > 0 && data->table_data_values->column_names &&
        data->table_data_main && data->table_data_main->table) {
        
        int e_col_idx = -1;
        for (int k = 0; k < data->table_data_values->column_count; ++k) {
            if (data->table_data_values->column_names[k] &&
                strcmp(data->table_data_values->column_names[k], "E") == 0) {
                e_col_idx = k;
                break;
            }
        }

        if (e_col_idx != -1) {
            debug_print_l1("   Колонка 'E' найдена в таблице 'Значения' (индекс %d).\n", e_col_idx);
            data->error_adjusted_table_data = g_new0(double*, main_row_count);
            for (int i = 0; i < main_row_count; ++i) {
                if (main_col_count >= 0 && data->table_data_main->table[i]) { // main_col_count может быть 0
                    data->error_adjusted_table_data[i] = g_new0(double, main_col_count > 0 ? main_col_count : 1); // Выделяем хотя бы 1, если main_col_count=0, чтобы избежать g_new0(..., 0)
                    double error_e_value = 0.0;
                    if (i < data->table_data_values->row_count && data->table_data_values->table[i]) {
                        error_e_value = data->table_data_values->table[i][e_col_idx];
                        if (isnan(error_e_value)) error_e_value = 0.0;
                    } else {
                        debug_print_l1("   Предупреждение: Нет данных E для строки %d основной таблицы. Используется E=0.\n", i);
                    }
                    for (int j = 0; j < main_col_count; ++j) {
                        data->error_adjusted_table_data[i][j] = data->table_data_main->table[i][j] + error_e_value;
                    }
                } else {
                    data->error_adjusted_table_data[i] = NULL;
                }
            }
            debug_print_l1("   Данные для таблицы с погрешностями E вычислены.\n");
        } else {
            debug_print_l1("   Колонка 'E' НЕ найдена в таблице 'Значения'.\n");
        }
    } else {
        debug_print_l1("   Нет данных в 'Значения' или 'table_data_main' для расчета таблицы с погрешностями.\n");
    }

    // 4. Вычисление μ' и α' для таблицы с погрешностями
    if (data->error_adjusted_table_data && main_row_count > 0) {
        data->mu_prime_data = compute_mu(data->error_adjusted_table_data, main_row_count, main_col_count);
        data->mu_prime_data_count = data->mu_prime_data ? main_row_count : 0;
        if (data->mu_prime_data) {
            data->alpha_prime_data = compute_alpha(data->error_adjusted_table_data, main_row_count, main_col_count, data->mu_prime_data);
            data->alpha_prime_data_count = data->alpha_prime_data ? main_row_count : 0;
        } else {
            data->alpha_prime_data = NULL; data->alpha_prime_data_count = 0;
        }
    } else {
        data->mu_prime_data = NULL; data->mu_prime_data_count = 0;
        data->alpha_prime_data = NULL; data->alpha_prime_data_count = 0;
    }

    // 5. ЗАПОЛНЕНИЕ ТАБЛИЦЫ С ПОГРЕШНОСТЯМИ В UI (level1_error_adj_tree_view)
    if (data->level1_error_adj_tree_view) {
        if (!data->level1_error_adj_store) { 
            int num_data_cols = main_col_count;
            // ID (int) + X1_adj (double) ... XN_adj (double) + mu_prime (double) + alpha_prime (double)
            int total_cols_in_store = 1 + num_data_cols + 2;
            GType *types = g_new(GType, total_cols_in_store);
            types[0] = G_TYPE_INT; 
            for (int k = 0; k < num_data_cols; ++k) types[k + 1] = G_TYPE_DOUBLE;
            types[num_data_cols + 1] = G_TYPE_DOUBLE; 
            types[num_data_cols + 2] = G_TYPE_DOUBLE; 
            
            data->level1_error_adj_store = gtk_list_store_newv(total_cols_in_store, types);
            g_free(types);
            gtk_tree_view_set_model(GTK_TREE_VIEW(data->level1_error_adj_tree_view), GTK_TREE_MODEL(data->level1_error_adj_store));
            // g_object_unref(data->level1_error_adj_store); // Не нужно, если мы храним указатель в AppData

            GtkCellRenderer *renderer; GtkTreeViewColumn *col;
            renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
            const char* id_col_title = (data->table_data_main && data->table_data_main->column_names && data->table_data_main->column_names[0]) ?
                                       data->table_data_main->column_names[0] : "Эпоха";
            col = gtk_tree_view_column_new_with_attributes(id_col_title, renderer, "text", 0, NULL);
            gtk_tree_view_column_set_fixed_width(col, 60);
            gtk_tree_view_append_column(GTK_TREE_VIEW(data->level1_error_adj_tree_view), col);

            if (data->table_data_main && data->table_data_main->column_names) {
                for (int k = 0; k < num_data_cols; ++k) {
                    const char* data_col_title = (data->table_data_main->column_names[k + 1]) ?
                                                 data->table_data_main->column_names[k + 1] : "Данные";
                    renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
                    col = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col, data_col_title);
                    gtk_tree_view_column_pack_start(col, renderer, TRUE);
                    gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(k + 1), NULL);
                    gtk_tree_view_column_set_fixed_width(col, 80);
                    gtk_tree_view_append_column(GTK_TREE_VIEW(data->level1_error_adj_tree_view), col);
                }
            }
            renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col, "μ'");
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(num_data_cols + 1), NULL);
            g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1));
            gtk_tree_view_column_set_fixed_width(col, 80);
            gtk_tree_view_append_column(GTK_TREE_VIEW(data->level1_error_adj_tree_view), col);

            renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col, "α'");
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(num_data_cols + 2), NULL);
            g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1));
            gtk_tree_view_column_set_fixed_width(col, 100);
            gtk_tree_view_append_column(GTK_TREE_VIEW(data->level1_error_adj_tree_view), col);
            debug_print_l1("   Store и колонки (включая mu', alpha') для level1_error_adj_tree_view созданы.\n");
        }

        if (data->level1_error_adj_store) {
            gtk_list_store_clear(data->level1_error_adj_store);
            if (data->table_data_main && main_row_count > 0) { // Используем table_data_main для эпох
                for (int i = 0; i < main_row_count; ++i) {
                    GtkTreeIter iter;
                    gtk_list_store_append(data->level1_error_adj_store, &iter);
                    gtk_list_store_set(data->level1_error_adj_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
                    if (data->error_adjusted_table_data && data->error_adjusted_table_data[i]) {
                        for (int j = 0; j < main_col_count; ++j) {
                            gtk_list_store_set(data->level1_error_adj_store, &iter, j + 1, data->error_adjusted_table_data[i][j], -1);
                        }
                    } else {
                        for (int j = 0; j < main_col_count; ++j) { // Заполняем NAN, если нет данных error_adjusted
                            gtk_list_store_set(data->level1_error_adj_store, &iter, j + 1, NAN, -1);
                        }
                    }
                    gtk_list_store_set(data->level1_error_adj_store, &iter, main_col_count + 1, (data->mu_prime_data && i < data->mu_prime_data_count) ? data->mu_prime_data[i] : NAN, -1);
                    gtk_list_store_set(data->level1_error_adj_store, &iter, main_col_count + 2, (data->alpha_prime_data && i < data->alpha_prime_data_count) ? data->alpha_prime_data[i] : NAN, -1);
                }
                debug_print_l1("   Таблица с погрешностями E (и mu', alpha') заполнена в UI (%d строк).\n", main_row_count);
            }
        }
    }
    
    // 6. Расчеты и заполнение таблиц сглаживания mu (обычного) и alpha (обычного)
    double coefficients[] = {0.1, 0.4, 0.7, 0.9}; int coeff_count = sizeof(coefficients) / sizeof(coefficients[0]);
    if (data->level1_smoothing_store && data->mu_data && main_row_count > 0) {
        data->smoothing_data = compute_smoothing(data->mu_data, main_row_count, &data->smoothing_row_count, coefficients, coeff_count);
        if (data->smoothing_data && data->smoothing_row_count > 0) {
            gtk_list_store_clear(data->level1_smoothing_store);
            for (int i = 0; i < data->smoothing_row_count; i++) { GtkTreeIter iter; gtk_list_store_append(data->level1_smoothing_store, &iter);
                gtk_list_store_set(data->level1_smoothing_store, &iter, 0, i, 1, (data->smoothing_data[i]) ? data->smoothing_data[i][0] : NAN, 2, (data->smoothing_data[i]) ? data->smoothing_data[i][1] : NAN, 3, (data->smoothing_data[i]) ? data->smoothing_data[i][2] : NAN, 4, (data->smoothing_data[i]) ? data->smoothing_data[i][3] : NAN, 5, (i < main_row_count) ? data->mu_data[i] : NAN, -1);
            }
        } else if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
    }
    if (data->level1_alpha_smoothing_store && data->alpha_data && data->alpha_count > 1) {
        data->smoothing_alpha_data = compute_smoothing(data->alpha_data + 1, data->alpha_count - 1, &data->smoothing_alpha_row_count, coefficients, coeff_count);
        if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
            gtk_list_store_clear(data->level1_alpha_smoothing_store);
            for (int i = 0; i < data->smoothing_alpha_row_count; i++) { GtkTreeIter iter; gtk_list_store_append(data->level1_alpha_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_smoothing_store, &iter, 0, i, 1, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][0] : NAN, 2, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][1] : NAN, 3, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][2] : NAN, 4, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][3] : NAN, 5, (i + 1 < data->alpha_count) ? data->alpha_data[i + 1] : NAN, -1);
            }
        } else if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
    }

    // 7. Сглаживание первой колонки X0+E
    if (data->level1_error_adj_smoothing_store && data->error_adjusted_table_data &&
        main_row_count > 0 && main_col_count > 0) {
        
        double *x0_plus_e_series = g_new(double, main_row_count);
        int valid_x0_count = 0;
        for (int i = 0; i < main_row_count; ++i) {
            if (data->error_adjusted_table_data[i] != NULL) {
                x0_plus_e_series[valid_x0_count++] = data->error_adjusted_table_data[i][0];
            }
        }

        if (valid_x0_count > 0) {
            data->smoothing_error_adjusted_data_x0 = compute_smoothing(
                x0_plus_e_series, valid_x0_count,
                &data->smoothing_error_adjusted_data_x0_row_count,
                coefficients, coeff_count
            );

            if (data->smoothing_error_adjusted_data_x0 && data->smoothing_error_adjusted_data_x0_row_count > 0) {
                gtk_list_store_clear(data->level1_error_adj_smoothing_store);
                for (int i = 0; i < data->smoothing_error_adjusted_data_x0_row_count; i++) {
                    GtkTreeIter iter; gtk_list_store_append(data->level1_error_adj_smoothing_store, &iter);
                    gtk_list_store_set(data->level1_error_adj_smoothing_store, &iter,
                                      0, i, 
                                      1, (data->smoothing_error_adjusted_data_x0[i]) ? data->smoothing_error_adjusted_data_x0[i][0] : NAN,
                                      2, (data->smoothing_error_adjusted_data_x0[i]) ? data->smoothing_error_adjusted_data_x0[i][1] : NAN,
                                      3, (data->smoothing_error_adjusted_data_x0[i]) ? data->smoothing_error_adjusted_data_x0[i][2] : NAN,
                                      4, (data->smoothing_error_adjusted_data_x0[i]) ? data->smoothing_error_adjusted_data_x0[i][3] : NAN,
                                      5, (i < valid_x0_count) ? x0_plus_e_series[i] : NAN,
                                      -1);
                }
                debug_print_l1("   Таблица сглаживания X0+E заполнена (%d строк).\n", data->smoothing_error_adjusted_data_x0_row_count);
            }
        }
        g_free(x0_plus_e_series);
    } else if (data->level1_error_adj_smoothing_store) {
        gtk_list_store_clear(data->level1_error_adj_smoothing_store);
    }

// --- 8. НОВОЕ: Сглаживание μ' (мю с погрешностью E) ---
    if (data->level1_mu_prime_smoothing_store && data->mu_prime_data && data->mu_prime_data_count > 0) {
        debug_print_l1(" - Сглаживание mu_prime_data (элементов: %d)...\n", data->mu_prime_data_count);
        data->smoothing_mu_prime_data = compute_smoothing(
            data->mu_prime_data,
            data->mu_prime_data_count,
            &data->smoothing_mu_prime_row_count,
            coefficients,
            coeff_count
        );
        if (data->smoothing_mu_prime_data && data->smoothing_mu_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_mu_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_mu_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_mu_prime_smoothing_store, &iter,
                                  0, i, 
                                  1, (data->smoothing_mu_prime_data[i]) ? data->smoothing_mu_prime_data[i][0] : NAN,
                                  2, (data->smoothing_mu_prime_data[i]) ? data->smoothing_mu_prime_data[i][1] : NAN,
                                  3, (data->smoothing_mu_prime_data[i]) ? data->smoothing_mu_prime_data[i][2] : NAN,
                                  4, (data->smoothing_mu_prime_data[i]) ? data->smoothing_mu_prime_data[i][3] : NAN,
                                  5, (i < data->mu_prime_data_count) ? data->mu_prime_data[i] : NAN, // Реальное μ'
                                  -1);
            }
            debug_print_l1("   Таблица сглаживания mu_prime заполнена (%d строк).\n", data->smoothing_mu_prime_row_count);
        } else if (data->level1_mu_prime_smoothing_store) {
             gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
        }
    } else if (data->level1_mu_prime_smoothing_store) {
        gtk_list_store_clear(data->level1_mu_prime_smoothing_store);
    }

    // --- 9. НОВОЕ: Сглаживание α' (альфа с погрешностью E) ---
    if (data->level1_alpha_prime_smoothing_store && data->alpha_prime_data && data->alpha_prime_data_count > 1) {
        debug_print_l1(" - Сглаживание alpha_prime_data (элементов для сглаживания: %d)...\n", data->alpha_prime_data_count - 1);
        data->smoothing_alpha_prime_data = compute_smoothing(
            data->alpha_prime_data + 1, // Начинаем со второго элемента, т.к. alpha_prime_data[0] = 0
            data->alpha_prime_data_count - 1,
            &data->smoothing_alpha_prime_row_count,
            coefficients,
            coeff_count
        );
        if (data->smoothing_alpha_prime_data && data->smoothing_alpha_prime_row_count > 0) {
            gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
            for (int i = 0; i < data->smoothing_alpha_prime_row_count; i++) {
                GtkTreeIter iter; gtk_list_store_append(data->level1_alpha_prime_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_prime_smoothing_store, &iter,
                                  0, i,
                                  1, (data->smoothing_alpha_prime_data[i]) ? data->smoothing_alpha_prime_data[i][0] : NAN,
                                  2, (data->smoothing_alpha_prime_data[i]) ? data->smoothing_alpha_prime_data[i][1] : NAN,
                                  3, (data->smoothing_alpha_prime_data[i]) ? data->smoothing_alpha_prime_data[i][2] : NAN,
                                  4, (data->smoothing_alpha_prime_data[i]) ? data->smoothing_alpha_prime_data[i][3] : NAN,
                                  5, (i + 1 < data->alpha_prime_data_count) ? data->alpha_prime_data[i + 1] : NAN, // Реальное alpha_prime[i+1]
                                  -1);
            }
            debug_print_l1("   Таблица сглаживания alpha_prime заполнена (%d строк).\n", data->smoothing_alpha_prime_row_count);
        } else if (data->level1_alpha_prime_smoothing_store) {
            gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
        }
    } else if (data->level1_alpha_prime_smoothing_store) {
        gtk_list_store_clear(data->level1_alpha_prime_smoothing_store);
    }


    // 10. Запись в файл
    if (data->mu_data && data->alpha_data && data->table_data_main && data->table_data_main->epochs) {
        if (!write_results_to_file("results.txt", data->mu_data, main_row_count, data->alpha_data, data->alpha_count, data->smoothing_data, data->smoothing_row_count, data->smoothing_alpha_data, data->smoothing_alpha_row_count, data->table_data_main->epochs, main_row_count)) {
            if(data->level1_window) show_message_dialog(GTK_WINDOW(data->level1_window), "Ошибка записи результатов в файл results.txt");
        }
    }

    // 11. Перерисовка графиков
    if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
    if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
    if (data->level1_error_adj_drawing_area) gtk_widget_queue_draw(data->level1_error_adj_drawing_area);
    if (data->level1_mu_prime_drawing_area) gtk_widget_queue_draw(data->level1_mu_prime_drawing_area);
    if (data->level1_alpha_prime_drawing_area) gtk_widget_queue_draw(data->level1_alpha_prime_drawing_area);

    debug_print_l1("level1_perform_calculations_and_update_ui: ЗАВЕРШЕНО\n");
}
static void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    double **graph_smoothing_data = NULL;
    int graph_smoothing_row_count = 0;
    double *real_plot_data = NULL;
    int real_plot_data_count = 0;
    const char *y_axis_label_str = "Значение";
    const char *title_str_graph = "График сглаживания";
    const char *real_data_legend_label = "Реальные данные";
    gboolean format_as_alpha = FALSE; // Для специального форматирования оси Y (E-нотация)
    GArray *temp_real_data_array_for_x0e = NULL; // Для временного хранения данных X0+E

    // Определяем, какой график рисовать, и какие данные использовать
    if (drawing_area == GTK_DRAWING_AREA(data->level1_drawing_area)) { // Обычный mu
        graph_smoothing_data = data->smoothing_data;
        graph_smoothing_row_count = data->smoothing_row_count;
        if (data->mu_data && data->table_data_main) real_plot_data = data->mu_data;
        if (data->table_data_main) real_plot_data_count = data->table_data_main->row_count;
        y_axis_label_str = "Значение μ"; title_str_graph = "Прогнозные функции Zпр(μ)"; real_data_legend_label = "Реальные μ";
    } else if (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_drawing_area)) { // Обычный alpha
        graph_smoothing_data = data->smoothing_alpha_data;
        graph_smoothing_row_count = data->smoothing_alpha_row_count;
        if (data->alpha_data && data->alpha_count > 1) { real_plot_data = data->alpha_data + 1; real_plot_data_count = data->alpha_count - 1; }
        y_axis_label_str = "Значение α"; title_str_graph = "Прогнозные функции Zпр(α)"; real_data_legend_label = "Реальные α";
        format_as_alpha = TRUE;
    } else if (drawing_area == GTK_DRAWING_AREA(data->level1_error_adj_drawing_area)) { // Сглаживание X0+E
        graph_smoothing_data = data->smoothing_error_adjusted_data_x0;
        graph_smoothing_row_count = data->smoothing_error_adjusted_data_x0_row_count;
        if (data->error_adjusted_table_data && data->table_data_main && data->table_data_main->row_count > 0 && data->table_data_main->column_count > 0) {
            temp_real_data_array_for_x0e = g_array_new(FALSE, FALSE, sizeof(double));
            for (int i = 0; i < data->table_data_main->row_count; ++i) {
                if (data->error_adjusted_table_data[i] != NULL) {
                    g_array_append_val(temp_real_data_array_for_x0e, data->error_adjusted_table_data[i][0]);
                }
            }
            if (temp_real_data_array_for_x0e->len > 0) {
                real_plot_data = (double*)temp_real_data_array_for_x0e->data;
                real_plot_data_count = temp_real_data_array_for_x0e->len;
            }
        }
        y_axis_label_str = "Значение X0+E"; title_str_graph = "Прогнозные функции Zпр(X0+E)"; real_data_legend_label = "Реальные X0+E";
    } else if (drawing_area == GTK_DRAWING_AREA(data->level1_mu_prime_drawing_area)) { // mu_prime
        graph_smoothing_data = data->smoothing_mu_prime_data;
        graph_smoothing_row_count = data->smoothing_mu_prime_row_count;
        if (data->mu_prime_data) { real_plot_data = data->mu_prime_data; real_plot_data_count = data->mu_prime_data_count; }
        y_axis_label_str = "Значение μ'"; title_str_graph = "Прогнозные функции Zпр(μ')"; real_data_legend_label = "Реальные μ'";
    } else if (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_prime_drawing_area)) { // alpha_prime
        graph_smoothing_data = data->smoothing_alpha_prime_data;
        graph_smoothing_row_count = data->smoothing_alpha_prime_row_count;
        if (data->alpha_prime_data && data->alpha_prime_data_count > 1) { real_plot_data = data->alpha_prime_data + 1; real_plot_data_count = data->alpha_prime_data_count - 1; }
        y_axis_label_str = "Значение α'"; title_str_graph = "Прогнозные функции Zпр(α')"; real_data_legend_label = "Реальные α'";
        format_as_alpha = TRUE;
    } else {
        debug_print_l1("level1_draw_graph: Неизвестная DrawingArea (%p)!\n", (void*)drawing_area);
        // Отрисовка сообщения об ошибке или просто выход
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr); cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(cr, 14);
        cairo_text_extents_t extents; const char* err_msg = "Ошибка: Неизвестный график";
        cairo_text_extents(cr, err_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height - extents.height) / 2.0 + extents.height);
        cairo_show_text(cr, err_msg);
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
        char no_data_msg[100];
        snprintf(no_data_msg, sizeof(no_data_msg), "Нет данных для графика: %s", title_str_graph);
        cairo_text_extents(cr, no_data_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height - extents.height) / 2.0 + extents.height);
        cairo_show_text(cr, no_data_msg);
        if (temp_real_data_array_for_x0e) g_array_free(temp_real_data_array_for_x0e, TRUE);
        return;
    }

    double margin_left = 70.0;
    double margin_right = 20.0;
    double margin_top = 40.0;
    double margin_bottom = 90.0;
    double plot_width = width - margin_left - margin_right;
    double plot_height = height - margin_top - margin_bottom;

    if (plot_width <= 1 || plot_height <= 1) {
        if (temp_real_data_array_for_x0e) g_array_free(temp_real_data_array_for_x0e, TRUE);
        return;
    }

    double y_min_val = G_MAXDOUBLE, y_max_val = -G_MAXDOUBLE;
    gboolean data_found_for_yrange = FALSE;

    if(has_smoothing_data){
        for (int i = 0; i < graph_smoothing_row_count; i++) {
            if (graph_smoothing_data[i] == NULL) continue;
            for (int j_idx = 0; j_idx < 4; j_idx++) { // Только первые 4 - сглаженные данные A=0.1 до A=0.9
                if (isnan(graph_smoothing_data[i][j_idx])) continue;
                if (graph_smoothing_data[i][j_idx] < y_min_val) y_min_val = graph_smoothing_data[i][j_idx];
                if (graph_smoothing_data[i][j_idx] > y_max_val) y_max_val = graph_smoothing_data[i][j_idx];
                data_found_for_yrange = TRUE;
            }
        }
    }
    if(has_real_data){
        for (int i = 0; i < real_plot_data_count; i++) {
            if (isnan(real_plot_data[i])) continue;
            if (real_plot_data[i] < y_min_val) y_min_val = real_plot_data[i];
            if (real_plot_data[i] > y_max_val) y_max_val = real_plot_data[i];
            data_found_for_yrange = TRUE;
        }
    }

    if (!data_found_for_yrange) { y_min_val = -1.0; y_max_val = 1.0; }
    double y_range_val_calc = y_max_val - y_min_val;
    if (fabs(y_range_val_calc) < 1e-9) { 
        double abs_val = fabs(y_min_val); 
        if (abs_val < 1e-9) abs_val = 1.0; 
        y_min_val -= 0.5 * abs_val; y_max_val += 0.5 * abs_val; 
        if (fabs(y_max_val - y_min_val) < 1e-9) { y_min_val = -1.0; y_max_val = 1.0; }
    } else { 
        y_min_val -= y_range_val_calc * 0.1; y_max_val += y_range_val_calc * 0.1; 
    }
    y_range_val_calc = y_max_val - y_min_val; 
    if (fabs(y_range_val_calc) < 1e-9) y_range_val_calc = 1.0;

    int x_num_points_axis = 1; // Минимальное количество точек на оси X (для M=0)
    int max_m_on_axis = 0;
    if (has_smoothing_data && graph_smoothing_row_count > 0) {
        max_m_on_axis = graph_smoothing_row_count - 1; // M идет от 0 до N (N+1 точек), макс M = N
    }
    if (has_real_data && real_plot_data_count > 0) {
        if ((real_plot_data_count - 1) > max_m_on_axis) { // Реальные данные M от 0 до N-1
            max_m_on_axis = real_plot_data_count - 1;
        }
    }
    x_num_points_axis = max_m_on_axis + 1; // Количество тиков на оси = макс_M + 1
    if (x_num_points_axis <= 0) x_num_points_axis = 1; // Защита

    double x_max_ticks_value = max_m_on_axis; // Максимальное значение M на оси
    double x_scale = (x_max_ticks_value > 1e-9) ? (plot_width / x_max_ticks_value) : plot_width;
    if (x_num_points_axis == 1) x_scale = plot_width; // Для одной точки M=0
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
            if (x_num_points_axis == 1) x_pos = margin_left + plot_width / 2.0; // Центрируем одну метку M=0
            else if (i == x_max_ticks_value && x_max_ticks_value == 0) x_pos = margin_left; // Если только M=0, то слева
            
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

    double colors[5][3] = {{1.0,0.0,0.0},{0.0,0.7,0.0},{0.0,0.0,1.0},{0.8,0.6,0.0},{0.3,0.3,0.3}}; 
    const char *legend_labels_A[] = {"A=0.1","A=0.4","A=0.7","A=0.9"};

    if(has_smoothing_data){ 
        for (int j_color = 0; j_color < 4; j_color++) { 
            cairo_set_source_rgb(cr, colors[j_color][0], colors[j_color][1], colors[j_color][2]); 
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
        cairo_set_source_rgb(cr, colors[color_idx][0], colors[color_idx][1], colors[color_idx][2]); 
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
    for (int j = 0; j < 4; j++) { // Сначала 4 линии сглаживания
        if (!has_smoothing_data) continue;
        if (legend_x_current + legend_item_width > width - margin_right) { legend_y_current += 15; legend_x_current = legend_x_start; if (legend_x_current + legend_item_width > width - margin_right) break; } 
        cairo_set_source_rgb(cr, colors[j][0], colors[j][1], colors[j][2]); 
        cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10); cairo_fill(cr); 
        cairo_set_source_rgb(cr, 0, 0, 0); 
        cairo_text_extents_t text_ext_legend; cairo_text_extents(cr, legend_labels_A[j], &text_ext_legend); 
        cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1); cairo_show_text(cr, legend_labels_A[j]); 
        legend_x_current += legend_item_width; 
    }
    if (has_real_data) { // Затем линия реальных данных
        if (legend_x_current + legend_item_width > width - margin_right) { legend_y_current += 15; legend_x_current = legend_x_start; }
        if (legend_x_current + legend_item_width <= width - margin_right) { // Проверка, что еще есть место
            cairo_set_source_rgb(cr, colors[4][0], colors[4][1], colors[4][2]); 
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

    if (temp_real_data_array_for_x0e) {
        g_array_free(temp_real_data_array_for_x0e, TRUE);
    }
    // debug_print_l1("level1_draw_graph: ЗАВЕРШЕНИЕ отрисовки для %s\n", title_str_graph);
}

// В файле level1_analysis.c

static void level1_draw_error_adj_x0_smoothing_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    debug_print_l1("level1_draw_error_adj_x0_smoothing_graph: НАЧАЛО отрисовки\n");

    double **graph_smoothing_data = data->smoothing_error_adjusted_data_x0;
    int graph_smoothing_row_count = data->smoothing_error_adjusted_data_x0_row_count;

    double *real_plot_data_series = NULL;
    int real_plot_data_count = 0;
    GArray *temp_real_data_array = NULL; 

    if (data->error_adjusted_table_data && data->table_data_main && data->table_data_main->row_count > 0 && data->table_data_main->column_count > 0) {
        temp_real_data_array = g_array_new(FALSE, FALSE, sizeof(double));
        for (int i = 0; i < data->table_data_main->row_count; ++i) {
            if (data->error_adjusted_table_data[i] != NULL) {
                double val = data->error_adjusted_table_data[i][0]; 
                g_array_append_val(temp_real_data_array, val);
            }
        }
        if (temp_real_data_array->len > 0) {
            real_plot_data_series = (double*)temp_real_data_array->data;
            real_plot_data_count = temp_real_data_array->len;
        }
    }

    gboolean has_smoothing_data = graph_smoothing_data && graph_smoothing_row_count > 0;
    gboolean has_real_data = real_plot_data_series && real_plot_data_count > 0;

    if (!has_smoothing_data && !has_real_data) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr); cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(cr, 14);
        cairo_text_extents_t extents; const char* no_data_msg = "Нет данных для графика X0+E";
        cairo_text_extents(cr, no_data_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height - extents.height) / 2.0 + extents.height);
        cairo_show_text(cr, no_data_msg);
        if (temp_real_data_array) g_array_free(temp_real_data_array, TRUE);
        return;
    }
    
    double margin_left = 70.0, margin_right = 20.0, margin_top = 40.0, margin_bottom = 90.0;
    double plot_width = width - margin_left - margin_right; double plot_height = height - margin_top - margin_bottom;
    if (plot_width <= 1 || plot_height <= 1) { if (temp_real_data_array) g_array_free(temp_real_data_array, TRUE); return; }

    double y_min_val = G_MAXDOUBLE, y_max_val = -G_MAXDOUBLE; gboolean data_found_for_yrange = FALSE;
    if(has_smoothing_data){ for (int i = 0; i < graph_smoothing_row_count; i++) { if (graph_smoothing_data[i] == NULL) continue; for (int j = 0; j < 4; j++) { if (isnan(graph_smoothing_data[i][j])) continue; if (graph_smoothing_data[i][j] < y_min_val) y_min_val = graph_smoothing_data[i][j]; if (graph_smoothing_data[i][j] > y_max_val) y_max_val = graph_smoothing_data[i][j]; data_found_for_yrange = TRUE; } } }
    if(has_real_data){ for (int i = 0; i < real_plot_data_count; i++) { if (isnan(real_plot_data_series[i])) continue; if (real_plot_data_series[i] < y_min_val) y_min_val = real_plot_data_series[i]; if (real_plot_data_series[i] > y_max_val) y_max_val = real_plot_data_series[i]; data_found_for_yrange = TRUE; } }
    if (!data_found_for_yrange) { y_min_val = -1.0; y_max_val = 1.0; }
    double y_range_val_calc = y_max_val - y_min_val;
    if (fabs(y_range_val_calc) < 1e-9) { double abs_val = fabs(y_min_val); if (abs_val < 1e-9) abs_val = 1.0; y_min_val -= 0.5 * abs_val; y_max_val += 0.5 * abs_val; if (fabs(y_max_val - y_min_val) < 1e-9) { y_min_val = -1.0; y_max_val = 1.0; }
    } else { y_min_val -= y_range_val_calc * 0.1; y_max_val += y_range_val_calc * 0.1; }
    y_range_val_calc = y_max_val - y_min_val; if (fabs(y_range_val_calc) < 1e-9) y_range_val_calc = 1.0;

    int x_num_points_axis = 1;
    if(has_smoothing_data) x_num_points_axis = graph_smoothing_row_count;
    if(has_real_data) { int max_real_m = real_plot_data_count > 0 ? real_plot_data_count -1 : -1; int max_smooth_m = graph_smoothing_row_count > 0 ? graph_smoothing_row_count -1 : -1; int max_m = max_real_m > max_smooth_m ? max_real_m : max_smooth_m; if(max_m >=0) x_num_points_axis = max_m +1; }
    if (x_num_points_axis == 0) x_num_points_axis = 1;
    
    double x_max_ticks = x_num_points_axis > 1 ? (x_num_points_axis - 1) : 1.0;
    double x_scale = (x_max_ticks > 1e-9) ? (plot_width / x_max_ticks) : plot_width;
    if (x_num_points_axis == 1 && fabs(x_max_ticks) < 1e-9) x_scale = plot_width;
    double y_scale = plot_height / y_range_val_calc;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, margin_left, height - margin_bottom); cairo_line_to(cr, margin_left + plot_width, height - margin_bottom); cairo_stroke(cr);
    cairo_move_to(cr, margin_left, margin_top); cairo_line_to(cr, margin_left, margin_top + plot_height); cairo_stroke(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); cairo_set_font_size(cr, 10);
    for (int i = 0; i < x_num_points_axis; i++) { int tick_frequency = 1; if (x_num_points_axis > 15) tick_frequency = (int)ceil((double)x_num_points_axis / 10.0); if (i == 0 || i == (x_num_points_axis -1) || (i % tick_frequency == 0)) { double x_pos = margin_left + i * x_scale; if (x_num_points_axis == 1) x_pos = margin_left + plot_width / 2; char label[16]; snprintf(label, sizeof(label), "%d", i); cairo_text_extents_t extents; cairo_text_extents(cr, label, &extents); cairo_move_to(cr, x_pos - extents.width / 2.0, height - margin_bottom + 15); cairo_show_text(cr, label); cairo_move_to(cr, x_pos, height - margin_bottom); cairo_line_to(cr, x_pos, height - margin_bottom + 3); cairo_stroke(cr); } }
    const char *x_axis_label_str = "M (период)"; cairo_text_extents_t x_label_extents_calc; cairo_text_extents(cr, x_axis_label_str, &x_label_extents_calc); cairo_move_to(cr, margin_left + plot_width / 2.0 - x_label_extents_calc.width / 2.0, height - margin_bottom + 35); cairo_show_text(cr, x_axis_label_str);
    int num_y_ticks = 5; for (int i = 0; i <= num_y_ticks; i++) { double y_value = y_min_val + (y_range_val_calc * i / num_y_ticks); double y_pos = height - margin_bottom - (y_value - y_min_val) * y_scale; char label[32]; snprintf(label, sizeof(label), "%.2f", y_value); cairo_text_extents_t extents; cairo_text_extents(cr, label, &extents); cairo_move_to(cr, margin_left - extents.width - 5, y_pos + extents.height / 2.0); cairo_show_text(cr, label); cairo_move_to(cr, margin_left - 3, y_pos); cairo_line_to(cr, margin_left, y_pos); cairo_stroke(cr); }
    const char *y_axis_label_str = "Значение X0+E"; cairo_text_extents_t y_label_extents_val; cairo_text_extents(cr, y_axis_label_str, &y_label_extents_val); cairo_save(cr); cairo_move_to(cr, margin_left - 55, margin_top + plot_height / 2.0 + y_label_extents_val.width / 2.0); cairo_rotate(cr, -M_PI / 2.0); cairo_show_text(cr, y_axis_label_str); cairo_restore(cr);
    double colors[5][3] = {{1.0,0.0,0.0},{0.0,0.7,0.0},{0.0,0.0,1.0},{0.8,0.6,0.0},{0.3,0.3,0.3}}; const char *legend_labels[] = {"A=0.1","A=0.4","A=0.7","A=0.9","Реальные X0+E"};
    if(has_smoothing_data){ for (int j_color = 0; j_color < 4; j_color++) { cairo_set_source_rgb(cr, colors[j_color][0], colors[j_color][1], colors[j_color][2]); cairo_set_line_width(cr, 1.5); gboolean first = TRUE; for (int i = 0; i < graph_smoothing_row_count; i++) { if (graph_smoothing_data[i] == NULL) {first=TRUE; continue;} double val_y = graph_smoothing_data[i][j_color]; if (isnan(val_y)||isinf(val_y)) {first=TRUE; continue;} double x_pos = margin_left + i * x_scale; if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2; double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; if(y_pos < margin_top) y_pos=margin_top; if(y_pos > height-margin_bottom) y_pos=height-margin_bottom; if(first){cairo_move_to(cr,x_pos,y_pos);first=FALSE;}else{cairo_line_to(cr,x_pos,y_pos);}} if(!first)cairo_stroke(cr); for (int i = 0; i < graph_smoothing_row_count; i++) { if (graph_smoothing_data[i] == NULL) continue; double val_y = graph_smoothing_data[i][j_color]; if(isnan(val_y)||isinf(val_y))continue; double x_pos = margin_left + i * x_scale; if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2; double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; if(y_pos<margin_top-3||y_pos>height-margin_bottom+3)continue; cairo_arc(cr,x_pos,y_pos,2.2,0,2*M_PI);cairo_fill(cr);}}}
    if(has_real_data){ int color_idx = 4; cairo_set_source_rgb(cr, colors[color_idx][0], colors[color_idx][1], colors[color_idx][2]); cairo_set_line_width(cr, 2.0); gboolean first = TRUE; for(int i = 0; i < real_plot_data_count; ++i){ int m_val = i; double val_y = real_plot_data_series[i]; if(isnan(val_y)||isinf(val_y)){first=TRUE;continue;} double x_pos = margin_left + m_val * x_scale; if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2; double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; if(y_pos < margin_top) y_pos=margin_top; if(y_pos > height-margin_bottom) y_pos=height-margin_bottom; if(first){cairo_move_to(cr,x_pos,y_pos);first=FALSE;}else{cairo_line_to(cr,x_pos,y_pos);}} if(!first)cairo_stroke(cr); for(int i = 0; i < real_plot_data_count; ++i){ int m_val = i; double val_y = real_plot_data_series[i]; if(isnan(val_y)||isinf(val_y))continue; double x_pos = margin_left + m_val * x_scale; if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2; double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; if(y_pos<margin_top-3||y_pos>height-margin_bottom+3)continue; cairo_arc(cr,x_pos,y_pos,2.8,0,2*M_PI);cairo_fill(cr);}}
    double legend_item_width = 110; double legend_x_start = margin_left; double legend_y_start = height - margin_bottom + 50; double legend_x_current = legend_x_start; double legend_y_current = legend_y_start; cairo_set_font_size(cr, 10);
    for (int j = 0; j < 5; j++) { if (j < 4 && !has_smoothing_data) continue; if (j == 4 && !has_real_data) continue; if (legend_x_current + legend_item_width > width - margin_right) { legend_y_current += 15; legend_x_current = legend_x_start; if (legend_x_current + legend_item_width > width - margin_right) break; } cairo_set_source_rgb(cr, colors[j][0], colors[j][1], colors[j][2]); cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10); cairo_fill(cr); cairo_set_source_rgb(cr, 0, 0, 0); cairo_text_extents_t text_ext_legend; cairo_text_extents(cr, legend_labels[j], &text_ext_legend); cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1); cairo_show_text(cr, legend_labels[j]); legend_x_current += legend_item_width; }
    const char *title_str_graph = "Прогнозные функции Zпр(X0+E)"; cairo_set_font_size(cr, 14); cairo_text_extents_t title_extents_val_graph; cairo_text_extents(cr, title_str_graph, &title_extents_val_graph); cairo_move_to(cr, margin_left + plot_width / 2.0 - title_extents_val_graph.width / 2.0, margin_top - 15); cairo_show_text(cr, title_str_graph);

    if (temp_real_data_array) {
        g_array_free(temp_real_data_array, TRUE);
    }
    debug_print_l1("level1_draw_error_adj_x0_smoothing_graph: ЗАВЕРШЕНИЕ отрисовки\n");
}

static void level1_on_window_destroy(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    debug_print_l1("level1_on_window_destroy: Окно уровня 1 закрывается.\n");

    if (data->level1_window) { // Дополнительная проверка, хотя обычно не нужна, если сигнал подключен к окну
      g_object_set_data(G_OBJECT(data->level1_window), "main_scroller_l1", NULL); // Очищаем пользовательские данные
    }

    // Обнуляем указатели на виджеты окна L1
    data->level1_window = NULL;
    // data->level1_container = NULL; // Если это поле еще используется, иначе можно удалить

    // Основная таблица L1
    data->level1_tree_view = NULL;
    if (data->level1_store) { // Store может быть уже unref-нут GtkTreeView, но указатель в AppData остался
        // g_object_unref(data->level1_store); // Не нужно, если он привязан к TreeView и будет уничтожен с ним
        data->level1_store = NULL;
    }

    // Сглаживание mu
    data->level1_smoothing_view = NULL;
    if (data->level1_smoothing_store) data->level1_smoothing_store = NULL;
    data->level1_drawing_area = NULL;

    // Сглаживание alpha
    data->level1_alpha_smoothing_view = NULL;
    if (data->level1_alpha_smoothing_store) data->level1_alpha_smoothing_store = NULL;
    data->level1_alpha_drawing_area = NULL;

    // Таблица X+E
    data->level1_error_adj_tree_view = NULL;
    if (data->level1_error_adj_store) data->level1_error_adj_store = NULL;

    // Сглаживание X0+E
    data->level1_error_adj_smoothing_view = NULL;
    if (data->level1_error_adj_smoothing_store) data->level1_error_adj_smoothing_store = NULL;
    data->level1_error_adj_drawing_area = NULL;

    // НОВОЕ: Сглаживание mu_prime
    data->level1_mu_prime_smoothing_view = NULL;
    if (data->level1_mu_prime_smoothing_store) data->level1_mu_prime_smoothing_store = NULL;
    data->level1_mu_prime_drawing_area = NULL;

    // НОВОЕ: Сглаживание alpha_prime
    data->level1_alpha_prime_smoothing_view = NULL;
    if (data->level1_alpha_prime_smoothing_store) data->level1_alpha_prime_smoothing_store = NULL;
    data->level1_alpha_prime_drawing_area = NULL;

    // Очищаем все вычисленные данные Уровня 1
    level1_clear_internal_data(data);

    debug_print_l1("level1_on_window_destroy: Обнуление указателей и очистка данных завершена.\n");
}

void level1_update_content_if_visible(AppData *data) {
    if (data && data->level1_window && gtk_widget_get_visible(data->level1_window)) {
        debug_print_l1("level1_update_content_if_visible: Окно L1 видимо, запускаем обновление...\n");
        level1_perform_calculations_and_update_ui(data);
    }
}

void level1_cleanup_calculated_data(AppData *data) {
    if (data) {
        level1_clear_internal_data(data);
    }
}

#pragma GCC diagnostic pop