#include <gtk/gtk.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <stdarg.h>
#include <sqlite3.h> // Включен для полноты, хотя AppData требует его косвенно

#include "calculations.h" // Функции расчетов

// Включаем заголовок, который теперь содержит ОПРЕДЕЛЕНИЕ AppData и прототипы функций Уровня 1
#include "level1_analysis.h"
// sqlite_utils.h включается через level1_analysis.h, так как AppData содержит TableData

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types" // Временно для GtkListStore/TreeView

// Логгер main.c
static FILE *debug_log = NULL;

// ========= ОПРЕДЕЛЕНИЕ AppData УДАЛЕНО ОТСЮДА ==========
// ========= Оно теперь в level1_analysis.h    ==========


// === Forward объявления функций main.c ===
// Функции, объявленные в level1_analysis.h, НЕ НУЖНО здесь повторно объявлять
// extern void level1_show_window(AppData *data); // Уже в level1_analysis.h
// extern void level1_update_content_if_visible(AppData *data); // Уже в level1_analysis.h
// extern void level1_cleanup_calculated_data(AppData *data); // Уже в level1_analysis.h

// НЕ-СТАТИЧЕСКИЕ (доступны из level1_analysis.c, прототипы ДОЛЖНЫ БЫТЬ до их вызова,
// но т.к. level1_analysis.h включен, этого может быть достаточно. Явное объявление тут
// не помешает для ясности и если level1_analysis.h не включает эти прототипы).
// На практике, level1_analysis.c использует extern для них.
void update_tree_view_columns(AppData *data,
                              gboolean is_main_table_context,             // Для data->tree_view_main
                              gboolean is_level1_main_context,            // Для data->level1_tree_view (исходные X, mu, alpha)
                              gboolean is_level1_error_adj_context,       // Для data->level1_error_adj_tree_view (X+E, mu', alpha')
                              gboolean is_level1_error_sub_context,       // Для data->level1_error_sub_tree_view (X-E, mu'', alpha'')
                              gboolean is_smoothing_mu_table,             // Для сглаживания mu
                              gboolean is_smoothing_alpha_table,          // Для сглаживания alpha
                              gboolean is_smoothing_mu_prime_table,       // Для сглаживания mu'
                              gboolean is_smoothing_alpha_prime_table,    // Для сглаживания alpha'
                              gboolean is_smoothing_mu_double_prime_table,// Для сглаживания mu''
                              gboolean is_smoothing_alpha_double_prime_table // Для сглаживания alpha''
                             );

void cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
void show_message_dialog(GtkWindow *parent, const char *message);

// СТАТИЧЕСКИЕ (только для main.c)
static void open_sqlite_file_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void load_and_display_main_table(AppData *data, const char* table_name_to_load);
static void on_level_clicked(GtkWidget *widget, gpointer user_data);
static void add_new_row(GtkWidget *widget, gpointer user_data);
static void open_sqlite_file(GtkWidget *widget, gpointer user_data);
static void activate(GtkApplication *app, gpointer user_data);
static void update_values_table_view(AppData *data);
static void load_and_display_schema_image_from_values_table(AppData *data);
static void on_cell_edited(GtkCellRendererText *renderer, gchar *path_string, gchar *new_text, gpointer user_data);
static void init_debug_log();
static void debug_print(const char *format, ...);

static void on_edit_row_clicked(GtkWidget *widget, gpointer user_data);
static void on_main_table_selection_changed(GtkTreeSelection *selection, gpointer user_data);

static void on_values_table_selection_changed(GtkTreeSelection *selection, gpointer user_data);
static void on_edit_values_row_clicked(GtkWidget *widget, gpointer user_data);
static void show_values_edit_dialog(AppData *data, GtkTreeModel *model, GtkTreeIter *iter);
static void on_values_edit_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void free_values_edit_dialog_context(gpointer data);

// --- Реализации init_debug_log, debug_print ---
static void init_debug_log() {
    if (!debug_log) {
        debug_log = fopen("debug.log", "a");
        if (!debug_log) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (main.c)\n");
        } else {
            fseek(debug_log, 0, SEEK_END);
            if (ftell(debug_log) == 0) { // Если файл пуст
                fprintf(debug_log, "\xEF\xBB\xBF"); // UTF-8 BOM
            }
            // Отладочное сообщение, что лог открыт (можно убрать)
            // fprintf(debug_log, "Лог-файл открыт (main.c)\n");
            // fflush(debug_log);
        }
    }
}

static void debug_print(const char *format, ...) {
    init_debug_log();
    if (debug_log) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log, format, args);
        va_end(args);
        fflush(debug_log);
    }
}

// --- Реализация show_message_dialog (non-static) ---
void show_message_dialog(GtkWindow *parent, const char *message) {
    if (message == NULL) {
        debug_print("show_message_dialog: Попытка показать диалог с NULL сообщением.\n");
        return;
    }
    debug_print("show_message_dialog: Сообщение: \"%s\"\n", message);
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);

    if (!dialog) {
        debug_print("show_message_dialog: gtk_alert_dialog_new() вернул NULL для сообщения: \"%s\"\n", message);
        return; // Нечего показывать или освобождать
    }
    gtk_alert_dialog_set_buttons(dialog, (const char *[]){"OK", NULL});
    gtk_alert_dialog_show(dialog, parent);
    g_object_unref(dialog);
}


// --- Реализация update_tree_view_columns (non-static) ---
void update_tree_view_columns(AppData *data,
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
                             ) {
    GtkWidget *tree_view_to_update = NULL;
    GtkListStore **store_ptr_to_update = NULL;
    TableData* current_table_data_ref_for_names = data->table_data_main; // По умолчанию для имен колонок Xn
    const char *context_str_debug = "UNKNOWN_UPDATE_CTX";
    
    gboolean is_a_level1_type_table = FALSE; // Для таблиц типа L1_MAIN, L1_ErrorAdj, L1_ErrorSub
    gboolean is_a_smoothing_table = FALSE;
    gboolean use_alpha_formatting_for_smoothing = FALSE;
    gboolean use_mu_formatting_for_smoothing_real_col = FALSE;

    const char *mu_col_title_l1 = "μ";        // По умолчанию для L1_MAIN
    const char *alpha_col_title_l1 = "α";     // По умолчанию для L1_MAIN

    // --- Определение целевых TreeView и Store ---
    if (is_main_table_context) {
        tree_view_to_update = data->tree_view_main;
        store_ptr_to_update = &data->store_main;
        context_str_debug = "MAIN_TABLE";
    } else if (is_level1_main_context) {
        tree_view_to_update = data->level1_tree_view;
        store_ptr_to_update = &data->level1_store;
        context_str_debug = "L1_MAIN";
        is_a_level1_type_table = TRUE;
        // mu_col_title_l1 и alpha_col_title_l1 уже по умолчанию
    } else if (is_level1_error_adj_context) { // НОВЫЙ КОНТЕКСТ
        tree_view_to_update = data->level1_error_adj_tree_view;
        store_ptr_to_update = &data->level1_error_adj_store;
        context_str_debug = "L1_ERR_ADJ";
        is_a_level1_type_table = TRUE;
        mu_col_title_l1 = "μ'";
        alpha_col_title_l1 = "α'";
    } else if (is_level1_error_sub_context) { // НОВЫЙ КОНТЕКСТ
        tree_view_to_update = data->level1_error_sub_tree_view;
        store_ptr_to_update = &data->level1_error_sub_store;
        context_str_debug = "L1_ERR_SUB";
        is_a_level1_type_table = TRUE;
        mu_col_title_l1 = "μ''";
        alpha_col_title_l1 = "α''";
    } 
    // --- Таблицы сглаживания ---
    else if (is_smoothing_mu_table) {
        tree_view_to_update = data->level1_smoothing_view;
        store_ptr_to_update = &data->level1_smoothing_store;
        context_str_debug = "L1_SM_MU";
        is_a_smoothing_table = TRUE;
        use_mu_formatting_for_smoothing_real_col = TRUE;
    } else if (is_smoothing_alpha_table) {
        tree_view_to_update = data->level1_alpha_smoothing_view;
        store_ptr_to_update = &data->level1_alpha_smoothing_store;
        context_str_debug = "L1_SM_ALPHA";
        is_a_smoothing_table = TRUE;
        use_alpha_formatting_for_smoothing = TRUE;
    } 
    // Удалены is_smoothing_error_adj_x0_table и is_smoothing_error_sub_x0_table
    else if (is_smoothing_mu_prime_table) {
        tree_view_to_update = data->level1_mu_prime_smoothing_view;
        store_ptr_to_update = &data->level1_mu_prime_smoothing_store;
        context_str_debug = "L1_SM_MU_PRIME";
        is_a_smoothing_table = TRUE;
        use_mu_formatting_for_smoothing_real_col = TRUE;
    } else if (is_smoothing_alpha_prime_table) {
        tree_view_to_update = data->level1_alpha_prime_smoothing_view;
        store_ptr_to_update = &data->level1_alpha_prime_smoothing_store;
        context_str_debug = "L1_SM_ALPHA_PRIME";
        is_a_smoothing_table = TRUE;
        use_alpha_formatting_for_smoothing = TRUE;
    } else if (is_smoothing_mu_double_prime_table) {
        tree_view_to_update = data->level1_mu_double_prime_smoothing_view;
        store_ptr_to_update = &data->level1_mu_double_prime_smoothing_store;
        context_str_debug = "L1_SM_MU_DPRIME";
        is_a_smoothing_table = TRUE;
        use_mu_formatting_for_smoothing_real_col = TRUE;
    } else if (is_smoothing_alpha_double_prime_table) {
        tree_view_to_update = data->level1_alpha_double_prime_smoothing_view;
        store_ptr_to_update = &data->level1_alpha_double_prime_smoothing_store;
        context_str_debug = "L1_SM_ALPHA_DPRIME";
        is_a_smoothing_table = TRUE;
        use_alpha_formatting_for_smoothing = TRUE;
    } else {
        debug_print("update_tree_view_columns: Ошибка: Неизвестный контекст таблицы!\n");
        return;
    }

    if (!tree_view_to_update) {
        debug_print("update_tree_view_columns (%s): Ошибка: tree_view_to_update is NULL!\n", context_str_debug);
        return;
    }

    debug_print("update_tree_view_columns (%s): Начало. TreeView=%p\n", context_str_debug, (void*)tree_view_to_update);

    GtkTreeViewColumn *column_iter_to_remove;
    while ((column_iter_to_remove = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view_to_update), 0)) != NULL) {
        gtk_tree_view_remove_column(GTK_TREE_VIEW(tree_view_to_update), column_iter_to_remove);
    }
    debug_print("update_tree_view_columns (%s): Старые столбцы удалены.\n", context_str_debug);


    if (store_ptr_to_update && *store_ptr_to_update) {
        if (G_IS_OBJECT(*store_ptr_to_update)) {
            g_object_unref(*store_ptr_to_update);
        }
        *store_ptr_to_update = NULL;
    } else if (store_ptr_to_update == NULL) {
         debug_print("update_tree_view_columns (%s): store_ptr_to_update is NULL! Невозможно продолжить.\n", context_str_debug);
         return;
    }

    int model_column_count;
    int data_col_count_local = 0; // Количество колонок Xn
    if (current_table_data_ref_for_names) { // Используем data->table_data_main для имен и количества колонок Xn
        data_col_count_local = current_table_data_ref_for_names->column_count;
    }

    if (is_a_smoothing_table) {
        model_column_count = 6; // M, A=0.1, A=0.4, A=0.7, A=0.9, RealValue
    } else if (is_a_level1_type_table) { // L1_MAIN, L1_ERR_ADJ, L1_ERR_SUB
        model_column_count = 1 + data_col_count_local + 2; // ID, X1..XN, mu_type, alpha_type
    } else { // MAIN_TABLE в главном окне
        model_column_count = 1 + data_col_count_local; // ID, X1..XN
    }

    // Защита, если current_table_data_ref_for_names был NULL
     if (current_table_data_ref_for_names == NULL && (is_main_table_context || is_a_level1_type_table)) {
         data_col_count_local = 0; 
         if (is_a_level1_type_table) model_column_count = 1 + 0 + 2;
         else model_column_count = 1 + 0; // MAIN_TABLE
     }
    
    if (model_column_count <= 0 && !is_a_smoothing_table) { // Для сглаживания всегда 6
        debug_print("update_tree_view_columns (%s): model_column_count <= 0 (%d), нечего создавать.\n", context_str_debug, model_column_count);
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_to_update), NULL); // Устанавливаем NULL модель
        return;
    }


    GType *types = g_new(GType, model_column_count);
    types[0] = G_TYPE_INT; // Первая колонка всегда ID/M (int)
    for (int i = 1; i < model_column_count; i++) types[i] = G_TYPE_DOUBLE; // Остальные double
    *store_ptr_to_update = gtk_list_store_newv(model_column_count, types);
    g_free(types);
    debug_print("update_tree_view_columns (%s): Новый GtkListStore %p создан с %d столбцами.\n", context_str_debug, (void*)*store_ptr_to_update, model_column_count);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    // --- Колонка ID (или M для сглаживания) ---
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    const char* id_col_title_final = "M"; // По умолчанию для таблиц сглаживания
    if (!is_a_smoothing_table) { // Для L1_MAIN, L1_ERR_ADJ, L1_ERR_SUB или MAIN_TABLE
         id_col_title_final = (current_table_data_ref_for_names && current_table_data_ref_for_names->column_names && current_table_data_ref_for_names->column_names[0]) ?
                              current_table_data_ref_for_names->column_names[0] : "Эпоха";
    }
    col = gtk_tree_view_column_new_with_attributes(id_col_title_final, renderer, "text", 0, NULL);
    gtk_tree_view_column_set_fixed_width(col, 60);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_to_update), col);

    // --- Колонки данных ---
    if (is_a_smoothing_table) {
        const char *real_data_col_name_suffix = " (реальн.)";
        char full_real_col_name[50];

        if (is_smoothing_mu_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "μ%s", real_data_col_name_suffix);
        else if (is_smoothing_alpha_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "α%s", real_data_col_name_suffix);
        else if (is_smoothing_mu_prime_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "μ'%s", real_data_col_name_suffix);
        else if (is_smoothing_alpha_prime_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "α'%s", real_data_col_name_suffix);
        else if (is_smoothing_mu_double_prime_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "μ''%s", real_data_col_name_suffix);
        else if (is_smoothing_alpha_double_prime_table) snprintf(full_real_col_name, sizeof(full_real_col_name), "α''%s", real_data_col_name_suffix);
        else strcpy(full_real_col_name, "Реальн. знач."); // Запасной вариант

        const char *smoothing_titles_A[] = {"A=0.1", "A=0.4", "A=0.7", "A=0.9"};
        for (int i = 0; i < 5; i++) { // 4 колонки A + 1 колонка реальных данных
            renderer = gtk_cell_renderer_text_new(); 
            g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new(); 
            gtk_tree_view_column_set_title(col, (i < 4) ? smoothing_titles_A[i] : full_real_col_name);
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(i + 1), NULL); // model_column_index от 1 до 5
            
            gboolean current_col_is_alpha_type = use_alpha_formatting_for_smoothing; // Для всех 5 колонок, если это таблица сглаживания alpha-типа
            gboolean current_col_is_mu_type_real_col_only = (use_mu_formatting_for_smoothing_real_col && i == 4); // Только для последней колонки, если это mu-тип

            if (current_col_is_alpha_type) {
                g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1));
                gtk_tree_view_column_set_fixed_width(col, (i == 4) ? 120 : 95); // Реальные шире
            } else if (current_col_is_mu_type_real_col_only) {
                g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1));
                 gtk_tree_view_column_set_fixed_width(col, 120); // Только реальные mu форматируются
            } else {
                // Для обычных значений в таблицах сглаживания (не alpha и не реальные mu)
                gtk_tree_view_column_set_fixed_width(col, (i == 4) ? 120 : 80);
            }
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_to_update), col);
        }
    } else if (is_a_level1_type_table || is_main_table_context) { 
        // Для L1_MAIN, L1_ERR_ADJ, L1_ERR_SUB и MAIN_TABLE (главного окна)
        if (current_table_data_ref_for_names && current_table_data_ref_for_names->column_names) {
            for (int i = 0; i < data_col_count_local; i++) { // data_col_count_local - это количество Xn
                // Имя колонки Xn берется из current_table_data_ref_for_names->column_names[i+1]
                const char* col_title_data = (current_table_data_ref_for_names->column_names[i + 1]) ? 
                                             current_table_data_ref_for_names->column_names[i + 1] : "ERR_COL_NAME";
                gint model_col_idx = i + 1; // Индекс в модели для этой колонки данных
                
                renderer = gtk_cell_renderer_text_new(); 
                g_object_set(renderer, "xalign", 0.5, NULL); // "editable" убрано для всех L1 таблиц, редактирование через диалог
                col = gtk_tree_view_column_new(); 
                gtk_tree_view_column_set_title(col, col_title_data);
                gtk_tree_view_column_pack_start(col, renderer, TRUE);
                gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(model_col_idx), NULL);
                gtk_tree_view_column_set_fixed_width(col, 80);
                gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_to_update), col);
            }
        }
        // Дополнительные колонки для L1-типа (μ и α или их производные)
        if (is_a_level1_type_table) {
            int mu_model_idx = data_col_count_local + 1;
            renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new(); 
            gtk_tree_view_column_set_title(col, mu_col_title_l1); // mu_col_title_l1 установлен в начале
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(mu_model_idx), NULL);
            g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1)); // Форматирование как μ
            gtk_tree_view_column_set_fixed_width(col, 80);
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_to_update), col);
            
            int alpha_model_idx = data_col_count_local + 2;
            renderer = gtk_cell_renderer_text_new(); g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new(); 
            gtk_tree_view_column_set_title(col, alpha_col_title_l1); // alpha_col_title_l1 установлен в начале
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(alpha_model_idx), NULL);
            g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1)); // Форматирование как α
            gtk_tree_view_column_set_fixed_width(col, 100);
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_to_update), col);
        }
    }

    // Установка модели для TreeView
    if (*store_ptr_to_update) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_to_update), GTK_TREE_MODEL(*store_ptr_to_update));
    } else {
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view_to_update), NULL);
    }
    debug_print("update_tree_view_columns (%s): Завершено.\n", context_str_debug);
}


// --- Реализация cell_data_func (non-static) ---
void cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
                    GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer user_data_callback)
{
    gint model_column_index = GPOINTER_TO_INT(user_data_callback);
    GtkWidget* tree_view_widget_raw = gtk_tree_view_column_get_tree_view(tree_column);
    GtkTreeView* current_tree_view = GTK_IS_TREE_VIEW(tree_view_widget_raw) ? GTK_TREE_VIEW(tree_view_widget_raw) : NULL;
    AppData* app_data_global = current_tree_view ? g_object_get_data(G_OBJECT(current_tree_view), "app_data_ptr_for_debug") : NULL;

    gdouble value;
    gtk_tree_model_get(tree_model, iter, model_column_index, &value, -1);
    gchar *text = NULL;

    if (g_object_get_data(G_OBJECT(tree_column), "is-alpha")) {
        if (isnan(value)) text = g_strdup(""); else text = g_strdup_printf("%.2E", value);
    } else if (g_object_get_data(G_OBJECT(tree_column), "is-mu")) {
        if (isnan(value)) text = g_strdup("");
        // Убираем условие для нулевого значения μ, чтобы 0.0000 отображался.
        // else if (value == 0.0) text = g_strdup("");
        else text = g_strdup_printf("%.4f", value);
    } else if (g_object_get_data(G_OBJECT(tree_column), "is-integer-like")) {
        if (isnan(value)) text = g_strdup(""); else text = g_strdup_printf("%d", (int)round(value));
    } else {
        if (isnan(value)) text = g_strdup(""); else text = g_strdup_printf("%.4f", value);
    }
    g_object_set(cell, "text", text, NULL);
    g_free(text);
}


// --- Реализация update_values_table_view (static) ---
static void update_values_table_view(AppData *data) {
    // ... (код без изменений, как был предоставлен ранее, включая GTK_IS_WIDGET)
    if (!data || !data->tree_view_values) { debug_print("update_values_table_view: Пропуск (no data/tree_view_values)\n"); return; }
    if (!data->table_data_values) { if (data->tree_view_values) gtk_widget_set_visible(data->tree_view_values, FALSE); debug_print("update_values_table_view: Пропуск (no table_data_values)\n"); return; }
    if (GTK_IS_WIDGET(data->tree_view_values)) g_object_set_data(G_OBJECT(data->tree_view_values), "app_data_ptr_for_debug", data);
    debug_print("update_values_table_view: Обновление таблицы 'Значения'...\n");
    GtkTreeView *treeview = GTK_TREE_VIEW(data->tree_view_values);
    GList *columns = gtk_tree_view_get_columns(treeview);
    for (GList *l = columns; l; l = l->next) gtk_tree_view_remove_column(treeview, GTK_TREE_VIEW_COLUMN(l->data));
    g_list_free_full(columns, g_object_unref);
    if (data->store_values) { g_object_unref(data->store_values); data->store_values = NULL; }
    if (data->table_data_values->column_names == NULL || data->table_data_values->column_count == 0 || data->table_data_values->row_count == 0) {
        gtk_tree_view_set_model(treeview, NULL); gtk_widget_set_visible(data->tree_view_values, FALSE); return;
    }
    int num_displayable_data_columns = 0;
    GList *displayable_db_col_names = NULL;
    GArray *displayable_data_col_indices = g_array_new(FALSE, FALSE, sizeof(int));
    for (int j_data = 0; j_data < data->table_data_values->column_count; j_data++) {
        if (data->table_data_values->column_names[j_data]) {
            const char * col_name_db = data->table_data_values->column_names[j_data];
            if (data->schema_blob_column_name && g_strcmp0(col_name_db, data->schema_blob_column_name) == 0) continue;
            num_displayable_data_columns++;
            displayable_db_col_names = g_list_append(displayable_db_col_names, (gpointer)col_name_db);
            g_array_append_val(displayable_data_col_indices, j_data);
        }
    }
    if (num_displayable_data_columns == 0) { gtk_tree_view_set_model(treeview, NULL); gtk_widget_set_visible(data->tree_view_values, FALSE); g_list_free(displayable_db_col_names); g_array_free(displayable_data_col_indices, TRUE); return; }
    int num_model_cols = 1 + num_displayable_data_columns;
    GType *types = g_new(GType, num_model_cols); types[0] = G_TYPE_INT;
    for (int i = 1; i < num_model_cols; i++) types[i] = G_TYPE_DOUBLE;
    data->store_values = gtk_list_store_newv(num_model_cols, types); g_free(types);
    GtkCellRenderer *renderer_val; GtkTreeViewColumn *col_val;
    GList *name_iter = displayable_db_col_names;
    for (int i_disp = 0; i_disp < num_displayable_data_columns; ++i_disp) {
        const char* col_db_name_val = (const char*)name_iter->data;
        int model_idx_in_store = i_disp + 1;
        renderer_val = gtk_cell_renderer_text_new();
        // УБИРАЕМ РЕДАКТИРОВАНИЕ НА МЕСТЕ
        // g_object_set(renderer_val, "editable", TRUE, "xalign", 0.5, NULL);
        g_object_set(renderer_val, "xalign", 0.5, NULL); // Оставляем выравнивание
        col_val = gtk_tree_view_column_new();

        gtk_tree_view_column_set_title(col_val, col_db_name_val);
        gtk_tree_view_column_pack_start(col_val, renderer_val, TRUE);
        if (g_strcmp0(col_db_name_val, "Количество") == 0) {
            g_object_set_data(G_OBJECT(col_val), "is-integer-like", GINT_TO_POINTER(1));
        }
        // gtk_tree_view_column_add_attribute(col_val, renderer_val, "text", model_idx_in_store); // Используем cell_data_func
        gtk_tree_view_column_set_cell_data_func(col_val, renderer_val, cell_data_func, GINT_TO_POINTER(model_idx_in_store), NULL);
        gtk_tree_view_column_set_fixed_width(col_val, 100);
        gtk_tree_view_append_column(treeview, col_val);
        name_iter = name_iter->next;
    }
    g_list_free(displayable_db_col_names);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(data->store_values));
    for(int i_row = 0; i_row < data->table_data_values->row_count; ++i_row) {
        GtkTreeIter iter_val; gtk_list_store_append(data->store_values, &iter_val);
        int rowid_val = (data->table_data_values->epochs) ? data->table_data_values->epochs[i_row] : (i_row + 1);
        gtk_list_store_set(data->store_values, &iter_val, 0, rowid_val, -1);
        for(guint i_disp = 0; i_disp < displayable_data_col_indices->len; ++i_disp) {
            int model_col_idx_to_set = i_disp + 1;
            int data_col_idx_val = g_array_index(displayable_data_col_indices, int, i_disp);
            double value_to_set = NAN;
            if (data->table_data_values->table && data->table_data_values->table[i_row] != NULL && data_col_idx_val < data->table_data_values->column_count)
                value_to_set = data->table_data_values->table[i_row][data_col_idx_val];
            gtk_list_store_set(data->store_values, &iter_val, model_col_idx_to_set, value_to_set, -1);
        }
    }
    g_array_free(displayable_data_col_indices, TRUE);
    gtk_widget_set_visible(data->tree_view_values, TRUE);
    debug_print("update_values_table_view: Завершено.\n");
}

// --- Реализация load_and_display_schema_image_from_values_table (static) ---
static void load_and_display_schema_image_from_values_table(AppData *data) {
    // ... (код без изменений, как был предоставлен ранее)
    if (!data || !data->db_opened || !*data->db || !data->values_table_name || !data->schema_blob_column_name) { if(data && data->picture_schema){ gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL); gtk_widget_set_visible(data->picture_schema, FALSE); } return; }
    int rowid_for_image = 1; GBytes *image_bytes = load_blob_from_table_by_rowid(*data->db, data->values_table_name, data->schema_blob_column_name, rowid_for_image);
    if (data->pixbuf_schema) { g_object_unref(data->pixbuf_schema); data->pixbuf_schema = NULL; }
    GdkPixbuf *loaded_pixbuf = NULL;
    if (image_bytes && data->picture_schema) {
        GError *error_load = NULL; GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
        if (!gdk_pixbuf_loader_write (loader, g_bytes_get_data (image_bytes, NULL), g_bytes_get_size (image_bytes), &error_load)) { if(error_load) g_error_free(error_load); g_object_unref (loader); g_bytes_unref (image_bytes); gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL); gtk_widget_set_visible(data->picture_schema, FALSE); return; }
        if (!gdk_pixbuf_loader_close (loader, &error_load)) { if(error_load) g_error_free(error_load); } // Ignore error on close, try to get pixbuf
        loaded_pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
        if (loaded_pixbuf) { g_object_ref(loaded_pixbuf); data->pixbuf_schema = loaded_pixbuf; gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), data->pixbuf_schema); gtk_widget_set_visible(data->picture_schema, TRUE); }
        else { gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL); gtk_widget_set_visible(data->picture_schema, FALSE); }
        g_object_unref (loader); g_bytes_unref (image_bytes);
    } else { if (data->picture_schema) { gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL); gtk_widget_set_visible(data->picture_schema, FALSE); } }
}


// --- Реализация load_and_display_main_table (static) ---
static void load_and_display_main_table(AppData *data, const char* table_name_to_load) {
    debug_print("load_and_display_main_table: Загрузка таблицы '%s'\n", table_name_to_load);
    if (!data || !table_name_to_load || !data->table_data_main) return;

    if (data->table_data_main->table) { for (int i = 0; i < data->table_data_main->row_count; i++) if (data->table_data_main->table[i]) g_free(data->table_data_main->table[i]); g_free(data->table_data_main->table); data->table_data_main->table = NULL; }
    if (data->table_data_main->epochs) { g_free(data->table_data_main->epochs); data->table_data_main->epochs = NULL; }
    if (data->table_data_main->column_names) { int num_names = data->table_data_main->column_count + 1; for(int i=0; i<num_names; ++i) if(data->table_data_main->column_names[i]) g_free(data->table_data_main->column_names[i]); g_free(data->table_data_main->column_names); data->table_data_main->column_names = NULL;}
    data->table_data_main->row_count = 0; data->table_data_main->column_count = 0;
    g_free(data->current_main_table_name); data->current_main_table_name = g_strdup(table_name_to_load);

    if (!data->db || !*data->db) { show_message_dialog(data->parent, "Ошибка: База данных не открыта."); return; }
    load_table(*data->db, data->table_data_main, data->current_main_table_name);

    // ИСПРАВЛЕНИЕ: вызываем update_tree_view_columns, чтобы он удалил старые колонки и создал новые ПЕРЕД заполнением store_main
    update_tree_view_columns(data, 
                             TRUE,  // is_main_table_context
                             FALSE, // is_level1_main_context
                             FALSE, // is_level1_error_adj_context
                             FALSE, // is_level1_error_sub_context
                             FALSE, // is_smoothing_mu_table
                             FALSE, // is_smoothing_alpha_table
                             FALSE, // is_smoothing_mu_prime_table
                             FALSE, // is_smoothing_alpha_prime_table
                             FALSE, // is_smoothing_mu_double_prime_table
                             FALSE  // is_smoothing_alpha_double_prime_table
                            );

    if (data->store_main) {
        gtk_list_store_clear(data->store_main);
        if (data->table_data_main->row_count > 0 && data->table_data_main->epochs && data->table_data_main->table) {
            int n_model_cols = gtk_tree_model_get_n_columns(GTK_TREE_MODEL(data->store_main));
            for (int i = 0; i < data->table_data_main->row_count; i++) {
                GtkTreeIter tree_iter; gtk_list_store_append(data->store_main, &tree_iter);
                gtk_list_store_set(data->store_main, &tree_iter, 0, data->table_data_main->epochs[i], -1);
                if(data->table_data_main->table[i]) {
                     for (int j = 0; j < data->table_data_main->column_count; j++) {
                          if ((j + 1) < n_model_cols) gtk_list_store_set(data->store_main, &tree_iter, j + 1, data->table_data_main->table[i][j], -1);
                          else break;
                     }
                 }
            }
        }
    } else {
        if(GTK_IS_TREE_VIEW(data->tree_view_main)) gtk_tree_view_set_model(GTK_TREE_VIEW(data->tree_view_main), NULL);
    }

    if(data->tree_view_main) gtk_widget_set_visible(data->tree_view_main, TRUE);
    if (data->add_row_button) gtk_widget_set_sensitive(data->add_row_button, data->table_data_main->row_count >= 2 && data->table_data_main->column_count > 0);
    if (data->level1_button) gtk_widget_set_sensitive(data->level1_button, data->table_data_main->row_count > 0);
}


// --- Реализация open_sqlite_file_callback (static) ---
static void open_sqlite_file_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    AppData *data = user_data;
    GFile *file = NULL; GError *error = NULL;
    file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), res, &error);
    if (error) { show_message_dialog(data->parent, error->message); g_error_free(error); if(file) g_object_unref(file); return; }
    if (!file) return;
    gchar *filename_utf8 = g_file_get_path(file); g_object_unref(file);
    if (!filename_utf8) { show_message_dialog(data->parent, "Не удалось получить путь к файлу."); return; }

    // ОЧИСТКА ВСЕГО
    if (data->db_opened && *data->db) { sqlite3_close(*data->db); *data->db = NULL; data->db_opened = FALSE; }
    g_free(data->current_main_table_name);
    data->current_main_table_name = NULL;
    // ...Полная очистка data->table_data_main как в load_and_display_main_table
    if (data->table_data_main->table) { for (int i=0;i<data->table_data_main->row_count;++i) if(data->table_data_main->table[i]) g_free(data->table_data_main->table[i]); g_free(data->table_data_main->table); data->table_data_main->table = NULL; }
    if (data->table_data_main->epochs) {g_free(data->table_data_main->epochs);data->table_data_main->epochs=NULL;}
    if (data->table_data_main->column_names) {int n=data->table_data_main->column_count+1; for(int i=0;i<n;++i) if(data->table_data_main->column_names[i]) g_free(data->table_data_main->column_names[i]);g_free(data->table_data_main->column_names);data->table_data_main->column_names=NULL;}
    data->table_data_main->row_count = 0; data->table_data_main->column_count = 0;
    // ИСПРАВЛЕНИЕ: Вызываем update_tree_view_columns с флагами FALSE, чтобы очистить UI главной таблицы
    if(data->tree_view_main) {
        update_tree_view_columns(data, 
                                 TRUE,  // is_main_table_context (для очистки именно этой таблицы)
                                 FALSE, // is_level1_main_context
                                 FALSE, // is_level1_error_adj_context
                                 FALSE, // is_level1_error_sub_context
                                 FALSE, // is_smoothing_mu_table
                                 FALSE, // is_smoothing_alpha_table
                                 FALSE, // is_smoothing_mu_prime_table
                                 FALSE, // is_smoothing_alpha_prime_table
                                 FALSE, // is_smoothing_mu_double_prime_table
                                 FALSE  // is_smoothing_alpha_double_prime_table
                                ); 
    }
    if(data->tree_view_main) gtk_widget_set_visible(data->tree_view_main, FALSE);

    // ... Полная очистка data->table_data_values
    if (data->table_data_values->table) { for (int i=0;i<data->table_data_values->row_count;++i) if(data->table_data_values->table[i]) g_free(data->table_data_values->table[i]); g_free(data->table_data_values->table); data->table_data_values->table = NULL; }
    if (data->table_data_values->epochs) {g_free(data->table_data_values->epochs);data->table_data_values->epochs=NULL;}
    if (data->table_data_values->column_names) {int n=data->table_data_values->column_count+1; for(int i=0;i<n;++i) if(data->table_data_values->column_names[i]) g_free(data->table_data_values->column_names[i]);g_free(data->table_data_values->column_names);data->table_data_values->column_names=NULL;}
    data->table_data_values->row_count = 0; data->table_data_values->column_count = 0;
    if(data->tree_view_values) update_values_table_view(data); // Это скроет таблицу если данных нет
    if(data->tree_view_values) gtk_widget_set_visible(data->tree_view_values, FALSE);


    if (data->pixbuf_schema) { g_object_unref(data->pixbuf_schema); data->pixbuf_schema = NULL; }
    if (data->picture_schema) { gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL); gtk_widget_set_visible(data->picture_schema, FALSE); }
    level1_cleanup_calculated_data(data);
    if (data->level1_window) { gtk_window_destroy(GTK_WINDOW(data->level1_window)); data->level1_window = NULL; }
    if(data->add_row_button) gtk_widget_set_sensitive(data->add_row_button, FALSE);
    if(data->level1_button) gtk_widget_set_sensitive(data->level1_button, FALSE);

    *data->db = open_database(filename_utf8);
    if (*data->db) {
        data->db_opened = TRUE;
        char *main_analysis_table_name_cb = NULL;
        GList *table_names_list = get_table_list(*data->db);
        if (table_names_list) {
             for (GList *l = table_names_list; l != NULL; l = l->next) {
                 if (data->values_table_name && strcmp((char*)l->data, data->values_table_name) != 0) { main_analysis_table_name_cb = g_strdup((char*)l->data); break; }
             }
            if (!main_analysis_table_name_cb && g_list_length(table_names_list) > 0) main_analysis_table_name_cb = g_strdup((char*)g_list_nth_data(table_names_list, 0));
             g_list_free_full(table_names_list, g_free);
        }
        if (main_analysis_table_name_cb) { load_and_display_main_table(data, main_analysis_table_name_cb); g_free(main_analysis_table_name_cb); }
        else { show_message_dialog(data->parent, "В БД не найдено таблиц для анализа."); }
        if (data->values_table_name) { load_table(*data->db, data->table_data_values, data->values_table_name); update_values_table_view(data); }
        load_and_display_schema_image_from_values_table(data);
        show_message_dialog(data->parent, "База данных успешно открыта.");
    } else { show_message_dialog(data->parent, "Не удалось открыть БД."); }
    g_free(filename_utf8);
}


// --- Реализация open_sqlite_file (static) ---
static void open_sqlite_file(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Выберите SQLite файл");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SQLite файлы");
    gtk_file_filter_add_pattern(filter, "*.sqlite"); gtk_file_filter_add_pattern(filter, "*.db"); gtk_file_filter_add_pattern(filter, "*.sqlite3");
    gtk_file_dialog_set_default_filter(dialog, filter);
    g_object_unref(filter);
    gtk_file_dialog_open(dialog, data->parent, NULL, open_sqlite_file_callback, data);
    g_object_unref(dialog);
}

// --- Реализация on_level_clicked (static) ---
static void on_level_clicked(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    const char *label = gtk_button_get_label(GTK_BUTTON(widget));
    if (g_strcmp0(label, "Уровень 1") == 0) {
        if (!data->db_opened) { show_message_dialog(data->parent, "Сначала откройте базу данных."); return; }
        if (!data->current_main_table_name) { show_message_dialog(data->parent, "Основная таблица не определена."); return; }
        if (data->table_data_main == NULL || data->table_data_main->row_count == 0){ show_message_dialog(data->parent, "Основная таблица пуста."); return; }
        level1_show_window(data);
    } else {
        show_message_dialog(data->parent, "Функция для этого уровня не реализована.");
    }
}

// --- Реализация add_new_row (static) ---
static void add_new_row(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    if (!data->db_opened) { show_message_dialog(data->parent, "Сначала откройте базу данных."); return; }
    if (!data->current_main_table_name) { show_message_dialog(data->parent, "Основная таблица не определена."); return; }
    if (!data->table_data_main || data->table_data_main->column_count <= 0 || data->table_data_main->row_count < 2) {
        show_message_dialog(data->parent, "Нужно минимум 2 строки и колонки данных для прогноза."); return;
    }
    double *new_coordinates = calculate_new_row(data->table_data_main);
    if (!new_coordinates) { show_message_dialog(data->parent, "Не удалось вычислить новую строку."); return; }
    int new_epoch = (data->table_data_main->epochs && data->table_data_main->row_count > 0) ? (data->table_data_main->epochs[data->table_data_main->row_count - 1] + 1) : (data->table_data_main->row_count + 1);
    if (!data->table_data_main->column_names) { show_message_dialog(data->parent, "Ошибка: Отсутствуют имена колонок."); g_free(new_coordinates); return; }
    if (!insert_row_into_table(*data->db, data->current_main_table_name, new_epoch, new_coordinates, data->table_data_main->column_count, data->table_data_main->column_names)) {
        show_message_dialog(data->parent, "Ошибка добавления строки в БД."); g_free(new_coordinates); return;
    }
    load_and_display_main_table(data, data->current_main_table_name);
    level1_update_content_if_visible(data);
    g_free(new_coordinates);
    show_message_dialog(data->parent, "Новая строка успешно добавлена.");
}

// --- Реализация on_cell_edited (static) ---
static void on_cell_edited(GtkCellRendererText *renderer, gchar *path_string, gchar *new_text, gpointer user_data) {
    AppData *data = user_data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    if (!path) return;

    GtkTreeIter iter;
    gboolean path_valid;
    GtkTreeModel *model = NULL;
    GtkListStore *current_store = NULL;
    const char* tree_view_id = g_object_get_data(G_OBJECT(renderer), "tree-view-origin");
    TableData *current_table_data_ref = NULL;
    const char *current_table_name_for_db = NULL;
    const char *id_col_db_name = NULL; // Имя колонки ID в БД
    gboolean is_main_analysis_table = FALSE;
    gboolean is_values_table = FALSE;

    // Определяем, какая таблица редактируется
    if (tree_view_id && g_strcmp0(tree_view_id, "main") == 0) {
        // Предполагаем, что редактирование происходит только в data->tree_view_main,
        // а data->level1_tree_view - только для чтения. Если это не так, нужна более сложная проверка.
        model = GTK_TREE_MODEL(data->store_main);
        current_store = data->store_main;
        current_table_data_ref = data->table_data_main;
        current_table_name_for_db = data->current_main_table_name;
        if (current_table_data_ref && current_table_data_ref->column_names && current_table_data_ref->column_names[0]) {
            id_col_db_name = current_table_data_ref->column_names[0];
        }
        is_main_analysis_table = TRUE;
    } else if (tree_view_id && g_strcmp0(tree_view_id, "values") == 0) {
        model = GTK_TREE_MODEL(data->store_values);
        current_store = data->store_values;
        current_table_data_ref = data->table_data_values;
        current_table_name_for_db = data->values_table_name;
        // Для таблицы "Значения" id_col_db_name не используется, так как обновление идет по ROWID
        is_values_table = TRUE;
    } else {
        gtk_tree_path_free(path);
        return;
    }

    if (!model || !current_store || !current_table_data_ref || !current_table_name_for_db) {
        gtk_tree_path_free(path);
        return;
    }

    path_valid = gtk_tree_model_get_iter(model, &iter, path);
    if (!path_valid) {
        gtk_tree_path_free(path);
        return;
    }

    gint model_column_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "model-column-index"));
    const char *db_column_name_to_update = g_object_get_data(G_OBJECT(renderer), "db-column-name");
    if (!db_column_name_to_update) {
        gtk_tree_path_free(path);
        return;
    }

    // Пропускаем редактирование колонки схемы, если она как-то попала сюда
    if (data->schema_blob_column_name && g_strcmp0(db_column_name_to_update, data->schema_blob_column_name) == 0) {
        gtk_tree_path_free(path);
        return;
    }

    gdouble new_value_double;
    char *endptr;
    gchar *temp_text = g_strdup(new_text);
    if(!temp_text){ gtk_tree_path_free(path); return; }
    for (char *p = temp_text; *p; ++p) if (*p == ',') *p = '.'; // Замена запятой на точку
    errno = 0;
    new_value_double = g_ascii_strtod(temp_text, &endptr);
    g_free(temp_text);

    if (errno == ERANGE || endptr == new_text || (*endptr != '\0' && !g_ascii_isspace(*endptr))) {
        show_message_dialog(data->parent, "Ошибка: Неверный формат числа.");
        gtk_tree_path_free(path);
        return;
    }

    gint row_id_for_db; // Это будет либо значение из колонки ID, либо ROWID
    gtk_tree_model_get(model, &iter, 0, &row_id_for_db, -1); // Получаем значение из первой колонки модели (ID или ROWID)

    if (!data->db || !*data->db) {
        show_message_dialog(data->parent, "Ошибка: База данных закрыта.");
        gtk_tree_path_free(path);
        return;
    }

    // Обновляем значение в БД
    // Для таблицы "Значения" id_col_db_name должен быть NULL, чтобы update_table_cell использовал ROWID
    gboolean updated_db = update_table_cell(*data->db,
                                            current_table_name_for_db,
                                            db_column_name_to_update,
                                            is_values_table ? NULL : id_col_db_name, // NULL для ROWID
                                            new_value_double,
                                            row_id_for_db);

    if (updated_db) {
        // Обновляем значение в GtkListStore
        gtk_list_store_set(current_store, &iter, model_column_index, new_value_double, -1);

        // Обновляем данные в памяти (в структурах TableData)
        // Вместо полной перезагрузки load_table, попробуем обновить только измененную ячейку в TableData
        int row_index_in_tabledata = -1;
        // Найти соответствующую строку в TableData. Для простоты, если ID/ROWID совпадают.
        // Это может быть сложнее, если порядок в UI и TableData не синхронизирован 1-в-1 по row_id_for_db.
        // Пока предполагаем, что row_id_for_db (из первой колонки UI) это эпоха или ROWID,
        // и мы можем найти эту строку в current_table_data_ref->epochs.
        for(int i=0; i < current_table_data_ref->row_count; ++i) {
            if (current_table_data_ref->epochs && current_table_data_ref->epochs[i] == row_id_for_db) {
                row_index_in_tabledata = i;
                break;
            }
        }

        if (row_index_in_tabledata != -1 && current_table_data_ref->table && current_table_data_ref->table[row_index_in_tabledata]) {
            // Найти индекс колонки данных в TableData
            // model_column_index это индекс в UI (начиная с 1 для данных)
            // В TableData->table данные хранятся с индекса 0
            int data_column_idx_in_tabledata = -1;
            if (is_values_table) {
                // Для таблицы "Значения", db_column_name_to_update это имя колонки из БД
                // Нужно найти его индекс в current_table_data_ref->column_names
                for(int j=0; j < current_table_data_ref->column_count; ++j) {
                    if (current_table_data_ref->column_names[j] && strcmp(current_table_data_ref->column_names[j], db_column_name_to_update) == 0) {
                        data_column_idx_in_tabledata = j;
                        break;
                    }
                }
            } else { // Для основной таблицы анализа
                // model_column_index это индекс в UI (1 для первой колонки данных, 2 для второй и т.д.)
                // В TableData->table это будет model_column_index - 1
                data_column_idx_in_tabledata = model_column_index - 1;
            }

            if (data_column_idx_in_tabledata >= 0 && data_column_idx_in_tabledata < current_table_data_ref->column_count) {
                current_table_data_ref->table[row_index_in_tabledata][data_column_idx_in_tabledata] = new_value_double;
                debug_print("on_cell_edited: Значение в TableData обновлено: table[%d][%d] = %f\n",
                            row_index_in_tabledata, data_column_idx_in_tabledata, new_value_double);
            } else {
                 debug_print("on_cell_edited: Ошибка: не удалось найти индекс колонки данных в TableData для '%s'. Перезагрузка всей таблицы.\n", db_column_name_to_update);
                 load_table(*data->db, current_table_data_ref, current_table_name_for_db); // Аварийная перезагрузка
            }
        } else {
            debug_print("on_cell_edited: Ошибка: не удалось найти строку в TableData для ID/ROWID %d. Перезагрузка всей таблицы.\n", row_id_for_db);
            load_table(*data->db, current_table_data_ref, current_table_name_for_db); // Аварийная перезагрузка
        }

        // Если это была основная таблица анализа, обновить Уровень 1
        if (is_main_analysis_table) {
            level1_update_content_if_visible(data);
            debug_print("on_cell_edited: level1_update_content_if_visible ВРЕМЕННО ОТКЛЮЧЕНА для теста.\n");
        }
        // Для таблицы "Значения" специфическое обновление UI (если нужно, кроме картинки) не требуется,
        // так как ее данные используются только для картинки и для расчетов в Уровне 1.
        // Картинка обновляется отдельно при открытии файла. Если нужно обновлять при редактировании,
        // то load_and_display_schema_image_from_values_table(data); здесь.

    } else {
        show_message_dialog(data->parent, "Ошибка: Не удалось обновить значение в БД.");
    }
    gtk_tree_path_free(path);
}

typedef struct {
    AppData *app_data;
    GtkTreeIter iter_to_edit; // Копия итератора строки для обновления в ListStore
    GtkListStore *store_to_edit;
    gboolean data_was_saved;
    GtkWidget **entries_array; // Массив GtkEntry для значений
    int num_data_columns;    // Количество колонок данных (X1, X2...)
    int original_row_id_value; // ID или ROWID редактируемой строки из первой колонки модели
    char **original_column_names_in_db; // Имена колонок данных в БД (X1, X2...)
} EditDialogContext;

typedef struct {
    AppData *app_data;
    GtkTreeIter iter_to_edit;
    GtkListStore *store_to_edit; // Будет data->store_values
    gboolean data_was_saved;
    GtkWidget **entries_array;
    int num_data_columns;        // Количество редактируемых колонок в таблице "Значения"
    int original_row_id_value;   // Это будет ROWID из таблицы "Значения"
    char **original_column_names_in_db; // Имена колонок из TableData (не из UI)
    int *original_data_column_indices; // Индексы этих колонок в TableData->table
} ValuesEditDialogContext;

static void free_values_edit_dialog_context(gpointer user_data) {
    ValuesEditDialogContext *ctx_to_free = (ValuesEditDialogContext*)user_data;
    if (ctx_to_free) {
        debug_print("Освобождение ValuesEditDialogContext\n");
        if (ctx_to_free->entries_array) {
            g_free(ctx_to_free->entries_array);
        }
        if (ctx_to_free->original_column_names_in_db) {
            for(int i=0; i < ctx_to_free->num_data_columns; ++i) {
                g_free(ctx_to_free->original_column_names_in_db[i]);
            }
            g_free(ctx_to_free->original_column_names_in_db);
        }
        if (ctx_to_free->original_data_column_indices) {
            g_free(ctx_to_free->original_data_column_indices);
        }
        // iter_to_edit не требует специального free
        g_free(ctx_to_free);
    }
}

static void on_values_edit_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    ValuesEditDialogContext *edit_ctx = user_data;
    AppData *data = edit_ctx->app_data;

    if (response_id == GTK_RESPONSE_ACCEPT) {
        gboolean all_values_valid = TRUE;
        double *new_parsed_values = g_new(double, edit_ctx->num_data_columns);

        for (int i = 0; i < edit_ctx->num_data_columns; i++) {
            const char *text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(edit_ctx->entries_array[i])));
            gchar *temp_text_for_conversion = g_strdup(text);
            for (char *p = temp_text_for_conversion; *p; ++p) if (*p == ',') *p = '.';
            
            char *endptr;
            errno = 0;
            new_parsed_values[i] = g_ascii_strtod(temp_text_for_conversion, &endptr);
            g_free(temp_text_for_conversion);

            if (errno == ERANGE || endptr == text || (*endptr != '\0' && !g_ascii_isspace(*endptr))) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Ошибка: Неверный формат числа для поля '%s'",
                         edit_ctx->original_column_names_in_db[i] ? edit_ctx->original_column_names_in_db[i] : "Неизвестно");
                show_message_dialog(GTK_WINDOW(dialog), err_msg);
                all_values_valid = FALSE;
                break; 
            }
        }

        if (all_values_valid) {
            gboolean overall_db_update_success = TRUE;
            for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                const char *db_col_name_to_update = edit_ctx->original_column_names_in_db[i];
                if (db_col_name_to_update) {
                    // Для таблицы "Значения" обновляем по ROWID, поэтому id_column_name = NULL
                    if (!update_table_cell(*data->db, data->values_table_name,
                                           db_col_name_to_update, NULL, 
                                           new_parsed_values[i], edit_ctx->original_row_id_value)) {
                        overall_db_update_success = FALSE;
                        char err_msg[256];
                        snprintf(err_msg, sizeof(err_msg), "Ошибка обновления значения в БД для колонки '%s'", db_col_name_to_update);
                        show_message_dialog(GTK_WINDOW(dialog), err_msg);
                        break; 
                    }
                }
            }

            if (overall_db_update_success) {
                // Обновляем GtkListStore (UI)
                // model_column_index для данных в UI таблицы "Значения" начинается с 1
                for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                    gtk_list_store_set(edit_ctx->store_to_edit, &edit_ctx->iter_to_edit, i + 1, new_parsed_values[i], -1);
                }

                // Обновляем TableData в памяти (data->table_data_values)
                int row_idx_in_memory = -1;
                for(int k=0; k < data->table_data_values->row_count; ++k) {
                    if (data->table_data_values->epochs && data->table_data_values->epochs[k] == edit_ctx->original_row_id_value) {
                        row_idx_in_memory = k;
                        break;
                    }
                }
                if (row_idx_in_memory != -1 && data->table_data_values->table[row_idx_in_memory]) {
                    for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                        int data_idx = edit_ctx->original_data_column_indices[i]; // Индекс колонки в TableData->table
                        data->table_data_values->table[row_idx_in_memory][data_idx] = new_parsed_values[i];
                    }
                } else {
                    load_table(*data->db, data->table_data_values, data->values_table_name);
                }
                
                edit_ctx->data_was_saved = TRUE;
                // Обновляем Уровень 1, так как данные "E" могли измениться
                level1_update_content_if_visible(data); 
                // Если картинка схемы зависит от этих данных, ее тоже можно обновить:
                // load_and_display_schema_image_from_values_table(data);
            }
        }
        g_free(new_parsed_values);
    }

    if (response_id == GTK_RESPONSE_CANCEL || (response_id == GTK_RESPONSE_ACCEPT && edit_ctx->data_was_saved)) {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
}

static void show_values_edit_dialog(AppData *data, GtkTreeModel *model, GtkTreeIter *iter) {
    if (!data || !data->parent) {
        g_warning("show_values_edit_dialog: AppData или родительское окно не инициализированы.");
        return;
    }
    if (!model || !iter) {
        show_message_dialog(GTK_WINDOW(data->parent), "Ошибка: Некорректные параметры для редактирования строки 'Значения'.");
        return;
    }
    if (!data->table_data_values || data->table_data_values->column_count == 0 ||
        !data->table_data_values->column_names) {
        show_message_dialog(GTK_WINDOW(data->parent), "Ошибка: Нет данных или имен колонок для редактирования в таблице 'Значения'.");
        return;
    }

    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *grid;
    ValuesEditDialogContext *edit_ctx = NULL;

    dialog = gtk_dialog_new_with_buttons("Редактировать запись 'Значения'",
                                         GTK_WINDOW(data->parent),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Сохранить", GTK_RESPONSE_ACCEPT,
                                         "_Отмена", GTK_RESPONSE_CANCEL,
                                         NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1); 
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();
    gtk_widget_set_margin_start(grid, 10); gtk_widget_set_margin_end(grid, 10);
    gtk_widget_set_margin_top(grid, 10); gtk_widget_set_margin_bottom(grid, 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_append(GTK_BOX(content_area), grid);

    // Сначала подсчитаем количество редактируемых колонок
    int num_editable_cols = 0;
    for (int i = 0; i < data->table_data_values->column_count; i++) {
        if (data->table_data_values->column_names[i]) {
            if (data->schema_blob_column_name && 
                strcmp(data->table_data_values->column_names[i], data->schema_blob_column_name) == 0) {
                continue; // Пропускаем колонку "Схема"
            }
            num_editable_cols++;
        }
    }

    if (num_editable_cols == 0) {
        show_message_dialog(GTK_WINDOW(data->parent), "Нет редактируемых колонок в таблице 'Значения'.");
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }

    edit_ctx = g_new0(ValuesEditDialogContext, 1);
    if (!edit_ctx) {
        g_critical("show_values_edit_dialog: Не удалось выделить память для ValuesEditDialogContext!");
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }

    edit_ctx->app_data = data;
    edit_ctx->store_to_edit = data->store_values;
    edit_ctx->iter_to_edit = *iter; // Прямое копирование
    edit_ctx->data_was_saved = FALSE;
    // edit_ctx->num_data_columns будет установлено после цикла
    
    edit_ctx->entries_array = g_new0(GtkWidget*, num_editable_cols);
    if (!edit_ctx->entries_array) {
        g_critical("show_values_edit_dialog: Не удалось выделить память для entries_array!");
        g_free(edit_ctx);
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }
    edit_ctx->original_column_names_in_db = g_new0(char*, num_editable_cols);
    if (!edit_ctx->original_column_names_in_db) {
        g_critical("show_values_edit_dialog: Не удалось выделить память для original_column_names_in_db!");
        g_free(edit_ctx->entries_array);
        g_free(edit_ctx);
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }
    edit_ctx->original_data_column_indices = g_new0(int, num_editable_cols);
     if (!edit_ctx->original_data_column_indices) {
        g_critical("show_values_edit_dialog: Не удалось выделить память для original_data_column_indices!");
        g_free(edit_ctx->entries_array);
        g_free(edit_ctx->original_column_names_in_db);
        g_free(edit_ctx);
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }

    gtk_tree_model_get(model, iter, 0, &edit_ctx->original_row_id_value, -1); // 0-я колонка в store_values - это ROWID
    debug_print("show_values_edit_dialog: Редактирование строки 'Значения' с ROWID = %d\n", edit_ctx->original_row_id_value);

    int current_entry_idx = 0; // Индекс для массивов в edit_ctx
    int ui_model_col_idx = 1;  // Индекс колонки в GtkListStore (store_values), начиная с 1 для данных

    for (int i = 0; i < data->table_data_values->column_count; i++) { // Итерация по колонкам из TableData
        if (data->table_data_values->column_names[i]) {
            // Пропускаем колонку "Схема" - она не редактируется и не отображается в диалоге
            if (data->schema_blob_column_name && 
                strcmp(data->table_data_values->column_names[i], data->schema_blob_column_name) == 0) {
                continue; 
            }
            
            const char *col_name_for_label = data->table_data_values->column_names[i];
            edit_ctx->original_column_names_in_db[current_entry_idx] = g_strdup(col_name_for_label);
            edit_ctx->original_data_column_indices[current_entry_idx] = i; // Индекс в TableData->table

            GtkWidget *label = gtk_label_new(col_name_for_label);
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            edit_ctx->entries_array[current_entry_idx] = gtk_entry_new();
            gtk_widget_set_hexpand(edit_ctx->entries_array[current_entry_idx], TRUE);

            double current_value;
            // Получаем значение из UI модели (store_values) по текущему ui_model_col_idx
            gtk_tree_model_get(model, iter, ui_model_col_idx, &current_value, -1);
            
            char buffer[50];
            if (isnan(current_value)) {
                snprintf(buffer, sizeof(buffer), "");
            } else {
                gboolean is_int_like = (strcmp(col_name_for_label, "Количество") == 0);
                if (is_int_like) {
                    snprintf(buffer, sizeof(buffer), "%d", (int)round(current_value));
                } else {
                    snprintf(buffer, sizeof(buffer), "%.4f", current_value);
                }
            }
            gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(edit_ctx->entries_array[current_entry_idx])), buffer, -1);

            gtk_grid_attach(GTK_GRID(grid), label, 0, current_entry_idx, 1, 1);
            gtk_grid_attach(GTK_GRID(grid), edit_ctx->entries_array[current_entry_idx], 1, current_entry_idx, 1, 1);
            
            current_entry_idx++;
            ui_model_col_idx++; // Переходим к следующей колонке в UI модели
        }
    }
    edit_ctx->num_data_columns = current_entry_idx; // Фактическое количество созданных полей ввода

    g_object_set_data_full(G_OBJECT(dialog), "values-edit-dialog-context", edit_ctx, free_values_edit_dialog_context);
    g_signal_connect(dialog, "response", G_CALLBACK(on_values_edit_dialog_response), edit_ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_edit_values_row_clicked(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!data->tree_view_values) return;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view_values));
    if (!selection) return;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (model == GTK_TREE_MODEL(data->store_values)) {
            show_values_edit_dialog(data, model, &iter);
        }
    } else {
        show_message_dialog(GTK_WINDOW(data->parent), "Сначала выберите строку в таблице 'Значения' для редактирования.");
    }
}

static void on_values_table_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    AppData *data = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean row_selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    
    if (data->edit_values_button) {
        GtkTreeView *tree_view_of_selection = gtk_tree_selection_get_tree_view(selection);
        if (tree_view_of_selection == GTK_TREE_VIEW(data->tree_view_values)) {
            gtk_widget_set_sensitive(data->edit_values_button, row_selected);
        } else {
            // Это может произойти, если сигнал подключен к другому TreeSelection случайно
            // gtk_widget_set_sensitive(data->edit_values_button, FALSE);
        }
    }
}

static void on_edit_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    EditDialogContext *edit_ctx = user_data;
    AppData *data = edit_ctx->app_data;

    if (response_id == GTK_RESPONSE_ACCEPT) { // Кнопка "Сохранить"
        gboolean all_values_valid = TRUE;
        double *new_parsed_values = g_new(double, edit_ctx->num_data_columns);

        for (int i = 0; i < edit_ctx->num_data_columns; i++) {
            const char *text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(edit_ctx->entries_array[i])));
            gchar *temp_text_for_conversion = g_strdup(text);
            for (char *p = temp_text_for_conversion; *p; ++p) if (*p == ',') *p = '.';
            
            char *endptr;
            errno = 0;
            new_parsed_values[i] = g_ascii_strtod(temp_text_for_conversion, &endptr);
            g_free(temp_text_for_conversion);

            if (errno == ERANGE || endptr == text || (*endptr != '\0' && !g_ascii_isspace(*endptr))) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "Ошибка: Неверный формат числа для поля '%s'",
                         (edit_ctx->original_column_names_in_db && edit_ctx->original_column_names_in_db[i]) ?
                         edit_ctx->original_column_names_in_db[i] : "Неизвестно");
                show_message_dialog(GTK_WINDOW(dialog), err_msg); // Показываем ошибку в контексте диалога
                all_values_valid = FALSE;
                break; 
            }
        }

        if (all_values_valid) {
            gboolean overall_db_update_success = TRUE;
            const char *id_column_name_in_db = (data->table_data_main->column_names && data->table_data_main->column_names[0])
                                           ? data->table_data_main->column_names[0]
                                           : NULL;

            if (!id_column_name_in_db) {
                show_message_dialog(GTK_WINDOW(dialog), "Ошибка: Не определено имя колонки ID для обновления БД.");
                overall_db_update_success = FALSE;
            } else {
                for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                    const char *db_col_name_to_update = edit_ctx->original_column_names_in_db[i];
                    if (db_col_name_to_update) {
                        if (!update_table_cell(*data->db, data->current_main_table_name,
                                               db_col_name_to_update, id_column_name_in_db,
                                               new_parsed_values[i], edit_ctx->original_row_id_value)) {
                            overall_db_update_success = FALSE;
                            char err_msg[256];
                            snprintf(err_msg, sizeof(err_msg), "Ошибка обновления значения в БД для колонки '%s'", db_col_name_to_update);
                            show_message_dialog(GTK_WINDOW(dialog), err_msg);
                            break; 
                        }
                    }
                }
            }

            if (overall_db_update_success) {
                // Обновляем GtkListStore (UI)
                for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                    // model_column_index для данных начинается с 1 (0 - это ID)
                    gtk_list_store_set(edit_ctx->store_to_edit, &edit_ctx->iter_to_edit, i + 1, new_parsed_values[i], -1);
                }

                // Обновляем TableData в памяти
                int row_idx_in_memory = -1;
                for(int k=0; k < data->table_data_main->row_count; ++k) {
                    if (data->table_data_main->epochs && data->table_data_main->epochs[k] == edit_ctx->original_row_id_value) {
                        row_idx_in_memory = k;
                        break;
                    }
                }
                if (row_idx_in_memory != -1 && data->table_data_main->table[row_idx_in_memory]) {
                    for (int i = 0; i < edit_ctx->num_data_columns; i++) {
                        data->table_data_main->table[row_idx_in_memory][i] = new_parsed_values[i];
                    }
                } else {
                    // Если не нашли строку в памяти, лучше перезагрузить из БД, чтобы избежать рассинхрона
                    // Это также обновит store_main через load_and_display_main_table
                    load_and_display_main_table(data, data->current_main_table_name);
                }
                
                edit_ctx->data_was_saved = TRUE;
                // show_message_dialog(GTK_WINDOW(data->parent), "Строка успешно обновлена."); // Можно убрать, если не нужно
                level1_update_content_if_visible(data);
            }
        }
        g_free(new_parsed_values);
    }

    if (response_id == GTK_RESPONSE_CANCEL || (response_id == GTK_RESPONSE_ACCEPT && edit_ctx->data_was_saved)) {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
    // EditDialogContext будет освобожден через g_object_set_data_full при уничтожении диалога, если он был так установлен.
    // Если он просто передается как user_data, то нужно g_free(edit_ctx) здесь, если он больше не нужен.
    // В show_edit_dialog мы использовали g_object_set_data_full, так что он освободится автоматически.
}


static void free_edit_dialog_context(gpointer user_data) { // user_data это edit_ctx
    EditDialogContext *ctx_to_free = (EditDialogContext*)user_data;
    if (ctx_to_free) {
        debug_print("Освобождение EditDialogContext\n");
        if (ctx_to_free->entries_array) {
            g_free(ctx_to_free->entries_array);
            ctx_to_free->entries_array = NULL;
        }
        if (ctx_to_free->original_column_names_in_db) {
            for(int i=0; i < ctx_to_free->num_data_columns; ++i) {
                g_free(ctx_to_free->original_column_names_in_db[i]); // Освобождаем каждую g_strdup-строку
            }
            g_free(ctx_to_free->original_column_names_in_db);
            ctx_to_free->original_column_names_in_db = NULL;
        }
        // iter_to_edit не требует специального free, так как это копия структуры, а не указатель
        g_free(ctx_to_free);
    }
}

static void show_edit_dialog(AppData *data, GtkTreeModel *model, GtkTreeIter *iter) {
    // Проверка входных данных
    if (!data || !data->parent) {
        g_warning("show_edit_dialog: AppData или родительское окно не инициализированы.");
        return;
    }
    // Проверка model и iter уже должна быть сделана в вызывающей функции (on_edit_row_clicked)
    // gtk_tree_selection_get_selected возвращает TRUE, если iter валиден.
    if (!model || !iter) { // Простая проверка на NULL
        show_message_dialog(GTK_WINDOW(data->parent), "Ошибка: Некорректные параметры для редактирования строки.");
        return;
    }
    if (!data->table_data_main || data->table_data_main->column_count <= 0 ||
        !data->table_data_main->column_names) {
        show_message_dialog(GTK_WINDOW(data->parent), "Ошибка: Нет данных или имен колонок для редактирования в основной таблице.");
        return;
    }

    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *grid;
    EditDialogContext *edit_ctx = NULL;

    dialog = gtk_dialog_new_with_buttons("Редактировать строку",
                                         GTK_WINDOW(data->parent),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Сохранить", GTK_RESPONSE_ACCEPT,
                                         "_Отмена", GTK_RESPONSE_CANCEL,
                                         NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();
    gtk_widget_set_margin_start(grid, 10); gtk_widget_set_margin_end(grid, 10);
    gtk_widget_set_margin_top(grid, 10); gtk_widget_set_margin_bottom(grid, 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_append(GTK_BOX(content_area), grid);

    int num_data_cols = data->table_data_main->column_count;
    edit_ctx = g_new0(EditDialogContext, 1);
    if (!edit_ctx) {
        g_critical("show_edit_dialog: Не удалось выделить память для EditDialogContext!");
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }

    edit_ctx->app_data = data;
    edit_ctx->store_to_edit = data->store_main;
    edit_ctx->iter_to_edit = *iter; // Прямое копирование содержимого структуры итератора
    edit_ctx->data_was_saved = FALSE;
    edit_ctx->num_data_columns = num_data_cols;
    
    edit_ctx->entries_array = g_new0(GtkWidget*, num_data_cols);
    if (!edit_ctx->entries_array) {
        g_critical("show_edit_dialog: Не удалось выделить память для entries_array!");
        g_free(edit_ctx);
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }
    edit_ctx->original_column_names_in_db = g_new0(char*, num_data_cols);
    if (!edit_ctx->original_column_names_in_db) {
        g_critical("show_edit_dialog: Не удалось выделить память для original_column_names_in_db!");
        g_free(edit_ctx->entries_array);
        g_free(edit_ctx);
        gtk_window_destroy(GTK_WINDOW(dialog));
        return;
    }

    gtk_tree_model_get(model, iter, 0, &edit_ctx->original_row_id_value, -1);
    debug_print("show_edit_dialog: Редактирование строки с ID/ROWID = %d\n", edit_ctx->original_row_id_value);

    for (int i = 0; i < num_data_cols; i++) {
        const char *col_name_for_label = (data->table_data_main->column_names[i + 1])
                               ? data->table_data_main->column_names[i + 1]
                               : "Неизвестно";
        edit_ctx->original_column_names_in_db[i] = data->table_data_main->column_names[i + 1] ?
                                                   g_strdup(data->table_data_main->column_names[i + 1]) : g_strdup("Неизвестно");

        GtkWidget *label = gtk_label_new(col_name_for_label);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        edit_ctx->entries_array[i] = gtk_entry_new();
        gtk_widget_set_hexpand(edit_ctx->entries_array[i], TRUE);

        double current_value;
        gtk_tree_model_get(model, iter, i + 1, &current_value, -1);
        char buffer[50];
        if (isnan(current_value)) {
            snprintf(buffer, sizeof(buffer), "");
        } else {
            snprintf(buffer, sizeof(buffer), "%.4f", current_value);
        }
        gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(edit_ctx->entries_array[i])), buffer, -1);

        gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), edit_ctx->entries_array[i], 1, i, 1, 1);
    }

    g_object_set_data_full(G_OBJECT(dialog), "edit-dialog-context", edit_ctx, free_edit_dialog_context);
    g_signal_connect(dialog, "response", G_CALLBACK(on_edit_dialog_response), edit_ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}


static void on_edit_row_clicked(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!data->tree_view_main) return;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view_main));
    if (!selection) return;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (model == GTK_TREE_MODEL(data->store_main)) { // Убедимся, что это правильная модель
            show_edit_dialog(data, model, &iter);
        } else {
            show_message_dialog(GTK_WINDOW(data->parent), "Выбрана строка из неожиданной модели.");
        }
    } else {
        show_message_dialog(GTK_WINDOW(data->parent), "Сначала выберите строку для редактирования.");
    }
}

// Обработчик изменения выделения в основной таблице
static void on_main_table_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    AppData *data = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean row_selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    
    // Активируем кнопку редактирования, только если выбрана строка и это основная таблица
    if (data->edit_row_button) {
        GtkTreeView *tree_view_of_selection = gtk_tree_selection_get_tree_view(selection);
        if (tree_view_of_selection == GTK_TREE_VIEW(data->tree_view_main)) {
            gtk_widget_set_sensitive(data->edit_row_button, row_selected);
        } else {
            gtk_widget_set_sensitive(data->edit_row_button, FALSE); // Если выделение не в основной таблице
        }
    }
}

// --- Реализация activate (static) ---
static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = user_data;
    GtkWidget *window;
    GtkWidget *button_box;
    GtkWidget *level_button_other;
    GtkWidget *main_paned;
    GtkWidget *right_panel_box;
    GtkWidget* label_values_table;
    GtkWidget* label_schema_image;
    GtkWidget* frame_picture;
    const char* levels[] = {"Уровень 2", "Уровень 3", "Уровень 4"};
    int i;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Анализ данных временных рядов");

    // Максимизация окна (код из предыдущих ответов)
    GdkDisplay *display = gtk_widget_get_display(window);
    GdkMonitor *monitor = NULL;
    if (display) {
        GListModel *monitors = gdk_display_get_monitors(display);
        if (monitors && g_list_model_get_n_items(monitors) > 0) {
            monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
        }
        // g_object_unref(monitors); // Не требуется для GListModel, возвращаемого gdk_display_get_monitors
    }
    if (monitor) {
        gtk_window_maximize(GTK_WINDOW(window));
    } else {
        gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    }

    data->parent = GTK_WINDOW(window);

    data->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(data->main_box, 5);
    gtk_widget_set_margin_bottom(data->main_box, 5);
    gtk_widget_set_margin_start(data->main_box, 5);
    gtk_widget_set_margin_end(data->main_box, 5);
    gtk_window_set_child(GTK_WINDOW(window), data->main_box);

    // Кнопки уровней
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(data->main_box), button_box);

    data->level1_button = gtk_button_new_with_label("Уровень 1");
    gtk_widget_set_sensitive(data->level1_button, FALSE);
    g_signal_connect(data->level1_button, "clicked", G_CALLBACK(on_level_clicked), data);
    gtk_box_append(GTK_BOX(button_box), data->level1_button);

    for (i = 0; i < 3; ++i) {
        level_button_other = gtk_button_new_with_label(levels[i]);
        gtk_widget_set_sensitive(level_button_other, FALSE);
        g_signal_connect(level_button_other, "clicked", G_CALLBACK(on_level_clicked), data);
        gtk_box_append(GTK_BOX(button_box), level_button_other);
    }

    // Кнопка "Открыть SQLite файл"
    data->open_button = gtk_button_new_with_label("Открыть SQLite файл");
    gtk_widget_set_halign(data->open_button, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(data->open_button, 5);
    gtk_widget_set_margin_bottom(data->open_button, 5);
    gtk_box_append(GTK_BOX(data->main_box), data->open_button);
    g_signal_connect(data->open_button, "clicked", G_CALLBACK(open_sqlite_file), data);

    // GtkPaned
    main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(main_paned, TRUE);
    gtk_widget_set_hexpand(main_paned, TRUE);
    gtk_paned_set_wide_handle(GTK_PANED(main_paned), TRUE);

    // Левая панель (main_table)
    data->scrolled_window_main = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window_main), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(data->scrolled_window_main, TRUE);
    gtk_widget_set_halign(data->scrolled_window_main, GTK_ALIGN_FILL);
    data->tree_view_main = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view_main), TRUE);
    if (GTK_IS_WIDGET(data->tree_view_main)) {
        g_object_set_data(G_OBJECT(data->tree_view_main), "app_data_ptr_for_debug", data);
    }
    // Подключаем обработчик изменения выделения
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view_main));
    g_signal_connect(selection, "changed", G_CALLBACK(on_main_table_selection_changed), data);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data->scrolled_window_main), data->tree_view_main);
    gtk_widget_set_visible(data->tree_view_main, FALSE);
    gtk_paned_set_start_child(GTK_PANED(main_paned), data->scrolled_window_main);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), TRUE);

    // Правая панель (values и schema)
    right_panel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(right_panel_box, 5);
    gtk_widget_set_hexpand(right_panel_box, FALSE);
    gtk_widget_set_halign(right_panel_box, GTK_ALIGN_START);

    label_values_table = gtk_label_new("Таблица 'Значения':");
    gtk_widget_set_halign(label_values_table, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(right_panel_box), label_values_table);
    data->scrolled_window_values = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window_values), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_set_size_request(data->scrolled_window_values, -1, 150);
    gtk_widget_set_vexpand(data->scrolled_window_values, FALSE);
    data->tree_view_values = gtk_tree_view_new();
    if (GTK_IS_WIDGET(data->tree_view_values)) {
        g_object_set_data(G_OBJECT(data->tree_view_values), "app_data_ptr_for_debug", data);
    }
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view_values), TRUE);
    gtk_widget_set_visible(data->tree_view_values, FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data->scrolled_window_values), data->tree_view_values);
    gtk_box_append(GTK_BOX(right_panel_box), data->scrolled_window_values);

    data->edit_values_button = gtk_button_new_with_label("Редактировать 'Значения'"); // Добавь edit_values_button в AppData
    gtk_widget_set_halign(data->edit_values_button, GTK_ALIGN_START); // Выравнивание
    gtk_widget_set_margin_top(data->edit_values_button, 5);
    gtk_box_append(GTK_BOX(right_panel_box), data->edit_values_button);
    g_signal_connect(data->edit_values_button, "clicked", G_CALLBACK(on_edit_values_row_clicked), data); // Новый обработчик
    gtk_widget_set_sensitive(data->edit_values_button, FALSE); // Изначально неактивна

    // Подключаем обработчик изменения выделения для таблицы "Значения"
    GtkTreeSelection *values_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view_values));
    g_signal_connect(values_selection, "changed", G_CALLBACK(on_values_table_selection_changed), data); // Новый обработчик

    label_schema_image = gtk_label_new("Схема:");
    gtk_widget_set_halign(label_schema_image, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label_schema_image, 10);
    gtk_box_append(GTK_BOX(right_panel_box), label_schema_image);
    data->picture_schema = gtk_picture_new();
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(data->picture_schema), TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(data->picture_schema), TRUE);
    gtk_widget_set_visible(data->picture_schema, FALSE);
    frame_picture = gtk_frame_new(NULL);
    gtk_widget_set_vexpand(frame_picture, TRUE);
    gtk_frame_set_child(GTK_FRAME(frame_picture), data->picture_schema);
    gtk_box_append(GTK_BOX(right_panel_box), frame_picture);

    gtk_paned_set_end_child(GTK_PANED(main_paned), right_panel_box);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(main_paned), FALSE);
    gtk_widget_set_size_request(right_panel_box, 300, -1);

    gtk_box_append(GTK_BOX(data->main_box), main_paned);

    // Кнопки управления основной таблицей
    GtkWidget *table_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(table_actions_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(table_actions_box, 5);
    gtk_box_append(GTK_BOX(data->main_box), table_actions_box);

    data->add_row_button = gtk_button_new_with_label("Добавить строку");
    gtk_box_append(GTK_BOX(table_actions_box), data->add_row_button);
    g_signal_connect(data->add_row_button, "clicked", G_CALLBACK(add_new_row), data);
    gtk_widget_set_sensitive(data->add_row_button, FALSE);

    // НОВАЯ КНОПКА "Редактировать строку"
    data->edit_row_button = gtk_button_new_with_label("Редактировать строку"); // Убедись, что edit_row_button есть в AppData
    gtk_box_append(GTK_BOX(table_actions_box), data->edit_row_button);
    g_signal_connect(data->edit_row_button, "clicked", G_CALLBACK(on_edit_row_clicked), data);
    gtk_widget_set_sensitive(data->edit_row_button, FALSE); // Изначально неактивна

    gtk_window_present(GTK_WINDOW(window));

    // Установка позиции разделителя GtkPaned
    int window_width = gtk_widget_get_width(GTK_WIDGET(window));
    int desired_right_panel_width = 300;
    int paned_gutter_spacing = 10;
    int min_allowable_left_width = 100;
    int min_allowable_right_width = 50;
    int position_for_left = window_width - desired_right_panel_width - paned_gutter_spacing;
    if (position_for_left < min_allowable_left_width) {
        position_for_left = min_allowable_left_width;
    }
    if ((window_width - position_for_left - paned_gutter_spacing) < min_allowable_right_width) {
        position_for_left = window_width - min_allowable_right_width - paned_gutter_spacing;
        if (position_for_left < min_allowable_left_width) {
             position_for_left = min_allowable_left_width;
        }
    }
    if (position_for_left < 0 ) position_for_left = 0;
    if (position_for_left > window_width - paned_gutter_spacing) position_for_left = window_width - paned_gutter_spacing;
    gtk_paned_set_position(GTK_PANED(main_paned), position_for_left);
}

// --- Функция main ---
int main(int argc, char **argv) {
    init_debug_log(); // Инициализация лога
    debug_print("main: Программа запущена\n");

    GtkApplication *app = gtk_application_new("org.example.timeseriesanalyzer", G_APPLICATION_DEFAULT_FLAGS);

    TableData *table_data_main_instance = g_new0(TableData, 1);
    TableData *table_data_values_instance = g_new0(TableData, 1);
    static sqlite3 *db_instance = NULL; // Статический, чтобы адрес не менялся

    AppData *data = g_new0(AppData, 1);
    data->db = &db_instance; // Передаем адрес указателя
    data->table_data_main = table_data_main_instance;
    data->table_data_values = table_data_values_instance;
    data->values_table_name = "Значения";
    data->schema_blob_column_name = "Схема";

    g_signal_connect(app, "activate", G_CALLBACK(activate), data);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    debug_print("main: Приложение завершило работу со статусом %d\n", status);

    // === Очистка ресурсов ===
    debug_print("main: Очистка ресурсов...\n");
    if (data->db && *data->db) { sqlite3_close(*data->db); *data->db = NULL; }
    if (data->table_data_main) {
        if (data->table_data_main->table) { for (int i=0;i<data->table_data_main->row_count;++i) if(data->table_data_main->table[i]) g_free(data->table_data_main->table[i]); g_free(data->table_data_main->table); }
        if (data->table_data_main->epochs) g_free(data->table_data_main->epochs);
        if (data->table_data_main->column_names) { int num_n=data->table_data_main->column_count+1; for(int i=0;i<num_n;++i) if(data->table_data_main->column_names[i]) g_free(data->table_data_main->column_names[i]); g_free(data->table_data_main->column_names); }
        g_free(data->table_data_main);
    }
    g_free(data->current_main_table_name);
    if (data->table_data_values) {
        if (data->table_data_values->table) { for (int i=0;i<data->table_data_values->row_count;++i) if(data->table_data_values->table[i]) g_free(data->table_data_values->table[i]); g_free(data->table_data_values->table); }
        if (data->table_data_values->epochs) g_free(data->table_data_values->epochs);
        if (data->table_data_values->column_names) { int num_n=data->table_data_values->column_count+1; for(int i=0;i<num_n;++i) if(data->table_data_values->column_names[i]) g_free(data->table_data_values->column_names[i]); g_free(data->table_data_values->column_names); }
        g_free(data->table_data_values);
    }
    if (data->pixbuf_schema) g_object_unref(data->pixbuf_schema);
    level1_cleanup_calculated_data(data); // Очистка данных L1
    g_free(data); data = NULL;

    if (app) g_object_unref(app);
    if (debug_log) { fprintf(debug_log, "main: Лог-файл закрыт\n"); fclose(debug_log); debug_log = NULL; }
    // Логгер level1_analysis.c закрывается при необходимости внутри себя (например, при закрытии программы), либо можно сделать публичную функцию
    // Или добавить fclose(debug_log_l1) сюда, если debug_log_l1 будет экспортирован.
    return status;
}

#pragma GCC diagnostic pop