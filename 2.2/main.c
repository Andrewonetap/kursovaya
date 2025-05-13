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
void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table);
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
    debug_print("show_message_dialog: %s\n", message);
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
    // Проверка на NULL для parent, хотя gtk_alert_dialog_show должен это обрабатывать
    gtk_alert_dialog_set_buttons(dialog, (const char *[]){"OK", NULL});
    gtk_alert_dialog_show(dialog, parent); // parent может быть NULL
    g_object_unref(dialog);
}


// --- Реализация update_tree_view_columns (non-static) ---
void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table) {
    GtkWidget *tree_view;
    GtkListStore **store_ptr;
    TableData* current_table_data;
    const char *context_str_debug = "UNKNOWN";

    if (is_smoothing_alpha_table) {
        tree_view = data->level1_alpha_smoothing_view;
        store_ptr = &data->level1_alpha_smoothing_store;
        current_table_data = data->table_data_main; // Данные для сглаживания берутся из основной таблицы (точнее, из mu/alpha)
        context_str_debug = "L1_SM_ALPHA";
    } else if (is_smoothing_mu_table) {
        tree_view = data->level1_smoothing_view;
        store_ptr = &data->level1_smoothing_store;
        current_table_data = data->table_data_main;
        context_str_debug = "L1_SM_MU";
    } else if (is_level1_context) {
        tree_view = data->level1_tree_view;
        store_ptr = &data->level1_store;
        current_table_data = data->table_data_main; // Основная таблица Уровня 1 отображает данные из table_data_main
        context_str_debug = "L1_MAIN";
    } else { // Это MAIN_TABLE в главном окне
        tree_view = data->tree_view_main;
        store_ptr = &data->store_main;
        current_table_data = data->table_data_main;
        context_str_debug = "MAIN_TABLE";
    }
    debug_print("update_tree_view_columns (%s): Начало. AppData=%p, TreeView=%p\n", context_str_debug, (void*)data, (void*)tree_view);
     if (!tree_view) {
         debug_print("update_tree_view_columns (%s): Ошибка: tree_view is NULL!\n", context_str_debug);
         return;
     }

    GList *columns_list = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
    for (GList *l = columns_list; l; l = l->next) {
        gtk_tree_view_remove_column(GTK_TREE_VIEW(tree_view), GTK_TREE_VIEW_COLUMN(l->data));
    }
    g_list_free_full(columns_list, g_object_unref);
    debug_print("update_tree_view_columns (%s): Старые столбцы удалены.\n", context_str_debug);

    if (*store_ptr) {
        debug_print("update_tree_view_columns (%s): Unref старого store %p (ref_count до: %u)\n",
                    context_str_debug, (void*)*store_ptr, G_IS_OBJECT(*store_ptr) ? G_OBJECT(*store_ptr)->ref_count : 0);
        g_object_unref(*store_ptr);
        *store_ptr = NULL;
    }

    int model_column_count;
    // data_col_count_local используется для таблиц L1_MAIN и MAIN_TABLE, это кол-во колонок данных в table_data_main
    int data_col_count_local = (current_table_data && (is_level1_context || (!is_smoothing_mu_table && !is_smoothing_alpha_table))) ? current_table_data->column_count : 0;


    if (is_smoothing_mu_table || is_smoothing_alpha_table) {
        model_column_count = 6; // M, A=0.1, A=0.4, A=0.7, A=0.9, Real
    } else if (is_level1_context) {
        // ID + X1..XN_data + mu + alpha
        model_column_count = 1 + data_col_count_local + 2;
    } else { // MAIN_TABLE
        // ID + X1..XN_data
        model_column_count = 1 + data_col_count_local;
    }

    // Защита от current_table_data == NULL, если он нужен для расчета data_col_count_local
     if (current_table_data == NULL && (is_level1_context || (!is_smoothing_mu_table && !is_smoothing_alpha_table))) {
         debug_print("update_tree_view_columns (%s): current_table_data is NULL, сбрасываю data_col_count_local на 0, model_column_count будет 1 или 1+2 (для L1).\n", context_str_debug);
         data_col_count_local = 0; // Пересчитаем model_column_count
         if (is_level1_context) model_column_count = 1 + 0 + 2; // ID, mu, alpha
         else model_column_count = 1 + 0; // Только ID
     }


    GType *types = g_new(GType, model_column_count);
    types[0] = G_TYPE_INT; // ID или M - всегда int
    for (int i = 1; i < model_column_count; i++) {
        types[i] = G_TYPE_DOUBLE; // Все остальные данные - double
    }
    *store_ptr = gtk_list_store_newv(model_column_count, types);
    g_free(types);
    debug_print("update_tree_view_columns (%s): Новый GtkListStore %p создан с %d столбцами.\n", context_str_debug, (void*)*store_ptr, model_column_count);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    // Колонка ID / M
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    const char* id_col_title = "ID_ERR"; // Default
     if (is_smoothing_mu_table || is_smoothing_alpha_table) {
         id_col_title = "M";
     } else if (current_table_data && current_table_data->column_names && current_table_data->column_names[0]){
         id_col_title = current_table_data->column_names[0]; // Имя первой колонки из БД
     } else if (is_level1_context || (!is_smoothing_mu_table && !is_smoothing_alpha_table)) { // L1_MAIN или MAIN_TABLE без данных
         id_col_title = "Эпоха"; // Запасной вариант
     }
    col = gtk_tree_view_column_new_with_attributes(id_col_title, renderer, "text", 0, NULL); // Биндинг на 0-ю колонку модели
    gtk_tree_view_column_set_fixed_width(col, 60);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
    debug_print("update_tree_view_columns (%s): Колонка ID/M '%s' (модель 0) добавлена.\n", context_str_debug, id_col_title);


    // Настройка остальных колонок в зависимости от контекста
    if (is_smoothing_mu_table || is_smoothing_alpha_table) {
        debug_print("update_tree_view_columns (%s): Добавление колонок для таблиц сглаживания...\n", context_str_debug);
        const char *titles[] = {"A=0.1", "A=0.4", "A=0.7", "A=0.9", is_smoothing_alpha_table ? "α (реальн.)" : "μ (реальн.)"};
        for (int i = 0; i < 5; i++) { // 5 колонок: 4 для A, 1 для реальных данных
            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(col, titles[i]);
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            // Индексы в модели: 1 (A=0.1), 2 (A=0.4), ..., 5 (реальные)
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(i + 1), NULL);

            if (is_smoothing_alpha_table) {
                // Для ВСЕХ колонок таблицы сглаживания альфа ставим флаг "is-alpha"
                // чтобы cell_data_func использовала формат "%.2E"
                g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1));
                if (i == 4) { // Колонка "α (реальн.)"
                    gtk_tree_view_column_set_fixed_width(col, 120);
                } else { // Колонки A=0.1, A=0.4, A=0.7, A=0.9 для альфа
                    gtk_tree_view_column_set_fixed_width(col, 95); // Немного шире для E-формата
                }
            } else { // Это is_smoothing_mu_table
                // Для таблицы сглаживания мю, только последняя колонка "μ (реальн.)" имеет специальный флаг.
                // Остальные колонки (A=...) будут использовать формат по умолчанию из cell_data_func ("%.4f").
                if (i == 4) { // Колонка "μ (реальн.)"
                    g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1));
                    gtk_tree_view_column_set_fixed_width(col, 120);
                } else { // Колонки A=0.1, A=0.4, A=0.7, A=0.9 для мю
                    gtk_tree_view_column_set_fixed_width(col, 80);
                }
            }
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
        }
    } else if (is_level1_context) { // Таблица L1_MAIN (данные X, μ, α)
        debug_print("update_tree_view_columns (%s): Добавление колонок для L1_MAIN (read-only data)...\n", context_str_debug);
        if (current_table_data && current_table_data->column_names) {
            // Колонки данных X1, X2... (НЕредактируемые)
            for (int i = 0; i < data_col_count_local; i++) {
                const char* col_title_data = (current_table_data->column_names[i + 1]) ? current_table_data->column_names[i + 1] : "ERR_COL";
                gint model_col_idx = i + 1; // Индекс в модели (начиная с 1)
                renderer = gtk_cell_renderer_text_new();
                g_object_set(renderer, "xalign", 0.5, NULL);
                g_object_set(renderer, "editable", FALSE, NULL); // НЕ редактируемые для L1

                col = gtk_tree_view_column_new();
                gtk_tree_view_column_set_title(col, col_title_data);
                gtk_tree_view_column_pack_start(col, renderer, TRUE);
                gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(model_col_idx), NULL);
                gtk_tree_view_column_set_fixed_width(col, 80);
                gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
            // Колонка μ
            int mu_model_idx = data_col_count_local + 1;
            if (mu_model_idx < model_column_count) { // Проверка, что колонка помещается в модель
                 renderer = gtk_cell_renderer_text_new();
                 g_object_set(renderer, "xalign", 0.5, NULL);
                 col = gtk_tree_view_column_new();
                 gtk_tree_view_column_set_title(col, "μ");
                 gtk_tree_view_column_pack_start(col, renderer, TRUE);
                 gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(mu_model_idx), NULL);
                 g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1));
                 gtk_tree_view_column_set_fixed_width(col, 80);
                 gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
            // Колонка α
            int alpha_model_idx = data_col_count_local + 2;
            if (alpha_model_idx < model_column_count) { // Проверка
                 renderer = gtk_cell_renderer_text_new();
                 g_object_set(renderer, "xalign", 0.5, NULL);
                 col = gtk_tree_view_column_new();
                 gtk_tree_view_column_set_title(col, "α");
                 gtk_tree_view_column_pack_start(col, renderer, TRUE);
                 gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(alpha_model_idx), NULL);
                 g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1));
                 gtk_tree_view_column_set_fixed_width(col, 100);
                 gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
        }
    } else { // MAIN_TABLE (главная таблица в основном окне - редактируемые данные)
        debug_print("update_tree_view_columns (%s): Добавление колонок для MAIN_TABLE (editable data)...\n", context_str_debug);
        if (current_table_data && current_table_data->column_names) {
            for (int i = 0; i < data_col_count_local; i++) {
                const char* col_title_data = (current_table_data->column_names[i + 1]) ? current_table_data->column_names[i + 1] : "ERR_COL_NAME";
                gint model_col_idx = i + 1;
                if (model_col_idx >= model_column_count) {
                    debug_print(" ! update_tree_view_columns (%s): model_col_idx %d >= model_column_count %d. Пропуск колонки '%s'\n",
                                context_str_debug, model_col_idx, model_column_count, col_title_data);
                    continue;
                }
                renderer = gtk_cell_renderer_text_new();
                g_object_set(renderer, "xalign", 0.5, NULL);
                g_object_set(renderer, "editable", TRUE, NULL); // РЕДАКТИРУЕМЫЕ
                // Установка данных для обработчика on_cell_edited
                g_object_set_data(G_OBJECT(renderer), "tree-view-origin", "main"); // "main" - значит, вызывается из основного tree_view или l1_tree_view
                g_object_set_data(G_OBJECT(renderer), "model-column-index", GINT_TO_POINTER(model_col_idx));
                g_object_set_data_full(G_OBJECT(renderer), "db-column-name", g_strdup(col_title_data), (GDestroyNotify)g_free);
                g_signal_connect(renderer, "edited", G_CALLBACK(on_cell_edited), data);

                col = gtk_tree_view_column_new();
                gtk_tree_view_column_set_title(col, col_title_data);
                gtk_tree_view_column_pack_start(col, renderer, TRUE);
                // gtk_tree_view_column_add_attribute(col, renderer, "text", model_col_idx); // Используем cell_data_func для форматирования
                gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(model_col_idx), NULL);
                gtk_tree_view_column_set_fixed_width(col, 80);
                gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
        }
    } // КОНЕЦ ВНЕШНЕГО IF/ELSE IF/ELSE ДЛЯ КОНТЕКСТОВ

    // Установка модели в TreeView
    if (*store_ptr) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(*store_ptr));
        debug_print("update_tree_view_columns (%s): Модель %p установлена для TreeView %p.\n", context_str_debug, (void*)*store_ptr, (void*)tree_view);
    } else {
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), NULL); // На всякий случай, если store не создан
        debug_print("update_tree_view_columns (%s): Store is NULL, модель не установлена для TreeView %p.\n", context_str_debug, (void*)tree_view);
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
        renderer_val = gtk_cell_renderer_text_new(); g_object_set(renderer_val, "editable", TRUE, "xalign", 0.5, NULL);
        g_object_set_data(G_OBJECT(renderer_val), "tree-view-origin", "values");
        g_object_set_data(G_OBJECT(renderer_val), "model-column-index", GINT_TO_POINTER(model_idx_in_store));
        g_object_set_data_full(G_OBJECT(renderer_val), "db-column-name", g_strdup(col_db_name_val), (GDestroyNotify)g_free);
        g_signal_connect(renderer_val, "edited", G_CALLBACK(on_cell_edited), data);
        col_val = gtk_tree_view_column_new(); gtk_tree_view_column_set_title(col_val, col_db_name_val);
        gtk_tree_view_column_pack_start(col_val, renderer_val, TRUE);
        if (g_strcmp0(col_db_name_val, "Количество") == 0) g_object_set_data(G_OBJECT(col_val), "is-integer-like", GINT_TO_POINTER(1));
        gtk_tree_view_column_add_attribute(col_val, renderer_val, "text", model_idx_in_store);
        gtk_tree_view_column_set_cell_data_func(col_val, renderer_val, cell_data_func, GINT_TO_POINTER(model_idx_in_store), NULL);
        gtk_tree_view_column_set_fixed_width(col_val, 100); gtk_tree_view_append_column(treeview, col_val);
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
    update_tree_view_columns(data, FALSE, FALSE, FALSE); // Обновляет data->tree_view_main и data->store_main

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
    if (data->table_data_main->table) { /* ... */ } /* и другие поля table_data_main */ g_free(data->current_main_table_name); data->current_main_table_name=NULL;
    // ...Полная очистка data->table_data_main как в load_and_display_main_table
    if (data->table_data_main->table) { for (int i=0;i<data->table_data_main->row_count;++i) if(data->table_data_main->table[i]) g_free(data->table_data_main->table[i]); g_free(data->table_data_main->table); data->table_data_main->table = NULL; }
    if (data->table_data_main->epochs) {g_free(data->table_data_main->epochs);data->table_data_main->epochs=NULL;}
    if (data->table_data_main->column_names) {int n=data->table_data_main->column_count+1; for(int i=0;i<n;++i) if(data->table_data_main->column_names[i]) g_free(data->table_data_main->column_names[i]);g_free(data->table_data_main->column_names);data->table_data_main->column_names=NULL;}
    data->table_data_main->row_count = 0; data->table_data_main->column_count = 0;
    // ИСПРАВЛЕНИЕ: Вызываем update_tree_view_columns с флагами FALSE, чтобы очистить UI главной таблицы
    if(data->tree_view_main) update_tree_view_columns(data, FALSE, FALSE, FALSE); // Это удалит колонки и store
    if(data->tree_view_main) gtk_widget_set_visible(data->tree_view_main, FALSE);

    if (data->table_data_values->table) { /* ... */ } /* и другие поля table_data_values */
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
    AppData *data = user_data; GtkTreePath *path = gtk_tree_path_new_from_string(path_string); if (!path) return;
    GtkTreeIter iter; gboolean path_valid; GtkTreeModel *model = NULL; GtkTreeView *tree_view_edited = NULL;
    const char* tree_view_id = g_object_get_data(G_OBJECT(renderer), "tree-view-origin");
    TableData *current_table_data_ref = NULL; GtkListStore *current_store = NULL; const char *current_table_name = NULL; const char *id_col_db_name = NULL; gboolean is_values_tbl = FALSE;

    if (tree_view_id && g_strcmp0(tree_view_id, "main") == 0) {
        // ПРОВЕРКА: Какой из main tree_view редактируется
        GtkWidget *ancestor_tree_view = gtk_widget_get_ancestor(GTK_WIDGET(renderer), GTK_TYPE_TREE_VIEW);
        if (data->level1_tree_view && GTK_WIDGET(data->level1_tree_view) == ancestor_tree_view) {
             tree_view_edited = GTK_TREE_VIEW(data->level1_tree_view); model = GTK_TREE_MODEL(data->level1_store);
        } else if (data->tree_view_main && GTK_WIDGET(data->tree_view_main) == ancestor_tree_view) {
            tree_view_edited = GTK_TREE_VIEW(data->tree_view_main); model = GTK_TREE_MODEL(data->store_main);
        } else { gtk_tree_path_free(path); return; }
        current_table_data_ref = data->table_data_main; current_store = GTK_LIST_STORE(model); current_table_name = data->current_main_table_name;
        if (current_table_data_ref && current_table_data_ref->column_names && current_table_data_ref->column_names[0]) id_col_db_name = current_table_data_ref->column_names[0];
        else { gtk_tree_path_free(path); return; }
    } else if (tree_view_id && g_strcmp0(tree_view_id, "values") == 0) {
        tree_view_edited = GTK_TREE_VIEW(data->tree_view_values); model = GTK_TREE_MODEL(data->store_values);
        current_table_data_ref = data->table_data_values; current_store = data->store_values; current_table_name = data->values_table_name; is_values_tbl = TRUE;
    } else { gtk_tree_path_free(path); return; }
    if (!model) { gtk_tree_path_free(path); return; }
    path_valid = gtk_tree_model_get_iter(model, &iter, path); if (!path_valid) { gtk_tree_path_free(path); return; }
    gint model_column_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "model-column-index"));
    const char *db_column_name = g_object_get_data(G_OBJECT(renderer), "db-column-name"); if (!db_column_name) { gtk_tree_path_free(path); return; }
    if (data->schema_blob_column_name && g_strcmp0(db_column_name, data->schema_blob_column_name) == 0) { gtk_tree_path_free(path); return; }
    gdouble new_value; char *endptr; gchar *temp_text = g_strdup(new_text); if(!temp_text){ gtk_tree_path_free(path); return; }
    for (char *p = temp_text; *p; ++p) if (*p == ',') *p = '.';
    errno = 0; new_value = g_ascii_strtod(temp_text, &endptr);
    if (errno == ERANGE || endptr == temp_text || (*endptr != '\0' && !g_ascii_isspace(*endptr))) {
        show_message_dialog(data->parent, "Ошибка: Неверный формат числа."); g_free(temp_text); gtk_tree_path_free(path); return;
    }
    g_free(temp_text);
    gint row_id_value; gtk_tree_model_get(model, &iter, 0, &row_id_value, -1);
    if (!data->db || !*data->db) { show_message_dialog(data->parent, "Ошибка: База данных закрыта."); gtk_tree_path_free(path); return; }
    gboolean updated_db = update_table_cell( *data->db, current_table_name, db_column_name, id_col_db_name, new_value, row_id_value );
    if (updated_db) {
        gtk_list_store_set(current_store, &iter, model_column_index, new_value, -1);
        if (!is_values_tbl) {
             load_table(*data->db, data->table_data_main, data->current_main_table_name); // Перезагрузка TableData
             level1_update_content_if_visible(data);
        } else {
            load_table(*data->db, data->table_data_values, data->values_table_name); // Перезагрузка TableData
        }
    } else { show_message_dialog(data->parent, "Ошибка: Не удалось обновить значение в БД."); }
    gtk_tree_path_free(path);
}

// --- Реализация activate (static) ---
static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = user_data;
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Анализ данных временных рядов");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    data->parent = GTK_WINDOW(window);

    data->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(data->main_box, 5);
    gtk_widget_set_margin_bottom(data->main_box, 5);
    gtk_widget_set_margin_start(data->main_box, 5);
    gtk_widget_set_margin_end(data->main_box, 5);
    gtk_window_set_child(GTK_WINDOW(window), data->main_box);

    // Кнопки уровней
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(data->main_box), button_box);

    data->level1_button = gtk_button_new_with_label("Уровень 1");
    gtk_widget_set_sensitive(data->level1_button, FALSE);
    g_signal_connect(data->level1_button, "clicked", G_CALLBACK(on_level_clicked), data);
    gtk_box_append(GTK_BOX(button_box), data->level1_button);

    const char* levels[] = {"Уровень 2", "Уровень 3", "Уровень 4"};
    for (int i = 0; i < 3; ++i) {
        GtkWidget *level_button_other = gtk_button_new_with_label(levels[i]);
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

    // --- GtkPaned для разделения на левую и правую части ---
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(main_paned, TRUE);
    gtk_widget_set_hexpand(main_paned, TRUE);
    gtk_paned_set_wide_handle(GTK_PANED(main_paned), TRUE); // Широкий разделитель

    // --- Левая панель (main_table) ---
    data->scrolled_window_main = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window_main), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    data->tree_view_main = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view_main), TRUE);
    if (GTK_IS_WIDGET(data->tree_view_main)) {
        g_object_set_data(G_OBJECT(data->tree_view_main), "app_data_ptr_for_debug", data);
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data->scrolled_window_main), data->tree_view_main);
    gtk_widget_set_visible(data->tree_view_main, FALSE); // Изначально невидима

    gtk_paned_set_start_child(GTK_PANED(main_paned), data->scrolled_window_main);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), TRUE); // Левая панель ДОЛЖНА изменять размер
    // gtk_widget_set_size_request(data->scrolled_window_main, 400, -1); // Можно закомментировать или убрать


    // --- Правая панель (values и schema) ---
    GtkWidget *right_panel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(right_panel_box, 5);

    GtkWidget* label_values_table = gtk_label_new("Таблица 'Значения':");
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

    GtkWidget* label_schema_image = gtk_label_new("Схема:");
    gtk_widget_set_halign(label_schema_image, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label_schema_image, 10);
    gtk_box_append(GTK_BOX(right_panel_box), label_schema_image);

    data->picture_schema = gtk_picture_new();
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(data->picture_schema), TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(data->picture_schema), TRUE);
    gtk_widget_set_visible(data->picture_schema, FALSE);
    GtkWidget* frame_picture = gtk_frame_new(NULL);
    gtk_widget_set_vexpand(frame_picture, TRUE);
    gtk_frame_set_child(GTK_FRAME(frame_picture), data->picture_schema);
    gtk_box_append(GTK_BOX(right_panel_box), frame_picture);

    gtk_paned_set_end_child(GTK_PANED(main_paned), right_panel_box);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), FALSE); // Правая панель НЕ ДОЛЖНА изменять размер
    gtk_widget_set_size_request(right_panel_box, 300, -1);      // Задаем ей желаемую ширину


    gtk_box_append(GTK_BOX(data->main_box), main_paned); // Добавляем GtkPaned в главный vbox

    // Кнопка "Добавить строку"
    data->add_row_button = gtk_button_new_with_label("Добавить строку (в основную таблицу)");
    gtk_widget_set_halign(data->add_row_button, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(data->add_row_button, 5);
    gtk_box_append(GTK_BOX(data->main_box), data->add_row_button);
    g_signal_connect(data->add_row_button, "clicked", G_CALLBACK(add_new_row), data);
    gtk_widget_set_sensitive(data->add_row_button, FALSE);

    // Показываем окно ПЕРЕД установкой позиции разделителя, чтобы get_width вернул актуальное значение
    gtk_window_present(GTK_WINDOW(window));

    // Устанавливаем позицию разделителя в GtkPaned
    int window_width = gtk_widget_get_width(GTK_WIDGET(window));
    int desired_right_panel_width = 300; // Желаемая ширина правой панели
    // Ширина разделителя может быть разной, 10-12 для wide_handle - нормальное предположение
    int paned_gutter_spacing = 12; 


    int position_for_left_panel = window_width - desired_right_panel_width - paned_gutter_spacing;

    // Минимальная ширина для левой панели (чтобы она не схлопнулась)
    if (position_for_left_panel < 100) {
        position_for_left_panel = 100;
    }
    // Убедимся, что правая панель не будет шире, чем desired_right_panel_width,
    // если окно слишком узкое для position_for_left_panel + desired_right_panel_width
    if (window_width - position_for_left_panel - paned_gutter_spacing < desired_right_panel_width) {
       // Если так, пересчитаем, давая правой панели её минимум
       if (window_width - desired_right_panel_width - paned_gutter_spacing > 100) { // Если для левой остается > 100
            position_for_left_panel = window_width - desired_right_panel_width - paned_gutter_spacing;
       } else if (window_width > desired_right_panel_width + paned_gutter_spacing + 100) { // если окно позволяет
            position_for_left_panel = 100; // отдать левой мин, остальное пойдет правой (не идеально, но предотвратит отрицательную)
       } else { // Окно очень узкое, пусть paned сам решает или одна панель будет очень узкой
            position_for_left_panel = window_width / 2;
       }
    }
    
    // Окончательная проверка, чтобы позиция была в пределах
    if (position_for_left_panel <= 0 ) position_for_left_panel = 50; // Защита от 0 или отрицательного
    if (position_for_left_panel >= window_width - paned_gutter_spacing - 50 ) position_for_left_panel = window_width - paned_gutter_spacing - 50; // Защита


    gtk_paned_set_position(GTK_PANED(main_paned), position_for_left_panel);
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