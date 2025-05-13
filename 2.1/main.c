#include <gtk/gtk.h>
#include <glib.h>
#include <cairo.h>
#include <math.h> 
#include <string.h> 
#include "sqlite_utils.h" 
#include "calculations.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" 
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"   
#pragma GCC diagnostic ignored "-Wformat-truncation"      

static FILE *debug_log = NULL;

// Определение AppData ПЕРЕД forward-объявлениями функций, его использующих
typedef struct {
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
    GtkWidget *level1_button;   
    gboolean db_opened;

    GtkWidget *level1_window;
    GtkWidget *level1_container;
    GtkWidget *level1_tree_view;
    GtkListStore *level1_store;
    GtkWidget *level1_smoothing_view;
    GtkListStore *level1_smoothing_store;
    GtkWidget *level1_drawing_area;
    GtkWidget *level1_alpha_smoothing_view;
    GtkListStore *level1_alpha_smoothing_store;
    GtkWidget *level1_alpha_drawing_area;
    double **smoothing_data;
    double **smoothing_alpha_data;
    double *mu_data;
    int smoothing_row_count;
    int smoothing_alpha_row_count;
    double *alpha_data;
    int alpha_count;
} AppData;


// === Forward объявления для статических функций ===
static void open_sqlite_file_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void update_level1_content(AppData *data); 
static void load_and_display_main_table(AppData *data, const char* table_name_to_load); 
static void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table); 
static void cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
static void draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data); 
static void show_message_dialog(GtkWindow *parent, const char *message);
static void on_level1_window_destroy(GtkWidget *widget, gpointer user_data); 
static void on_level_clicked(GtkWidget *widget, gpointer user_data); 
static void add_new_row(GtkWidget *widget, gpointer user_data); 
static void open_sqlite_file(GtkWidget *widget, gpointer user_data); 
static void activate(GtkApplication *app, gpointer user_data); 
static void update_values_table_view(AppData *data);
static void load_and_display_schema_image_from_values_table(AppData *data);
// ================================================


static void init_debug_log() {
    if (!debug_log) {
        debug_log = fopen("debug.log", "a");
        if (!debug_log) {
            fprintf(stderr, "Не удалось открыть debug.log для записи\n");
        } else {
            fseek(debug_log, 0, SEEK_END);
            if (ftell(debug_log) == 0) {
                fprintf(debug_log, "\xEF\xBB\xBF"); 
            }
            // fprintf(debug_log, "Лог-файл открыт (main.c)\n"); 
            fflush(debug_log);
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

static void show_message_dialog(GtkWindow *parent, const char *message) {
    debug_print("show_message_dialog: %s\n", message); 
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_set_buttons(dialog, (const char *[]){"OK", NULL}); 
    gtk_alert_dialog_show(dialog, parent); 
    g_object_unref(dialog);
}



static void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table) {
    GtkWidget *tree_view;
    GtkListStore **store_ptr;
    TableData* current_table_data; 
    // const char *context_str_debug; 

    if (is_smoothing_alpha_table) {
        tree_view = data->level1_alpha_smoothing_view;
        store_ptr = &data->level1_alpha_smoothing_store;
        current_table_data = data->table_data_main; 
        // context_str_debug = "L1_SM_ALPHA";
    } else if (is_smoothing_mu_table) {
        tree_view = data->level1_smoothing_view;
        store_ptr = &data->level1_smoothing_store;
        current_table_data = data->table_data_main;
        // context_str_debug = "L1_SM_MU";
    } else if (is_level1_context) {
        tree_view = data->level1_tree_view;
        store_ptr = &data->level1_store;
        current_table_data = data->table_data_main;
        // context_str_debug = "L1_MAIN";
    } else { 
        tree_view = data->tree_view_main; 
        store_ptr = &data->store_main;    
        current_table_data = data->table_data_main;
        // context_str_debug = "MAIN_TABLE";
    }
    // debug_print("update_tree_view_columns (%s): Начало. Старый store: %p\n", context_str_debug, (void*)*store_ptr);

    GList *columns_list = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view)); 
    for (GList *l = columns_list; l; l = l->next) {
        GtkTreeViewColumn *col_to_remove = GTK_TREE_VIEW_COLUMN(l->data);
        gtk_tree_view_remove_column(GTK_TREE_VIEW(tree_view), col_to_remove);
    }
    g_list_free_full(columns_list, g_object_unref); 
    // debug_print("update_tree_view_columns (%s): Старые столбцы удалены и unref-нуты\n", context_str_debug);


    if (*store_ptr) {
        // debug_print("update_tree_view_columns (%s): Unref старого store %p (ref_count до: %u)\n", 
        //             context_str_debug, (void*)*store_ptr, G_OBJECT(*store_ptr)->ref_count);
        g_object_unref(*store_ptr); 
        *store_ptr = NULL;
    }

    int model_column_count;
    if (is_smoothing_mu_table || is_smoothing_alpha_table) {
        model_column_count = 6; 
    } else if (is_level1_context) { 
        model_column_count = 1 + (current_table_data ? current_table_data->column_count : 0) + 2; 
    } else { 
        model_column_count = 1 + (current_table_data ? current_table_data->column_count : 0); 
    }
     if (current_table_data == NULL && model_column_count > 1 && (is_level1_context || (!is_smoothing_mu_table && !is_smoothing_alpha_table)) ) { 
        model_column_count = 1; 
    }

    GType *types = g_new(GType, model_column_count);
    types[0] = G_TYPE_INT; 
    for (int i = 1; i < model_column_count; i++) {
        types[i] = G_TYPE_DOUBLE;
    }
    *store_ptr = gtk_list_store_newv(model_column_count, types); 
    g_free(types);
    // debug_print("update_tree_view_columns (%s): Новый GtkListStore %p создан с %d столбцами (ref_count: %u)\n", 
    //             context_str_debug, (void*)*store_ptr, model_column_count, G_OBJECT(*store_ptr)->ref_count);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    col = gtk_tree_view_column_new_with_attributes(
        (is_smoothing_mu_table || is_smoothing_alpha_table) ? "M" : "Эпоха",
        renderer, "text", 0, NULL);
    gtk_tree_view_column_set_fixed_width(col, 60);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    if (is_smoothing_mu_table || is_smoothing_alpha_table) {
        const char *titles[] = {"A=0.1", "A=0.4", "A=0.7", "A=0.9", is_smoothing_alpha_table ? "α (реальн.)" : "μ (реальн.)"};
        for (int i = 0; i < 5; i++) { 
            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "xalign", 0.5, NULL);
            
            col = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(col, titles[i]);
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(i + 1), NULL);

            if (i == 4) { 
                g_object_set_data(G_OBJECT(col), is_smoothing_alpha_table ? "is-alpha" : "is-mu", GINT_TO_POINTER(1));
                gtk_tree_view_column_set_fixed_width(col, 120);
            } else {
                gtk_tree_view_column_set_fixed_width(col, 80);
            }
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
        }
    } else if (is_level1_context) { 
        if (current_table_data && current_table_data->column_names) { 
            for (int i = 0; i < current_table_data->column_count; i++) {
                renderer = gtk_cell_renderer_text_new();
                g_object_set(renderer, "xalign", 0.5, NULL);
                col = gtk_tree_view_column_new();
                const char* col_title = (current_table_data->column_names[i + 1]) ? current_table_data->column_names[i + 1] : "ERR_COL_NAME";
                gtk_tree_view_column_set_title(col, col_title);

                gtk_tree_view_column_pack_start(col, renderer, TRUE);
                gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(i + 1), NULL);
                gtk_tree_view_column_set_fixed_width(col, 80);
                gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(col, "μ");
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            int mu_model_idx = current_table_data->column_count + 1;
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(mu_model_idx), NULL);
            g_object_set_data(G_OBJECT(col), "is-mu", GINT_TO_POINTER(1)); 
            gtk_tree_view_column_set_fixed_width(col, 80);
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "xalign", 0.5, NULL);
            col = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(col, "α");
            gtk_tree_view_column_pack_start(col, renderer, TRUE);
            int alpha_model_idx = current_table_data->column_count + 2;
            gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(alpha_model_idx), NULL);
            g_object_set_data(G_OBJECT(col), "is-alpha", GINT_TO_POINTER(1));
            gtk_tree_view_column_set_fixed_width(col, 100);
            gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
        } 
    } else { 
        if (current_table_data && current_table_data->column_names) { 
            for (int i = 0; i < current_table_data->column_count; i++) {
                renderer = gtk_cell_renderer_text_new();
                g_object_set(renderer, "xalign", 0.5, NULL);
                col = gtk_tree_view_column_new();
                const char* col_title = (current_table_data->column_names[i + 1]) ? current_table_data->column_names[i + 1] : "ERR_COL_NAME";
                gtk_tree_view_column_set_title(col, col_title);
                gtk_tree_view_column_pack_start(col, renderer, TRUE);
                gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(i + 1), NULL);
                gtk_tree_view_column_set_fixed_width(col, 80);
                gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);
            }
        }
    }
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(*store_ptr));
    // debug_print("update_tree_view_columns (%s): Завершено. Модель %p установлена.\n", context_str_debug, (void*)*store_ptr);
}

static void update_values_table_view(AppData *data) {
    if (!data || !data->tree_view_values) { // Упрощенная проверка
        debug_print("update_values_table_view: Пропуск: data или tree_view_values is NULL.\n");
        return;
    }
     if (!data->table_data_values) {
        debug_print("update_values_table_view: Пропуск: table_data_values is NULL.\n");
        gtk_widget_set_visible(data->tree_view_values, FALSE);
        return;
    }

    debug_print("update_values_table_view: Обновление таблицы 'Значения'. Строк в table_data_values: %d, Колонок данных: %d\n", 
                data->table_data_values->row_count, data->table_data_values->column_count);

    GtkTreeView *treeview = GTK_TREE_VIEW(data->tree_view_values);
    
    // Устанавливаем AppData для использования в cell_data_func (если отладка там еще нужна)
    g_object_set_data(G_OBJECT(treeview), "app_data_ptr_for_debug", data); 

    // 1. Очистка старых колонок GtkTreeView
    GList *columns = gtk_tree_view_get_columns(treeview);
    for (GList *l = columns; l; l = l->next) {
        gtk_tree_view_remove_column(treeview, GTK_TREE_VIEW_COLUMN(l->data));
    }
    g_list_free_full(columns, g_object_unref);

    // 2. Очистка и создание GtkListStore (модели)
    if (data->store_values) {
        g_object_unref(data->store_values);
        data->store_values = NULL;
    }

    if (data->table_data_values->column_names == NULL) { 
        debug_print("update_values_table_view: column_names для таблицы 'Значения' не загружены. Невозможно создать колонки.\n");
        gtk_tree_view_set_model(treeview, NULL); 
        gtk_widget_set_visible(data->tree_view_values, FALSE);
        return;
    }
    
    // Количество колонок в модели: 1 (для скрытого ID/ROWID) + количество ВИДИМЫХ колонок данных.
    // Сначала посчитаем, сколько колонок данных мы действительно будем отображать (пропуская "Схема").
    int num_displayable_data_columns = 0;
    for (int i = 0; i < data->table_data_values->column_count; i++) {
        if (data->table_data_values->column_names[i] &&
            g_strcmp0(data->table_data_values->column_names[i], data->schema_blob_column_name) != 0) {
            num_displayable_data_columns++;
        }
    }

    if (num_displayable_data_columns == 0 && data->table_data_values->column_count > 0) {
        debug_print("update_values_table_view: Нет видимых колонок данных для таблицы 'Значения' (возможно, только BLOB?).\n");
    }

    int num_model_cols = 1 + num_displayable_data_columns; // 1 для скрытого ID/ROWID

    GType *types = g_new(GType, num_model_cols);
    types[0] = G_TYPE_INT; // Скрытый ID/ROWID
    for (int i = 1; i < num_model_cols; i++) { 
        types[i] = G_TYPE_DOUBLE; // Видимые колонки (A, E, Количество)
    }
    data->store_values = gtk_list_store_newv(num_model_cols, types);
    g_free(types);
    debug_print("update_values_table_view: Новый store_values %p создан с %d колонками (1 скр.+ %d вид.).\n", 
                (void*)data->store_values, num_model_cols, num_displayable_data_columns);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    // --- Колонки GtkTreeView НЕ СОЗДАЮТСЯ для 0-го столбца модели (где ROWID) ---

    // Создаем колонки GtkTreeView только для видимых данных (A, E, Количество)
    // model_idx_in_store будет указывать на столбец в GtkListStore (начиная с 1)
    int model_idx_in_store = 1; 
    for (int i = 0; i < data->table_data_values->column_count; i++) { 
        // i - индекс в table_data_values->column_names и table_data_values->table (0="A", 1="E", 2="Схема", 3="Количество")
        const char* title = data->table_data_values->column_names[i]; 
        
        if (title == NULL) continue; 
        if (g_strcmp0(title, data->schema_blob_column_name) == 0) { // Сравниваем с "Схема"
            continue; // Пропускаем BLOB колонку "Схема"
        }

        renderer = gtk_cell_renderer_text_new();
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(col, title);
        gtk_tree_view_column_pack_start(col, renderer, TRUE);
        
        // Связываем эту отображаемую колонку GtkTreeView с соответствующим model_idx_in_store в GtkListStore
        gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, GINT_TO_POINTER(model_idx_in_store), NULL); 
        
        if (g_strcmp0(title, "Количество") == 0) {
            g_object_set_data(G_OBJECT(col), "is-integer-like", GINT_TO_POINTER(1)); 
            g_object_set(renderer, "xalign", 0.5, NULL); 
        } else { 
            g_object_set(renderer, "xalign", 0.5, NULL); 
        }
        
        gtk_tree_view_column_set_fixed_width(col, 100);
        gtk_tree_view_append_column(treeview, col);
        debug_print("update_values_table_view: Добавлена колонка '%s' в TreeView, привязана к модели[%d]\n", title, model_idx_in_store);
        model_idx_in_store++; // Переходим к следующему столбцу в МОДЕЛИ ListStore для следующей видимой колонки
    }
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(data->store_values));

    // 3. Заполнение GtkListStore
    if(data->table_data_values && data->table_data_values->row_count > 0 && data->table_data_values->table) {
        for(int i_row = 0; i_row < data->table_data_values->row_count; ++i_row) {
            GtkTreeIter iter; 
            gtk_list_store_append(data->store_values, &iter);

            // Записываем ID/ROWID в 0-й (скрытый) столбец модели
            gtk_list_store_set(data->store_values, &iter, 0, 
                               (data->table_data_values->epochs) ? data->table_data_values->epochs[i_row] : (i_row + 1), 
                               -1); 
            
            int current_store_col_idx = 1; // Индекс для записи данных в store_values (начиная с 1)
            // j_data - индекс колонки в исходных данных table_data_values->table и column_names
            for(int j_data = 0; j_data < data->table_data_values->column_count; ++j_data) { 
                 const char *col_name_from_data = data->table_data_values->column_names[j_data];

                 if (col_name_from_data == NULL) continue; 
                 if (g_strcmp0(col_name_from_data, data->schema_blob_column_name) == 0) {
                    continue; // Пропускаем данные для колонки BLOB при заполнении store_values
                 }
                
                double value_to_set = NAN; 
                if (data->table_data_values->table[i_row] != NULL) { 
                     value_to_set = data->table_data_values->table[i_row][j_data]; 
                }

                // Лог для отладки (можно закомментировать, если все работает)
                // if (strcmp(col_name_from_data, "A") == 0 || strcmp(col_name_from_data, "Количество") == 0 || strcmp(col_name_from_data, "E") == 0) {
                //      debug_print("update_values_table_view: Установка в store для '%s' (исх.строка %d, модель кол. %d) значение: %f\n",
                //                 col_name_from_data, i_row, current_store_col_idx, value_to_set);
                // }
                gtk_list_store_set(data->store_values, &iter, current_store_col_idx, value_to_set, -1);
                current_store_col_idx++;
            }
        }
    }
    gtk_widget_set_visible(data->tree_view_values, (data->table_data_values && data->table_data_values->row_count > 0));
    debug_print("update_values_table_view: Завершено. Строк в store_values: %d.\n", 
                data->store_values ? gtk_tree_model_iter_n_children(GTK_TREE_MODEL(data->store_values), NULL) : 0);
}


static void cell_data_func(GtkTreeViewColumn *tree_column,
                           GtkCellRenderer *cell,
                           GtkTreeModel *tree_model,
                           GtkTreeIter *iter,
                           gpointer user_data_callback) { 
    gint model_column_index = GPOINTER_TO_INT(user_data_callback); 
    
    GtkWidget* tree_view_as_widget = gtk_tree_view_column_get_tree_view(tree_column); // Получаем как GtkWidget*
    GtkTreeView* current_tree_view_widget = NULL;
    if (GTK_IS_TREE_VIEW(tree_view_as_widget)) { // Проверяем тип перед приведением
        current_tree_view_widget = GTK_TREE_VIEW(tree_view_as_widget);
    }

    AppData* app_data_global = NULL;
    if(current_tree_view_widget) { // Используем уже проверенный и приведенный current_tree_view_widget
        app_data_global = g_object_get_data(G_OBJECT(current_tree_view_widget), "app_data_ptr_for_debug");
    }
    const char* col_title_debug = gtk_tree_view_column_get_title(tree_column);

    gdouble value;
    gtk_tree_model_get(tree_model, iter, model_column_index, &value, -1);
    gchar *text;

    if (g_object_get_data(G_OBJECT(tree_column), "is-alpha")) {
        if (isnan(value)) text = g_strdup(""); 
        else text = g_strdup_printf("%.2E", value);
    } else if (g_object_get_data(G_OBJECT(tree_column), "is-mu")) {
        if (isnan(value)) { text = g_strdup(""); }
        else if (model_column_index == 5 && value == 0.0) { text = g_strdup("");  }
        else { text = g_strdup_printf("%.4f", value); }
    } else if (g_object_get_data(G_OBJECT(tree_column), "is-integer-like")) { 
        if (isnan(value)) text = g_strdup("");
        else text = g_strdup_printf("%d", (int)round(value)); 
    } else { 
        if (isnan(value)) text = g_strdup("");
        else text = g_strdup_printf("%.4f", value); 
        
        if (app_data_global && current_tree_view_widget == GTK_TREE_VIEW(app_data_global->tree_view_values) && 
            col_title_debug && strcmp(col_title_debug, "A") == 0) {
             debug_print("cell_data_func for 'A' (values_table): model_col_idx=%d, value_from_model=%f, formatted_text='%s'\n",
                         model_column_index, value, text);
        }
         // Для отладки "Количество", если оно не помечено как "is-integer-like"
        if (app_data_global && current_tree_view_widget == GTK_TREE_VIEW(app_data_global->tree_view_values) && 
            col_title_debug && strcmp(col_title_debug, "Количество") == 0 && 
            !g_object_get_data(G_OBJECT(tree_column), "is-integer-like")) { // Если не обработано выше
             debug_print("cell_data_func for 'Количество' (values_table, default format): model_col_idx=%d, value_from_model=%f, formatted_text='%s'\n",
                         model_column_index, value, text);
        }
    }
    g_object_set(cell, "text", text, NULL);
    g_free(text);
}


static void load_and_display_schema_image_from_values_table(AppData *data) {
    if (!data->db_opened || !*data->db || !data->values_table_name || !data->schema_blob_column_name) {
        debug_print("load_and_display_schema_image: Недостаточно информации для загрузки изображения (БД, имя таблицы/колонки).\n");
        if(data->picture_schema) gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
        if(data->picture_schema) gtk_widget_set_visible(data->picture_schema, FALSE);
        return;
    }
    
    int rowid_for_image = 1; // Берем BLOB из первой строки (ROWID=1) таблицы "Значения"
    debug_print("load_image: Попытка загрузки BLOB '%s' из таблицы '%s', ROWID=%d\n",
        data->schema_blob_column_name, data->values_table_name, rowid_for_image);

    GBytes *image_bytes = load_blob_from_table_by_rowid(*data->db, 
                                               data->values_table_name, 
                                               data->schema_blob_column_name,
                                               rowid_for_image); 

    if (data->pixbuf_schema) { 
        g_object_unref(data->pixbuf_schema);
        data->pixbuf_schema = NULL;
    }

    if (image_bytes && data->picture_schema) { 
        GError *error_load = NULL; 
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new (); 

        if (!gdk_pixbuf_loader_write (loader, 
                                    g_bytes_get_data (image_bytes, NULL), 
                                    g_bytes_get_size (image_bytes), 
                                    &error_load)) {
            debug_print("load_image: Ошибка записи данных в GdkPixbufLoader: %s\n", error_load ? error_load->message : "неизвестная ошибка");
            if(error_load) g_error_free(error_load);
            g_object_unref (loader);
            g_bytes_unref (image_bytes);
            gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
            gtk_widget_set_visible(data->picture_schema, FALSE);
            return;
        }

        if (!gdk_pixbuf_loader_close (loader, &error_load)) {
            debug_print("load_image: Ошибка закрытия GdkPixbufLoader: %s\n", error_load ? error_load->message : "неизвестная ошибка");
            if(error_load) g_error_free(error_load);
            // Попробуем получить pixbuf, даже если close не удался
            data->pixbuf_schema = gdk_pixbuf_loader_get_pixbuf (loader); 
            if (data->pixbuf_schema) g_object_ref(data->pixbuf_schema); 
            
            if (data->pixbuf_schema) { 
                gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), data->pixbuf_schema);
                gtk_widget_set_visible(data->picture_schema, TRUE);
             } else {
                gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
                gtk_widget_set_visible(data->picture_schema, FALSE);
             }
            g_object_unref (loader);
            g_bytes_unref (image_bytes);
            return;
        }

        data->pixbuf_schema = gdk_pixbuf_loader_get_pixbuf (loader);
        if (data->pixbuf_schema) {
            g_object_ref(data->pixbuf_schema); 
            debug_print("load_image: Pixbuf успешно создан (через GdkPixbufLoader).\n");
            gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), data->pixbuf_schema);
            gtk_widget_set_visible(data->picture_schema, TRUE);
        } else {
            debug_print("load_image: gdk_pixbuf_loader_get_pixbuf вернул NULL после успешного close.\n");
            gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
            gtk_widget_set_visible(data->picture_schema, FALSE);
        }
        
        g_object_unref (loader); 
        g_bytes_unref (image_bytes); 
    } else {
        if (data->picture_schema) gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
        if (data->picture_schema) gtk_widget_set_visible(data->picture_schema, FALSE);
        if (!image_bytes) debug_print("load_image: BLOB не загружен из БД (image_bytes is NULL).\n");
    }
}



static void draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    gboolean is_alpha_graph = (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_drawing_area));
    debug_print("draw_graph: НАЧАЛО отрисовки графика (%s), width=%d, height=%d\n", is_alpha_graph ? "α" : "μ", width, height);


    double **graph_smoothing_data = is_alpha_graph ? data->smoothing_alpha_data : data->smoothing_data;
    int graph_smoothing_row_count = is_alpha_graph ? data->smoothing_alpha_row_count : data->smoothing_row_count;
    
    double *real_plot_data = NULL;
    int real_plot_data_count = 0;

    if (is_alpha_graph) {
        if (data->alpha_data && data->alpha_count > 1) {
            real_plot_data = data->alpha_data + 1; 
            real_plot_data_count = data->alpha_count - 1;
        }
    } else { // mu graph
        // Для мю-графика, реальные данные - это data->mu_data, и их количество соответствует
        // data->table_data_main->row_count, так как расчеты μ/α Уровня 1 всегда на data_main
        if (data->mu_data && data->table_data_main && data->table_data_main->row_count > 0) {
            real_plot_data = data->mu_data;
            real_plot_data_count = data->table_data_main->row_count; 
        }
    }

    gboolean has_smoothing_data = graph_smoothing_data && graph_smoothing_row_count > 0;
    gboolean has_real_data = real_plot_data && real_plot_data_count > 0;


    if (!has_smoothing_data && !has_real_data) { 
        debug_print("draw_graph: Нет данных для графика (%s) - has_smoothing_data=%d, has_real_data=%d\n", 
            is_alpha_graph ? "α" : "μ", has_smoothing_data, has_real_data);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
        cairo_paint(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); 
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t extents;
        const char* no_data_msg = "Нет данных для графика";
        cairo_text_extents(cr, no_data_msg, &extents);
        cairo_move_to(cr, (width - extents.width) / 2.0, (height - extents.height) / 2.0 + extents.height); 
        cairo_show_text(cr, no_data_msg);
        return;
    }

    double margin_left = 70.0; 
    double margin_right = 20.0;
    double margin_top = 40.0; 
    double margin_bottom = 90.0; 
    double plot_width = width - margin_left - margin_right;
    double plot_height = height - margin_top - margin_bottom;

    if (plot_width <= 1 || plot_height <= 1) { 
        debug_print("draw_graph: Недостаточно места для отрисовки графика (plot_width=%.1f, plot_height=%.1f).\n", plot_width, plot_height);
        return;
    }

    double y_min_val = G_MAXDOUBLE, y_max_val = -G_MAXDOUBLE; 
    gboolean data_found_for_yrange = FALSE;


    if(has_smoothing_data){
        for (int i = 0; i < graph_smoothing_row_count; i++) {
            for (int j_idx = 0; j_idx < 4; j_idx++) { 
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
    
    if (!data_found_for_yrange) { 
        debug_print("draw_graph: Данные для диапазона Y не найдены (все NaN?). Использую дефолтный диапазон.\n");
        y_min_val = -1.0; y_max_val = 1.0;
    }

    double y_range_val_calc = y_max_val - y_min_val;
    if (fabs(y_range_val_calc) < 1e-9) { 
        double abs_val = fabs(y_min_val);
        if (abs_val < 1e-9) abs_val = 1.0; 
        y_min_val -= 0.5 * abs_val; 
        y_max_val += 0.5 * abs_val;
         if (fabs(y_max_val - y_min_val) < 1e-9) { 
            y_min_val = -1.0;
            y_max_val = 1.0;
        }
    } else {
        y_min_val -= y_range_val_calc * 0.1; 
        y_max_val += y_range_val_calc * 0.1; 
    }
    y_range_val_calc = y_max_val - y_min_val; 
    if (fabs(y_range_val_calc) < 1e-9) y_range_val_calc = 1.0; 

    int x_num_points_axis = 0; 
    if(has_smoothing_data) x_num_points_axis = graph_smoothing_row_count;
    if(has_real_data && real_plot_data_count > x_num_points_axis) x_num_points_axis = real_plot_data_count;
    if (x_num_points_axis == 0) x_num_points_axis = 1; 


    double x_max_ticks = x_num_points_axis > 1 ? (x_num_points_axis -1) : 1.0;
    double x_scale = (x_max_ticks > 1e-9) ? (plot_width / x_max_ticks) : plot_width;  
    if (x_num_points_axis == 1 && fabs(x_max_ticks) < 1e-9) x_scale = plot_width; 


    double y_scale = plot_height / y_range_val_calc;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); 
    cairo_set_line_width(cr, 1.0);

    cairo_move_to(cr, margin_left, height - margin_bottom);
    cairo_line_to(cr, margin_left + plot_width, height - margin_bottom);
    cairo_stroke(cr);

    cairo_move_to(cr, margin_left, margin_top);
    cairo_line_to(cr, margin_left, margin_top + plot_height);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    for (int i = 0; i < x_num_points_axis; i++) { 
        int tick_frequency = 1;
        if (x_num_points_axis > 15) tick_frequency = (int)ceil((double)x_num_points_axis / 10.0);
        if (x_num_points_axis <= 15 || i % tick_frequency == 0 || i == x_num_points_axis -1 || i == 0 ) {
            double x_pos = margin_left + i * x_scale;
            if (x_num_points_axis == 1) x_pos = margin_left + plot_width / 2; 
            char label[16];
            snprintf(label, sizeof(label), "%d", i); 
            cairo_text_extents_t extents;
            cairo_text_extents(cr, label, &extents);
            cairo_move_to(cr, x_pos - extents.width / 2.0, height - margin_bottom + 15);
            cairo_show_text(cr, label);
        }
    }
    const char *x_axis_label_str = "M (период)"; 
    cairo_text_extents_t x_label_extents_calc; 
    cairo_text_extents(cr, x_axis_label_str, &x_label_extents_calc);
    cairo_move_to(cr, margin_left + plot_width / 2.0 - x_label_extents_calc.width / 2.0, height - margin_bottom + 35);
    cairo_show_text(cr, x_axis_label_str);

    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; i++) {
        double y_value = y_min_val + (y_range_val_calc * i / num_y_ticks);
        double y_pos = height - margin_bottom - (y_value - y_min_val) * y_scale;
        char label[32];
        snprintf(label, sizeof(label), is_alpha_graph ? "%.1E" : "%.2f", y_value); 
        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, margin_left - extents.width - 5, y_pos + extents.height / 2.0); 
        cairo_show_text(cr, label);

        cairo_move_to(cr, margin_left - 3, y_pos);
        cairo_line_to(cr, margin_left, y_pos);
        cairo_stroke(cr);
    }
    const char *y_axis_label_str = is_alpha_graph ? "Значение α" : "Значение μ"; 
    cairo_text_extents_t y_label_extents_val; 
    cairo_text_extents(cr, y_axis_label_str, &y_label_extents_val);
    cairo_save(cr);
    cairo_move_to(cr, margin_left - 55, margin_top + plot_height / 2.0 + y_label_extents_val.width / 2.0); 
    cairo_rotate(cr, -M_PI / 2.0);
    cairo_show_text(cr, y_axis_label_str);
    cairo_restore(cr);

    double colors[5][3] = {
        {1.0, 0.0, 0.0}, 
        {0.0, 0.7, 0.0}, 
        {0.0, 0.0, 1.0}, 
        {0.8, 0.6, 0.0}, 
        {0.3, 0.3, 0.3}  
    };
    const char *legend_labels[] = {"A=0.1", "A=0.4", "A=0.7", "A=0.9", is_alpha_graph ? "Реальные α" : "Реальные μ"};

    if(has_smoothing_data){
        for (int j_idx_color = 0; j_idx_color < 4; j_idx_color++) { 
            cairo_set_source_rgb(cr, colors[j_idx_color][0], colors[j_idx_color][1], colors[j_idx_color][2]);
            cairo_set_line_width(cr, 1.5); 

            gboolean first_point_in_series = TRUE;
            for (int i = 0; i < graph_smoothing_row_count; i++) {
                double val_y = graph_smoothing_data[i][j_idx_color];
                if (isnan(val_y) || isinf(val_y)) {
                    first_point_in_series = TRUE; 
                    continue;
                }
                double x_pos = margin_left + i * x_scale;
                if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2; 
                double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;
                
                if (y_pos < margin_top) y_pos = margin_top;
                if (y_pos > height - margin_bottom) y_pos = height - margin_bottom;

                if (first_point_in_series) {
                    cairo_move_to(cr, x_pos, y_pos);
                    first_point_in_series = FALSE;
                } else {
                    cairo_line_to(cr, x_pos, y_pos);
                }
            }
            if (!first_point_in_series) cairo_stroke(cr); 

             for (int i = 0; i < graph_smoothing_row_count; i++) {
                double val_y = graph_smoothing_data[i][j_idx_color];
                if (isnan(val_y) || isinf(val_y)) continue;
                double x_pos = margin_left + i * x_scale;
                if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2;
                double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;
                if (y_pos < margin_top -3 || y_pos > height - margin_bottom + 3 ) continue;
                cairo_arc(cr, x_pos, y_pos, 2.2, 0, 2 * M_PI); 
                cairo_fill(cr);
            }
        }
    }

    if(has_real_data){
        int real_data_color_idx = 4;
        cairo_set_source_rgb(cr, colors[real_data_color_idx][0], colors[real_data_color_idx][1], colors[real_data_color_idx][2]);
        cairo_set_line_width(cr, 2.0);
        gboolean first_point_in_series = TRUE;
        for(int i = 0; i < real_plot_data_count; ++i){
            double val_y = real_plot_data[i];
            if (isnan(val_y) || isinf(val_y)) {
                first_point_in_series = TRUE;
                continue;
            }
            double x_pos = margin_left + i * x_scale;
             if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2;
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;

            if (y_pos < margin_top) y_pos = margin_top;
            if (y_pos > height - margin_bottom) y_pos = height - margin_bottom;

            if (first_point_in_series) {
                cairo_move_to(cr, x_pos, y_pos);
                first_point_in_series = FALSE;
            } else {
                cairo_line_to(cr, x_pos, y_pos);
            }
        }
        if (!first_point_in_series) cairo_stroke(cr);

        for(int i = 0; i < real_plot_data_count; ++i){
            double val_y = real_plot_data[i];
             if (isnan(val_y) || isinf(val_y)) continue;
            double x_pos = margin_left + i * x_scale;
             if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2;
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;
            if (y_pos < margin_top -3 || y_pos > height - margin_bottom + 3 ) continue;
            cairo_arc(cr, x_pos, y_pos, 2.8, 0, 2 * M_PI); 
            cairo_fill(cr);
        }
    }

    double legend_item_width = 90;
    double legend_x_current = margin_left; 
    double legend_y_current = height - margin_bottom + 50; 
    
    cairo_set_font_size(cr, 10);
    
    for (int j_idx_leg = 0; j_idx_leg < 5; j_idx_leg++) { 
        if (j_idx_leg < 4 && !has_smoothing_data) continue; 
        if (j_idx_leg == 4 && !has_real_data) continue;   

        if (legend_x_current + legend_item_width > width - margin_right) {
             legend_y_current += 15; 
             legend_x_current = margin_left;
             if (legend_x_current + legend_item_width > width - margin_right) break; 
        }

        cairo_set_source_rgb(cr, colors[j_idx_leg][0], colors[j_idx_leg][1], colors[j_idx_leg][2]);
        cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0, 0, 0); 
        cairo_text_extents_t text_ext_legend; 
        cairo_text_extents(cr, legend_labels[j_idx_leg], &text_ext_legend);
        cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1); 
        cairo_show_text(cr, legend_labels[j_idx_leg]);
        legend_x_current += legend_item_width;
    }

    const char *title_str_graph = is_alpha_graph ? "Прогнозные функции Zпр(α)" : "Прогнозные функции Zпр(μ)"; 
    cairo_set_font_size(cr, 14);
    cairo_text_extents_t title_extents_val_graph; 
    cairo_text_extents(cr, title_str_graph, &title_extents_val_graph);
    cairo_move_to(cr, margin_left + plot_width / 2.0 - title_extents_val_graph.width / 2.0, margin_top - 15);
    cairo_show_text(cr, title_str_graph);
}


static void update_level1_content(AppData *data) {
    debug_print("update_level1_content: НАЧАЛО\n");

    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; i++) {if(data->smoothing_data[i]) g_free(data->smoothing_data[i]);}
        g_free(data->smoothing_data); data->smoothing_data = NULL;
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; i++) {if(data->smoothing_alpha_data[i]) g_free(data->smoothing_alpha_data[i]);}
        g_free(data->smoothing_alpha_data); data->smoothing_alpha_data = NULL;
    }
    if (data->mu_data) { g_free(data->mu_data); data->mu_data = NULL; }
    if (data->alpha_data) { g_free(data->alpha_data); data->alpha_data = NULL; }
    data->smoothing_row_count = 0;
    data->smoothing_alpha_row_count = 0;
    data->alpha_count = 0;

    // Расчеты для Уровня 1 производятся на основе table_data_main
    if(data->table_data_main == NULL || data->table_data_main->row_count == 0) { 
        debug_print("update_level1_content: НЕТ ДАННЫХ в table_data_main (row_count=%d, table_data_main=%p). Вычисления не производятся.\n", 
                    data->table_data_main ? data->table_data_main->row_count : -1, (void*)data->table_data_main);
        if (data->level1_store) gtk_list_store_clear(data->level1_store);
        if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
        if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
        if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area); 
        if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
        return;
    }
    debug_print("update_level1_content: TableDataMain: rows=%d, cols=%d\n", data->table_data_main->row_count, data->table_data_main->column_count);

    data->mu_data = compute_mu(data->table_data_main->table, data->table_data_main->row_count, data->table_data_main->column_count);
    if (data->mu_data && data->table_data_main->row_count > 0) {
        debug_print("update_level1_content: mu_data вычислено (%d элементов). mu[0]=%f\n", data->table_data_main->row_count, data->mu_data[0]);
    } else {
        debug_print("update_level1_content: mu_data is NULL или row_count=0 после compute_mu.\n");
    }

    if (data->mu_data) { 
        data->alpha_data = compute_alpha(data->table_data_main->table, data->table_data_main->row_count,
                                       data->table_data_main->column_count, data->mu_data);
        if (data->alpha_data && data->table_data_main->row_count > 0) {
            data->alpha_count = data->table_data_main->row_count;
            debug_print("update_level1_content: alpha_data вычислено (%d элементов). alpha[0]=%E, alpha[1]=%E (если есть)\n", 
                        data->alpha_count, data->alpha_data[0], data->alpha_count > 1 ? data->alpha_data[1] : NAN);
        } else {
            debug_print("update_level1_content: alpha_data is NULL или row_count=0 после compute_alpha.\n");
        }
    } else {
         debug_print("update_level1_content: mu_data отсутствуют, alpha_data не вычисляется.\n");
    }

    if (data->level1_store) { // Заполняем таблицу Level 1 с μ и α
        gtk_list_store_clear(data->level1_store);
        for (int i = 0; i < data->table_data_main->row_count; i++) {
            GtkTreeIter iter;
            gtk_list_store_append(data->level1_store, &iter);
            gtk_list_store_set(data->level1_store, &iter, 0, data->table_data_main->epochs[i], -1);
            for (int j = 0; j < data->table_data_main->column_count; j++) {
                gtk_list_store_set(data->level1_store, &iter, j + 1, data->table_data_main->table[i][j], -1);
            }
            gtk_list_store_set(data->level1_store, &iter,
                              data->table_data_main->column_count + 1, (data->mu_data && i < data->table_data_main->row_count) ? data->mu_data[i] : NAN,
                              data->table_data_main->column_count + 2, (data->alpha_data && i < data->alpha_count) ? data->alpha_data[i] : NAN,
                              -1);
        }
    }

    double coefficients[] = {0.1, 0.4, 0.7, 0.9};
    int coeff_count = sizeof(coefficients) / sizeof(coefficients[0]);

    if (data->level1_smoothing_store && data->mu_data && data->table_data_main->row_count > 0) {
        gtk_list_store_clear(data->level1_smoothing_store);
        data->smoothing_data = compute_smoothing(data->mu_data, data->table_data_main->row_count,
                                               &data->smoothing_row_count, coefficients, coeff_count);
        if (data->smoothing_data && data->smoothing_row_count > 0) {
            debug_print("update_level1_content: smoothing_data (для mu) вычислено. Строк: %d. sm[0][0]=%f\n", 
                        data->smoothing_row_count, data->smoothing_data[0][0]);
            for (int i = 0; i < data->smoothing_row_count; i++) {
                GtkTreeIter iter;
                gtk_list_store_append(data->level1_smoothing_store, &iter);
                gtk_list_store_set(data->level1_smoothing_store, &iter,
                                  0, i, 
                                  1, data->smoothing_data[i][0], 
                                  2, data->smoothing_data[i][1], 
                                  3, data->smoothing_data[i][2], 
                                  4, data->smoothing_data[i][3], 
                                  5, (i < data->table_data_main->row_count) ? data->mu_data[i] : NAN, 
                                  -1);
            }
        } else {
             debug_print("update_level1_content: smoothing_data (для mu) is NULL или smoothing_row_count=0.\n");
        }
    } else {
        debug_print("update_level1_content: Пропуск сглаживания mu (условия не выполнены: store=%p, mu_data=%p, rows_main=%d).\n",
            (void*)data->level1_smoothing_store, (void*)data->mu_data, data->table_data_main ? data->table_data_main->row_count : -1);
    }

    if (data->level1_alpha_smoothing_store && data->alpha_data && data->alpha_count > 1) { 
        gtk_list_store_clear(data->level1_alpha_smoothing_store);
        data->smoothing_alpha_data = compute_smoothing(data->alpha_data + 1, data->alpha_count - 1, 
                                                     &data->smoothing_alpha_row_count, coefficients, coeff_count);
        if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
             debug_print("update_level1_content: smoothing_alpha_data вычислено. Строк: %d. sm_a[0][0]=%E\n", 
                        data->smoothing_alpha_row_count, data->smoothing_alpha_data[0][0]);
            for (int i = 0; i < data->smoothing_alpha_row_count; i++) { 
                GtkTreeIter iter;
                gtk_list_store_append(data->level1_alpha_smoothing_store, &iter);
                gtk_list_store_set(data->level1_alpha_smoothing_store, &iter,
                                  0, i, 
                                  1, data->smoothing_alpha_data[i][0], 
                                  2, data->smoothing_alpha_data[i][1], 
                                  3, data->smoothing_alpha_data[i][2], 
                                  4, data->smoothing_alpha_data[i][3], 
                                  5, (i < (data->alpha_count - 1)) ? data->alpha_data[i + 1] : NAN, 
                                  -1);
            }
        } else {
            debug_print("update_level1_content: smoothing_alpha_data is NULL или smoothing_alpha_row_count=0.\n");
        }
    } else {
         debug_print("update_level1_content: Пропуск сглаживания alpha (условия не выполнены: store=%p, alpha_data=%p, alpha_cnt=%d).\n",
            (void*)data->level1_alpha_smoothing_store, (void*)data->alpha_data, data->alpha_count);
    }
    
    if (data->mu_data && data->alpha_data && data->smoothing_data && data->smoothing_alpha_data) { 
        if (!write_results_to_file("results.txt", data->mu_data, data->table_data_main->row_count, // Используем table_data_main
                                 data->alpha_data, data->alpha_count,
                                 data->smoothing_data, data->smoothing_row_count, 
                                 data->smoothing_alpha_data, data->smoothing_alpha_row_count, 
                                 data->table_data_main->epochs, data->table_data_main->row_count)) { // Используем table_data_main
             debug_print("Ошибка записи результатов в results.txt\n");
        }
    }

    debug_print("update_level1_content: Вызов gtk_widget_queue_draw для графиков.\n");
    if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
    if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);

    debug_print("update_level1_content: Завершено\n");
}


// Остальная часть main.c (load_and_display_main_table, open_sqlite_file_callback, open_sqlite_file, on_level1_window_destroy, on_level_clicked, add_new_row, activate, main)
// ДОЛЖНА БЫТЬ ТАКОЙ ЖЕ, как в моем предыдущем ответе (от 21 мая 2024, 20:23 UTC)

static void load_and_display_main_table(AppData *data, const char* table_name_to_load) {
    debug_print("load_and_display_main_table: Загрузка таблицы '%s'\n", table_name_to_load);

    // Используем data->table_data_main и связанные с ним поля
    if (data->table_data_main->table) {
        for (int i = 0; i < data->table_data_main->row_count; i++) {
            if (data->table_data_main->table[i]) g_free(data->table_data_main->table[i]);
        }
        g_free(data->table_data_main->table); data->table_data_main->table = NULL;
    }
    if (data->table_data_main->epochs) { g_free(data->table_data_main->epochs); data->table_data_main->epochs = NULL; }
    if (data->table_data_main->column_names) {
        for (int i = 0; i <= data->table_data_main->column_count; i++) { 
             if (data->table_data_main->column_names[i]) g_free(data->table_data_main->column_names[i]);
        }
        g_free(data->table_data_main->column_names); data->table_data_main->column_names = NULL;
    }
    data->table_data_main->row_count = 0;
    data->table_data_main->column_count = 0; 
    
    g_free(data->current_main_table_name); 
    data->current_main_table_name = g_strdup(table_name_to_load);
    load_table(*data->db, data->table_data_main, data->current_main_table_name); 

    update_tree_view_columns(data, FALSE, FALSE, FALSE); // Обновляет data->tree_view_main и data->store_main

    if (data->store_main) { 
        gtk_list_store_clear(data->store_main);
        for (int i = 0; i < data->table_data_main->row_count; i++) {
            GtkTreeIter tree_iter;
            gtk_list_store_append(data->store_main, &tree_iter);
            gtk_list_store_set(data->store_main, &tree_iter, 0, data->table_data_main->epochs[i], -1);
            for (int j = 0; j < data->table_data_main->column_count; j++) {
                gtk_list_store_set(data->store_main, &tree_iter, j + 1, data->table_data_main->table[i][j], -1);
            }
        }
    }

    gtk_widget_queue_draw(data->tree_view_main);
    gtk_widget_set_visible(data->tree_view_main, TRUE);
    gtk_widget_queue_resize(data->tree_view_main); 
    
    gtk_widget_set_sensitive(data->add_row_button, data->table_data_main->row_count >=2 && data->table_data_main->column_count > 0);
    gtk_widget_set_sensitive(data->level1_button, data->table_data_main->row_count > 0);
    debug_print("load_and_display_main_table: Таблица '%s' для ОСНОВНОГО tree_view загружена и отображена.\n", table_name_to_load);
}


static void open_sqlite_file_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    AppData *data = user_data;
    GError *error = NULL;

    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), res, &error);
    if (error) {
        debug_print("Ошибка при выборе файла: %s\n", error->message);
        show_message_dialog(data->parent, error->message);
        g_error_free(error);
        if(file) g_object_unref(file);
        return;
    }

    if (file) {
        gchar *filename_utf8 = g_file_get_path(file);
        if (!filename_utf8) {
            debug_print("Не удалось получить путь к файлу\n");
            show_message_dialog(data->parent, "Не удалось получить путь к файлу.");
            if(file) g_object_unref(file); 
            return;
        }
        debug_print("Выбран файл: %s\n", filename_utf8);

        if (data->db_opened && *data->db) {
             sqlite3_close(*data->db);
             *data->db = NULL;
        }
        // Очистка current_main_table_name и данных/стора основной таблицы
        if (data->current_main_table_name) {
            g_free(data->current_main_table_name);
            data->current_main_table_name = NULL;
        }
        if (data->store_main) gtk_list_store_clear(data->store_main);
        if(data->tree_view_main) gtk_widget_set_visible(data->tree_view_main, FALSE); 
        
        // Очистка данных/стора для таблицы "Значения"
        if (data->table_data_values->table) { 
            for (int i = 0; i < data->table_data_values->row_count; i++) { if (data->table_data_values->table[i]) g_free(data->table_data_values->table[i]); }
            g_free(data->table_data_values->table); data->table_data_values->table = NULL;
        }
        if (data->table_data_values->epochs) { g_free(data->table_data_values->epochs); data->table_data_values->epochs = NULL; }
        if (data->table_data_values->column_names) { 
             int ncn = data->table_data_values->column_count + 1;
             for(int i=0; i<ncn; ++i) { if(data->table_data_values->column_names[i]) g_free(data->table_data_values->column_names[i]);}
             g_free(data->table_data_values->column_names); data->table_data_values->column_names = NULL;
        }
        data->table_data_values->row_count = 0; data->table_data_values->column_count = 0;
        if (data->store_values) gtk_list_store_clear(data->store_values);
        if(data->tree_view_values) gtk_widget_set_visible(data->tree_view_values, FALSE);

        // Очистка картинки
        if (data->pixbuf_schema) { g_object_unref(data->pixbuf_schema); data->pixbuf_schema = NULL; }
        if (data->picture_schema) gtk_picture_set_pixbuf(GTK_PICTURE(data->picture_schema), NULL);
        if (data->picture_schema) gtk_widget_set_visible(data->picture_schema, FALSE);

        // Деактивация кнопок
        gtk_widget_set_sensitive(data->add_row_button, FALSE); 
        gtk_widget_set_sensitive(data->level1_button, FALSE);
        data->db_opened = FALSE; 

        *data->db = open_database(filename_utf8); 
        if (*data->db) {
            data->db_opened = TRUE;
            debug_print("База данных успешно открыта: %s\n", filename_utf8);
            
            GList *table_names_list = get_table_list(*data->db); 
            if (table_names_list && g_list_length(table_names_list) > 0) {
                char *main_analysis_table_name = NULL;
                // Ищем таблицу, которая НЕ является "Значения"
                for (GList *l = table_names_list; l != NULL; l = l->next) {
                    if (strcmp((char*)l->data, data->values_table_name) != 0) {
                        main_analysis_table_name = g_strdup((char*)l->data);
                        break;
                    }
                }
                // Если не нашли (например, в БД только таблица "Значения" или другие специфические),
                // то можно взять первую из списка как основную для анализа, если это осмысленно.
                // Или выдать ошибку, что подходящей таблицы для анализа нет.
                if (!main_analysis_table_name && g_list_length(table_names_list) > 0) { 
                     debug_print("Не найдено таблицы, отличной от '%s'. Будет использована первая таблица из списка: %s\n", 
                                data->values_table_name, (char*)g_list_nth_data(table_names_list, 0));
                     main_analysis_table_name = g_strdup((char*)g_list_nth_data(table_names_list, 0));
                }


                if (main_analysis_table_name) {
                    load_and_display_main_table(data, main_analysis_table_name);
                    g_free(main_analysis_table_name);
                } else {
                     debug_print("Не найдено подходящей основной таблицы в БД.\n");
                     show_message_dialog(data->parent, "Не найдено таблицы для основного анализа.");
                     // Очистка TableData для main, если она была как-то заполнена
                     if(data->table_data_main && data->table_data_main->table){ /*...*/ }
                }

            } else { 
                debug_print("В базе нет таблиц для основной функциональности.\n");
                show_message_dialog(data->parent, "В базе данных нет таблиц для анализа.");
            }
            if (table_names_list) g_list_free_full(table_names_list, g_free); 

            // Загрузка таблицы "Значения"
            debug_print("Загрузка таблицы '%s'...\n", data->values_table_name);
            if (data->table_data_values->table) {  for (int i = 0; i < data->table_data_values->row_count; i++) { if (data->table_data_values->table[i]) g_free(data->table_data_values->table[i]); } g_free(data->table_data_values->table); data->table_data_values->table = NULL; }
            if (data->table_data_values->epochs) { g_free(data->table_data_values->epochs); data->table_data_values->epochs = NULL; }
            if (data->table_data_values->column_names) { int ncn = data->table_data_values->column_count + 1; for(int i=0; i<ncn; ++i) { if(data->table_data_values->column_names[i]) g_free(data->table_data_values->column_names[i]);} g_free(data->table_data_values->column_names); data->table_data_values->column_names = NULL; }
            data->table_data_values->row_count = 0; data->table_data_values->column_count = 0;
            
            load_table(*data->db, data->table_data_values, data->values_table_name); 
            update_values_table_view(data);

            load_and_display_schema_image_from_values_table(data);

        } else { 
            debug_print("Не удалось открыть базу данных: %s\n", filename_utf8);
            show_message_dialog(data->parent, "Не удалось открыть базу данных.");
        }
        g_free(filename_utf8);
        if(file) g_object_unref(file); 
    } else {
        debug_print("Файл не выбран (диалог отменен)\n");
    }
}

static void open_sqlite_file(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Выберите SQLite файл");
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.sqlite");
    gtk_file_filter_add_pattern(filter, "*.db");
    gtk_file_filter_add_pattern(filter, "*.sqlite3");
    gtk_file_filter_set_name(filter, "SQLite файлы (*.sqlite, *.db, *.sqlite3)"); 
    
    gtk_file_dialog_set_default_filter(dialog, filter); 
    g_object_unref(filter); 

    gtk_file_dialog_open(dialog, data->parent, NULL, open_sqlite_file_callback, data);
    g_object_unref(dialog); 
}

static void on_level1_window_destroy(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    data->level1_window = NULL; 
    data->level1_store = NULL; 
    data->level1_smoothing_store = NULL;
    data->level1_alpha_smoothing_store = NULL;

    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; i++) { if(data->smoothing_data[i]) g_free(data->smoothing_data[i]); }
        g_free(data->smoothing_data); data->smoothing_data = NULL;
    }
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; i++) { if(data->smoothing_alpha_data[i]) g_free(data->smoothing_alpha_data[i]); }
        g_free(data->smoothing_alpha_data); data->smoothing_alpha_data = NULL;
    }
    if (data->mu_data) { g_free(data->mu_data); data->mu_data = NULL; }
    if (data->alpha_data) { g_free(data->alpha_data); data->alpha_data = NULL; }
    data->smoothing_row_count = 0;
    data->smoothing_alpha_row_count = 0;
    data->alpha_count = 0;
}


static void on_level_clicked(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    const char *label = gtk_button_get_label(GTK_BUTTON(widget));

    if (g_strcmp0(label, "Уровень 1") == 0) {
        if (!data->db_opened || !data->current_main_table_name) { 
            show_message_dialog(data->parent, "Сначала откройте базу данных и выберите (основную) таблицу.");
            return;
        }
        if (data->table_data_main == NULL || data->table_data_main->row_count == 0){ 
             show_message_dialog(data->parent, "Основная таблица пуста. Невозможно выполнить расчеты.");
            return;
        }

        if (!data->level1_window) { 
            data->level1_window = gtk_window_new();
            gtk_window_set_title(GTK_WINDOW(data->level1_window), "Уровень 1: Анализ данных");
            gtk_window_set_default_size(GTK_WINDOW(data->level1_window), 1450, 900); 
            gtk_window_set_transient_for(GTK_WINDOW(data->level1_window), data->parent);
            g_signal_connect(data->level1_window, "destroy", G_CALLBACK(on_level1_window_destroy), data);

            GtkWidget *level1_scrolled_window = gtk_scrolled_window_new();
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(level1_scrolled_window),
                                           GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
            gtk_window_set_child(GTK_WINDOW(data->level1_window), level1_scrolled_window);

            data->level1_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_margin_start(data->level1_container, 10);
            gtk_widget_set_margin_end(data->level1_container, 10); 
            gtk_widget_set_margin_top(data->level1_container, 10);
            gtk_widget_set_margin_bottom(data->level1_container, 10);
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(level1_scrolled_window), data->level1_container);

            GtkWidget *left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
            gtk_widget_set_hexpand(left_vbox, TRUE); 
            gtk_box_append(GTK_BOX(data->level1_container), left_vbox);

            GtkWidget *main_table_label = gtk_label_new("Данные основной таблицы (с μ и α)");
            gtk_widget_set_halign(main_table_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(left_vbox), main_table_label);

            GtkWidget *main_table_scrolled = gtk_scrolled_window_new();
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
            gtk_widget_set_vexpand(main_table_scrolled, TRUE); 
            gtk_widget_set_size_request(main_table_scrolled, -1, 300); 
            data->level1_tree_view = gtk_tree_view_new();
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_table_scrolled), data->level1_tree_view);
            gtk_box_append(GTK_BOX(left_vbox), main_table_scrolled);

            GtkWidget *mu_graph_label = gtk_label_new("График сглаживания μ");
            gtk_widget_set_halign(mu_graph_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(left_vbox), mu_graph_label);

            data->level1_drawing_area = gtk_drawing_area_new();
            gtk_widget_set_size_request(data->level1_drawing_area, -1, 400); 
            gtk_widget_set_vexpand(data->level1_drawing_area, TRUE);
            gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_drawing_area), draw_graph, data, NULL);
            gtk_box_append(GTK_BOX(left_vbox), data->level1_drawing_area);

            GtkWidget *right_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
            gtk_widget_set_hexpand(right_vbox, TRUE); 
            gtk_box_append(GTK_BOX(data->level1_container), right_vbox);

            GtkWidget *smoothing_mu_label = gtk_label_new("Таблица сглаживания μ");
            gtk_widget_set_halign(smoothing_mu_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(right_vbox), smoothing_mu_label);
            GtkWidget *smoothing_mu_scrolled = gtk_scrolled_window_new();
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
            gtk_widget_set_size_request(smoothing_mu_scrolled, -1, 150); 
            data->level1_smoothing_view = gtk_tree_view_new();
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), data->level1_smoothing_view);
            gtk_box_append(GTK_BOX(right_vbox), smoothing_mu_scrolled);

            GtkWidget *smoothing_alpha_label = gtk_label_new("Таблица сглаживания α");
            gtk_widget_set_halign(smoothing_alpha_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(right_vbox), smoothing_alpha_label);
            GtkWidget *smoothing_alpha_scrolled = gtk_scrolled_window_new();
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_alpha_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
            gtk_widget_set_size_request(smoothing_alpha_scrolled, -1, 150);
            data->level1_alpha_smoothing_view = gtk_tree_view_new();
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(smoothing_alpha_scrolled), data->level1_alpha_smoothing_view);
            gtk_box_append(GTK_BOX(right_vbox), smoothing_alpha_scrolled);

            GtkWidget *alpha_graph_label = gtk_label_new("График сглаживания α");
            gtk_widget_set_halign(alpha_graph_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(right_vbox), alpha_graph_label);
            data->level1_alpha_drawing_area = gtk_drawing_area_new();
            gtk_widget_set_size_request(data->level1_alpha_drawing_area, -1, 400); 
            gtk_widget_set_vexpand(data->level1_alpha_drawing_area, TRUE);
            gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_alpha_drawing_area), draw_graph, data, NULL);
            gtk_box_append(GTK_BOX(right_vbox), data->level1_alpha_drawing_area);
            
            update_tree_view_columns(data, TRUE, FALSE, FALSE); 
            update_tree_view_columns(data, FALSE, TRUE, FALSE); 
            update_tree_view_columns(data, FALSE, FALSE, TRUE); 
        }

        update_level1_content(data); 
        gtk_widget_set_visible(data->level1_window, TRUE);
        show_message_dialog(GTK_WINDOW(data->level1_window), 
                            "Расчеты выполнены. Результаты также в results.txt.");
    } else {
        show_message_dialog(data->parent, "Функция для этого уровня не реализована.");
    }
}


static void add_new_row(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;

    if (!data->db_opened) {
        show_message_dialog(data->parent, "Сначала откройте базу данных.");
        return;
    }
    if (!data->current_main_table_name) { // Используем current_main_table_name
        show_message_dialog(data->parent, "Сначала выберите (основную) таблицу (откройте файл).");
        return;
    }
    if (data->table_data_main == NULL || data->table_data_main->column_count == 0) { 
        show_message_dialog(data->parent, "В основной таблице нет колонок данных для прогноза.");
        return;
    }
     if (data->table_data_main->row_count < 2) { 
        show_message_dialog(data->parent, "Нужно минимум 2 строки в основной таблице для прогноза.");
        return;
    }

    double *new_coordinates = calculate_new_row(data->table_data_main); // Работаем с основной таблицей
    if (!new_coordinates) {
        show_message_dialog(data->parent, "Не удалось вычислить новую строку.");
        return;
    }

    int new_epoch = (data->table_data_main->row_count > 0) ? (data->table_data_main->epochs[data->table_data_main->row_count - 1] + 1) : 1;

    if (!insert_row_into_table(*data->db, data->current_main_table_name, new_epoch, new_coordinates,
                              data->table_data_main->column_count, data->table_data_main->column_names)) {
        show_message_dialog(data->parent, "Ошибка при добавлении строки в базу данных.");
        g_free(new_coordinates);
        return;
    }

    load_and_display_main_table(data, data->current_main_table_name); 


    if (data->level1_window && gtk_widget_get_visible(data->level1_window)) {
        update_level1_content(data); 
    }
 
    g_free(new_coordinates);
    show_message_dialog(data->parent, "Новая строка успешно добавлена в основную таблицу.");
}


static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = user_data;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Анализ данных временных рядов");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800); 
    data->parent = GTK_WINDOW(window);

    data->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), data->main_box);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_name(button_box, "button_box"); 
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(button_box, 5);
    gtk_widget_set_margin_bottom(button_box, 5);
    gtk_box_append(GTK_BOX(data->main_box), button_box);

    data->level1_button = gtk_button_new_with_label("Уровень 1");
    gtk_widget_set_sensitive(data->level1_button, FALSE); 
    g_signal_connect(data->level1_button, "clicked", G_CALLBACK(on_level_clicked), data);
    gtk_box_append(GTK_BOX(button_box), data->level1_button);

    const char* levels[] = {"Уровень 2", "Уровень 3", "Уровень 4"};
    for (int i=0; i < 3; ++i) {
        GtkWidget *level_button = gtk_button_new_with_label(levels[i]);
        gtk_widget_set_sensitive(level_button, FALSE); 
        g_signal_connect(level_button, "clicked", G_CALLBACK(on_level_clicked), data); 
        gtk_box_append(GTK_BOX(button_box), level_button);
    }

    data->open_button = gtk_button_new_with_label("Открыть SQLite файл");
    gtk_widget_set_halign(data->open_button, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(data->open_button, 5);
    gtk_widget_set_margin_bottom(data->open_button, 5);
    gtk_box_append(GTK_BOX(data->main_box), data->open_button); 
    g_signal_connect(data->open_button, "clicked", G_CALLBACK(open_sqlite_file), data);
    
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(main_paned, TRUE); 
    gtk_paned_set_position(GTK_PANED(main_paned), 700); 
    gtk_box_append(GTK_BOX(data->main_box), main_paned);

    data->scrolled_window_main = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window_main), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(data->scrolled_window_main, TRUE); 
    gtk_widget_set_hexpand(data->scrolled_window_main, TRUE);
    data->tree_view_main = gtk_tree_view_new(); 
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view_main), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data->scrolled_window_main), data->tree_view_main);
    gtk_widget_set_visible(data->tree_view_main, FALSE); 
    gtk_paned_set_start_child(GTK_PANED(main_paned), data->scrolled_window_main);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), TRUE); 

    GtkWidget *right_panel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(right_panel_box, TRUE);
    gtk_widget_set_margin_start(right_panel_box, 5); 
    gtk_widget_set_margin_end(right_panel_box, 5);
    gtk_paned_set_end_child(GTK_PANED(main_paned), right_panel_box);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);

    GtkWidget* label_values_table = gtk_label_new("Таблица 'Значения':");
    gtk_widget_set_halign(label_values_table, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label_values_table, 5);
    gtk_box_append(GTK_BOX(right_panel_box), label_values_table);

    data->scrolled_window_values = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window_values), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(data->scrolled_window_values, -1, 150); 
    gtk_widget_set_vexpand(data->scrolled_window_values, FALSE); 
    data->tree_view_values = gtk_tree_view_new();
    g_object_set_data(G_OBJECT(data->tree_view_values), "app_data_ptr_for_debug", data); // <--- ВОТ ЭТА СТРОКА
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
    gtk_widget_set_vexpand(data->picture_schema, TRUE); 
    gtk_widget_set_hexpand(data->picture_schema, TRUE);
    gtk_widget_set_valign(data->picture_schema, GTK_ALIGN_FILL); 
    gtk_widget_set_halign(data->picture_schema, GTK_ALIGN_FILL); 
    gtk_widget_set_visible(data->picture_schema, FALSE); 
    
    GtkWidget* frame_picture = gtk_frame_new(NULL); 
    gtk_frame_set_child(GTK_FRAME(frame_picture), data->picture_schema);
    gtk_widget_set_vexpand(frame_picture, TRUE); 
    gtk_box_append(GTK_BOX(right_panel_box), frame_picture);
    
    data->add_row_button = gtk_button_new_with_label("Добавить строку (в основную таблицу)");
    gtk_widget_set_halign(data->add_row_button, GTK_ALIGN_CENTER); 
    gtk_widget_set_margin_top(data->add_row_button, 5);
    gtk_widget_set_margin_bottom(data->add_row_button, 5);
    gtk_box_append(GTK_BOX(data->main_box), data->add_row_button); 
    g_signal_connect(data->add_row_button, "clicked", G_CALLBACK(add_new_row), data);
    gtk_widget_set_sensitive(data->add_row_button, FALSE);

    gtk_widget_set_visible(window, TRUE); 
}

int main(int argc, char **argv) {
    init_debug_log(); 
    debug_print("Программа запущена (main.c: main())\n");

    GtkApplication *app = gtk_application_new("org.example.timeseriesanalyzer", G_APPLICATION_DEFAULT_FLAGS);

    static TableData table_data_main_instance = {0}; 
    static TableData table_data_values_instance = {0};
    static sqlite3 *db_instance = NULL;     

    static AppData data = {0}; 
    data.db = &db_instance;
    data.table_data_main = &table_data_main_instance; 
    data.table_data_values = &table_data_values_instance;

    data.values_table_name = "Значения"; 
    data.schema_blob_column_name = "Схема"; 


    g_signal_connect(app, "activate", G_CALLBACK(activate), &data);
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    debug_print("Программа завершает работу (main.c: main())\n");

    if (data.db && *data.db) { 
        sqlite3_close(*data.db); *data.db = NULL; 
    }
    if (data.table_data_main) { 
        if (data.table_data_main->table) {
            for (int i = 0; i < data.table_data_main->row_count; i++) {
                if (data.table_data_main->table[i]) g_free(data.table_data_main->table[i]);
            }
            g_free(data.table_data_main->table); data.table_data_main->table = NULL;
        }
        if (data.table_data_main->epochs) {
            g_free(data.table_data_main->epochs); data.table_data_main->epochs = NULL;
        }
        if (data.table_data_main->column_names) { 
            int num_col_names = data.table_data_main->column_count + 1;
            for (int i = 0; i < num_col_names; i++) { 
                if (data.table_data_main->column_names[i]) 
                    g_free(data.table_data_main->column_names[i]);
            }
            g_free(data.table_data_main->column_names); data.table_data_main->column_names = NULL;
        }
    }
    g_free(data.current_main_table_name); data.current_main_table_name = NULL; 

     if (data.table_data_values) { 
        if (data.table_data_values->table) {
            for (int i = 0; i < data.table_data_values->row_count; i++) {
                if (data.table_data_values->table[i]) g_free(data.table_data_values->table[i]);
            }
            g_free(data.table_data_values->table); data.table_data_values->table = NULL;
        }
        if (data.table_data_values->epochs) {
            g_free(data.table_data_values->epochs); data.table_data_values->epochs = NULL;
        }
        if (data.table_data_values->column_names) { 
            int num_col_names = data.table_data_values->column_count + 1;
            for (int i = 0; i < num_col_names; i++) { 
                if (data.table_data_values->column_names[i]) 
                    g_free(data.table_data_values->column_names[i]);
            }
            g_free(data.table_data_values->column_names); data.table_data_values->column_names = NULL;
        }
    }

    if (data.pixbuf_schema) {
        g_object_unref(data.pixbuf_schema); data.pixbuf_schema = NULL;
    }

    if (data.smoothing_data) {
        for (int i = 0; i < data.smoothing_row_count; i++) { if (data.smoothing_data[i]) g_free(data.smoothing_data[i]); }
        g_free(data.smoothing_data); data.smoothing_data = NULL;
    }
    if (data.smoothing_alpha_data) {
        for (int i = 0; i < data.smoothing_alpha_row_count; i++) { if(data.smoothing_alpha_data[i]) g_free(data.smoothing_alpha_data[i]); }
        g_free(data.smoothing_alpha_data); data.smoothing_alpha_data = NULL;
    }
    if (data.mu_data) {g_free(data.mu_data); data.mu_data = NULL;}
    if (data.alpha_data) {g_free(data.alpha_data); data.alpha_data = NULL;}

    if (app) g_object_unref(app); 

    if (debug_log) {
        fprintf(debug_log, "Лог-файл закрыт (main.c)\n");
        fclose(debug_log);
        debug_log = NULL;
    }
    return status;
}

#pragma GCC diagnostic pop