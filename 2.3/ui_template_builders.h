#ifndef UI_TEMPLATE_BUILDERS_H
#define UI_TEMPLATE_BUILDERS_H

#include <gtk/gtk.h>
#include "level1_analysis.h" // Для AppData

// Enum для определения типа данных/графика, который обрабатывает шаблон
typedef enum {
     GRAPH_DATA_TYPE_MU,
     GRAPH_DATA_TYPE_ALPHA,
     GRAPH_DATA_TYPE_MU_PRIME,
     GRAPH_DATA_TYPE_ALPHA_PRIME,
     GRAPH_DATA_TYPE_MU_DOUBLE_PRIME,
     GRAPH_DATA_TYPE_ALPHA_DOUBLE_PRIME,
     GRAPH_DATA_TYPE_UNKNOWN
} GraphDataType;

typedef enum {
    CONCLUSION_GRAPH_TYPE_ALPHA_VS_MU // Единственный тип пока
} ConclusionGraphDataType;

// Прототип шаблонной функции для создания секции сглаживания (таблица + график)
void ui_template_create_smoothing_section(AppData *data,
                                          GtkBox *parent_vbox,
                                          const char *section_title_base,
                                          GraphDataType graph_type, // Тип уже без X0
                                          GtkWidget **out_smoothing_view_ptr,
                                          GtkListStore **out_smoothing_store_ptr,
                                          GtkWidget **out_drawing_area_ptr,
                                          GtkWidget *tab_scrolled_window_for_event);

                                          
void ui_template_create_conclusion_alpha_mu_section(AppData *data,
                                                    GtkBox *parent_vbox,
                                                    const char *section_title,
                                                    ConclusionGraphDataType graph_type, // Для возможного расширения
                                                    GtkWidget **out_table_view_ptr,
                                                    GtkListStore **out_table_store_ptr,
                                                    GtkWidget **out_drawing_area_ptr,
                                                    GtkWidget *tab_scrolled_window_for_event);

// TODO: Позже добавим прототип для ui_template_create_main_data_section

#endif // UI_TEMPLATE_BUILDERS_H