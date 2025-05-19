#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "ui_template_builders.h"


extern gboolean on_inner_scroll_event (GtkEventController *controller_generic, double dx, double dy, gpointer user_data);

extern void conclusion_draw_alpha_mu_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);

// --- Реализация шаблонной функции ---

void ui_template_create_smoothing_section(AppData *data,
                                          GtkBox *parent_vbox,
                                          const char *section_title_base,
                                          GraphDataType graph_type,
                                          GtkWidget **out_smoothing_view_ptr,
                                          GtkListStore **out_smoothing_store_ptr,
                                          GtkWidget **out_drawing_area_ptr,
                                          GtkWidget *tab_scrolled_window_for_event) // 8-й параметр
{
    char full_section_title[256];
    snprintf(full_section_title, sizeof(full_section_title), "<b>%s</b>", section_title_base);

    GtkWidget *block_label = gtk_label_new(full_section_title);
    gtk_label_set_use_markup(GTK_LABEL(block_label), TRUE);
    gtk_widget_set_halign(block_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(block_label, 10); // Отступ сверху для секции
    gtk_box_append(parent_vbox, block_label);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_hexpand(hbox, TRUE);
    gtk_widget_set_vexpand(hbox, TRUE); // Позволяем этому блоку растягиваться вертикально
    gtk_box_append(parent_vbox, hbox);

    // 1. Таблица сглаживания (слева)
    GtkWidget *scrolled_window_table = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_table), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_set_vexpand(scrolled_window_table, FALSE); // Высота по содержимому
    gtk_widget_set_hexpand(scrolled_window_table, FALSE); // Не растягивать горизонтально
    gtk_widget_set_size_request(scrolled_window_table, 480, -1); // Предпочтительная ширина
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrolled_window_table), TRUE);
    
    GtkWidget *tree_view = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    if (GTK_IS_WIDGET(tree_view)) {
        g_object_set_data(G_OBJECT(tree_view), "app_data_ptr_for_debug", data); // Для cell_data_func
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_table), tree_view);
    gtk_box_append(GTK_BOX(hbox), scrolled_window_table);

    // Подключаем контроллер прокрутки к GtkScrolledWindow таблицы
    GtkEventController *scroll_controller_table = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    // Передаем скроллер текущей вкладки в user_data для обработчика on_inner_scroll_event
    g_signal_connect (scroll_controller_table, "scroll", G_CALLBACK (on_inner_scroll_event), tab_scrolled_window_for_event);
    gtk_widget_add_controller(scrolled_window_table, scroll_controller_table);

    // 2. График сглаживания (справа)
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_widget_set_hexpand(drawing_area, TRUE);
    
    // Сохраняем тип графика в user_data для drawing_area,
    // чтобы функция отрисовки знала, какие данные использовать.
    // Ключ должен быть уникальным.
    g_object_set_data_full(G_OBJECT(drawing_area), "graph_type_enum_key", GINT_TO_POINTER(graph_type), NULL);
    
    // Подключаем функцию отрисовки. Предполагается, что level1_draw_graph определена
    // в level1_analysis.c и доступна (не static, или этот файл компилируется вместе).
    // Если level1_draw_graph static, то этот вызов не сработает при раздельной компиляции.
    // В этом случае level1_draw_graph нужно либо сделать не-static, либо перенести сюда.
    // Для gtk_drawing_area_set_draw_func достаточно, чтобы функция была где-то определена.
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), level1_draw_graph, data, NULL);
    gtk_box_append(GTK_BOX(hbox), drawing_area);

    // Возвращаем созданные виджеты через выходные параметры (указатели на указатели)
    if (out_smoothing_view_ptr) {
        *out_smoothing_view_ptr = tree_view;
    }
    if (out_drawing_area_ptr) {
        *out_drawing_area_ptr = drawing_area;
    }
    
    // GtkListStore будет создан и присвоен позже, при вызове update_tree_view_columns
    // из level1_show_window. Здесь мы просто инициализируем указатель на NULL,
    // чтобы вызывающая функция знала, что store еще не создан шаблоном.
    if (out_smoothing_store_ptr) {
        *out_smoothing_store_ptr = NULL; 
    }

    // Примечание: Настройка колонок для 'tree_view' (и создание его 'GtkListStore')
    // будет выполнена позже функцией update_tree_view_columns, вызываемой из level1_show_window.
    // update_tree_view_columns должна будет использовать *out_smoothing_view_ptr и *out_smoothing_store_ptr
    // (точнее, поля AppData, на которые они указывают) для своей работы.
}

void ui_template_create_conclusion_alpha_mu_section(AppData *data,
                                                    GtkBox *parent_vbox,
                                                    const char *section_title_base,
                                                    ConclusionGraphDataType graph_type,
                                                    GtkWidget **out_table_view_ptr,
                                                    GtkListStore **out_table_store_ptr,
                                                    GtkWidget **out_drawing_area_ptr,
                                                    GtkWidget *tab_scrolled_window_for_event)
{
    char full_section_title[256];
    snprintf(full_section_title, sizeof(full_section_title), "<b>%s</b>", section_title_base);

    GtkWidget *block_label = gtk_label_new(full_section_title);
    gtk_label_set_use_markup(GTK_LABEL(block_label), TRUE);
    gtk_widget_set_halign(block_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(block_label, 15);
    gtk_box_append(parent_vbox, block_label);

    // Основной горизонтальный контейнер для таблицы и графика
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_hexpand(hbox, TRUE);
    gtk_widget_set_vexpand(hbox, TRUE); 
    gtk_box_append(parent_vbox, hbox);

    // 1. Таблица (слева)
    GtkWidget *scrolled_window_table = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_table), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); // Разрешим верт. прокрутку
    gtk_widget_set_vexpand(scrolled_window_table, TRUE); // Растягивать таблицу вертикально
    gtk_widget_set_hexpand(scrolled_window_table, FALSE); 
    gtk_widget_set_size_request(scrolled_window_table, 450, -1); // Ширина таблицы
    
    GtkWidget *tree_view = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    if (GTK_IS_WIDGET(tree_view)) {
        g_object_set_data(G_OBJECT(tree_view), "app_data_ptr_for_debug", data); 
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_table), tree_view);
    gtk_box_append(GTK_BOX(hbox), scrolled_window_table);

    // Контроллер прокрутки для таблицы, если ее собственный скроллбар не виден
    // (может быть избыточен, если у таблицы всегда есть свой скроллбар)
    GtkEventController *scroll_controller_table = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect (scroll_controller_table, "scroll", G_CALLBACK (on_inner_scroll_event), tab_scrolled_window_for_event);
    gtk_widget_add_controller(scrolled_window_table, scroll_controller_table);


    // 2. График (справа)
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_widget_set_hexpand(drawing_area, TRUE);
    
    // Сохраняем тип графика (хотя здесь он пока один)
    g_object_set_data_full(G_OBJECT(drawing_area), "conclusion_graph_type_key", GINT_TO_POINTER(graph_type), NULL);
    
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), conclusion_draw_alpha_mu_graph, data, NULL);
    gtk_box_append(GTK_BOX(hbox), drawing_area);

    // Возвращаем созданные виджеты
    if (out_table_view_ptr) *out_table_view_ptr = tree_view;
    if (out_drawing_area_ptr) *out_drawing_area_ptr = drawing_area;
    if (out_table_store_ptr) *out_table_store_ptr = NULL; // Store будет создан позже
}

#pragma GCC diagnostic pop