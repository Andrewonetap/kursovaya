// Включаем ОБЩИЙ заголовок с определением AppData
#include "level1_analysis.h"
// Остальные необходимые заголовки
#include "calculations.h"
#include "sqlite_utils.h" // Для TableData и прочего (хотя level1_analysis.h его уже включает)

#include <gtk/gtk.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

// Объявления функций из main.c, которые мы вызываем (они теперь не static)
// НЕ ХОРОШО, но пока оставляем для работоспособности.
// Правильнее сделать отдельный "ui_utils.h" или передавать колбэки.
extern void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table);
extern void show_message_dialog(GtkWindow *parent, const char *message);
extern void cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);

// --- ЛОКАЛЬНЫЙ ЛОГГЕР ДЛЯ ЭТОГО ФАЙЛА ---
// Пишем в тот же файл debug.log
static FILE *debug_log_l1 = NULL;

static void init_l1_debug_log() {
    if (!debug_log_l1) {
        debug_log_l1 = fopen("debug.log", "a");
        if (!debug_log_l1) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (level1_analysis.c)\n");
        } else {
            fseek(debug_log_l1, 0, SEEK_END);
            if (ftell(debug_log_l1) == 0) {
                fprintf(debug_log_l1, "\xEF\xBB\xBF"); // BOM
            }
            // fprintf(debug_log_l1, "Лог-файл открыт/доступен (level1_analysis.c)\n");
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

// === Вспомогательные статические функции, ПЕРЕНЕСЕННЫЕ из main.c ===
// (или функции, которые теперь нужны здесь)

// Нужна forward-декларация, т.к. используется в draw_graph и on_destroy
static void level1_clear_internal_data(AppData *data); 

// Функция отрисовки графика (бывшая draw_graph в main.c)
// ПЕРЕНЕСЕНА СЮДА и сделана статической
static void level1_draw_graph(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = user_data;
    gboolean is_alpha_graph = (drawing_area == GTK_DRAWING_AREA(data->level1_alpha_drawing_area));
    // debug_print_l1("level1_draw_graph: НАЧАЛО отрисовки графика (%s), width=%d, height=%d\n", is_alpha_graph ? "α" : "μ", width, height);


    double **graph_smoothing_data = is_alpha_graph ? data->smoothing_alpha_data : data->smoothing_data;
    int graph_smoothing_row_count = is_alpha_graph ? data->smoothing_alpha_row_count : data->smoothing_row_count;

    double *real_plot_data = NULL;
    int real_plot_data_count = 0;

    // --- Получение указателей на РЕАЛЬНЫЕ данные для графика ---
    if (is_alpha_graph) { // Alpha график
        // Реальные alpha для графика - это все, кроме alpha_data[0]
        if (data->alpha_data && data->alpha_count > 1) {
            real_plot_data = data->alpha_data + 1; // Начинаем со второго элемента
            real_plot_data_count = data->alpha_count - 1;
        }
    } else { // Mu график
        // Реальные mu - это все mu_data, их количество равно row_count основной таблицы
        if (data->mu_data && data->table_data_main && data->table_data_main->row_count > 0) {
            real_plot_data = data->mu_data;
            real_plot_data_count = data->table_data_main->row_count;
        }
    }
    // debug_print_l1("level1_draw_graph (%s): Smoothing data rows: %d, Real data points: %d\n",
    //            is_alpha_graph ? "α" : "μ", graph_smoothing_row_count, real_plot_data_count);

    gboolean has_smoothing_data = graph_smoothing_data && graph_smoothing_row_count > 0;
    gboolean has_real_data = real_plot_data && real_plot_data_count > 0;


    // --- Обработка отсутствия данных ---
    if (!has_smoothing_data && !has_real_data) {
        // ... (код отрисовки "Нет данных", как был в main.c) ...
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
        debug_print_l1("level1_draw_graph: Нет данных для графика (%s).\n", is_alpha_graph ? "α" : "μ");
        return;
    }

    // --- Расчет геометрии и масштаба (как было) ---
    double margin_left = 70.0;
    double margin_right = 20.0;
    double margin_top = 40.0;
    double margin_bottom = 90.0; // Оставляем место для легенды
    double plot_width = width - margin_left - margin_right;
    double plot_height = height - margin_top - margin_bottom;

    if (plot_width <= 1 || plot_height <= 1) {
        debug_print_l1("level1_draw_graph: Недостаточно места для отрисовки графика (plot_width=%.1f, plot_height=%.1f).\n", plot_width, plot_height);
        return;
    }

    // --- Определение диапазона Y (как было) ---
    double y_min_val = G_MAXDOUBLE, y_max_val = -G_MAXDOUBLE;
    gboolean data_found_for_yrange = FALSE;

    if(has_smoothing_data){
        for (int i = 0; i < graph_smoothing_row_count; i++) {
            // Проверяем на NULL строку с данными, если вдруг
            if (graph_smoothing_data[i] == NULL) continue;
            for (int j_idx = 0; j_idx < 4; j_idx++) { // Только первые 4 - сглаженные данные
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
        debug_print_l1("level1_draw_graph: Данные для диапазона Y не найдены (все NaN?). Использую дефолтный диапазон [-1, 1].\n");
        y_min_val = -1.0; y_max_val = 1.0;
    }
     // Расширяем диапазон Y, чтобы точки не лежали на границах (как было)
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
    if (fabs(y_range_val_calc) < 1e-9) y_range_val_calc = 1.0; // Защита от деления на 0 для y_scale


    // --- Определение максимального значения по X (M) для оси ---
    // Ось X рисуем по M, которое идет от 0 до N для сглаживания (N+1 точек)
    // или от 0 до N-1 для реальных данных (N точек)
    int x_num_points_axis = 0; // Макс. M + 1
    if(has_smoothing_data) x_num_points_axis = graph_smoothing_row_count; // от M=0 до M=N => N+1 точек
    if(has_real_data) {
         // реальные данные имеют индексы от 0 до N-1, значит N точек
         // Если M=N есть в сглаживании, то нам нужен M=N на оси.
        if( (real_plot_data_count + 1) > x_num_points_axis ) {
            // Если реальных точек N, и последняя точка M сглаживания M=N-1 (т.е. N точек сглаживания)
            // То ось все равно должна идти до M=N, если есть сглаживание до M=N
             // Пример: 3 точки реальных (N=3, индексы 0,1,2). Сглаживание M=0,1,2,3 (4 точки). x_num_points_axis=4
             // Пример: 1 точка реальных (N=1, индекс 0). Сглаживание M=0,1 (2 точки). x_num_points_axis=2. real_count=1. real_count+1=2.
            // Оставляем x_num_points_axis как есть, если сглаживание > реальных.
            // Иначе берем макс. M реальных данных + 1? Нет, берем макс из длин.
           // x_num_points_axis = real_plot_data_count; // Нет, индекс последней точки реальных N-1. Ось до N?
            // Максимальный индекс реальной точки M это real_plot_data_count - 1.
            // Максимальный индекс точки сглаживания M это graph_smoothing_row_count - 1.
             int max_real_m = real_plot_data_count > 0 ? real_plot_data_count - 1 : -1;
             int max_smooth_m = graph_smoothing_row_count > 0 ? graph_smoothing_row_count - 1 : -1;
             int max_m = max_real_m > max_smooth_m ? max_real_m : max_smooth_m;
             if (max_m >= 0) x_num_points_axis = max_m + 1; else x_num_points_axis = 1;

        }
    }
     if (x_num_points_axis == 0) x_num_points_axis = 1; // Минимальное количество точек на оси X


    // --- Расчет масштаба X и Y (как было, но x_max_ticks теперь из x_num_points_axis) ---
    double x_max_ticks = x_num_points_axis > 1 ? (x_num_points_axis - 1) : 1.0; // Максимальное значение М на оси (N или N-1)
    double x_scale = (x_max_ticks > 1e-9) ? (plot_width / x_max_ticks) : plot_width; // Масштаб: ширина графика / (макс_М - 0)
    if (x_num_points_axis == 1 && fabs(x_max_ticks) < 1e-9) x_scale = plot_width; // Для одного M

    double y_scale = plot_height / y_range_val_calc;

    // --- Отрисовка фона, осей, меток (как было) ---
    // Фон
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // Белый фон
    cairo_paint(cr);

    // Оси
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); // Черный цвет для осей и текста
    cairo_set_line_width(cr, 1.0);
    // Ось X
    cairo_move_to(cr, margin_left, height - margin_bottom);
    cairo_line_to(cr, margin_left + plot_width, height - margin_bottom);
    cairo_stroke(cr);
    // Ось Y
    cairo_move_to(cr, margin_left, margin_top);
    cairo_line_to(cr, margin_left, margin_top + plot_height);
    cairo_stroke(cr);

    // Метки оси X (рисуем для M от 0 до x_max_ticks)
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    for (int i = 0; i < x_num_points_axis; i++) { // i здесь соответствует значению M
        int tick_frequency = 1;
        if (x_num_points_axis > 15) tick_frequency = (int)ceil((double)x_num_points_axis / 10.0);
        // Показываем метку: всегда первую и последнюю, и остальные с частотой tick_frequency
        if (i == 0 || i == (x_num_points_axis -1) || (i % tick_frequency == 0)) {
            double x_pos = margin_left + i * x_scale; // x_pos = левое поле + M * масштаб_x
             if (x_num_points_axis == 1) x_pos = margin_left + plot_width / 2; // Центрируем, если точка одна
            char label[16];
            snprintf(label, sizeof(label), "%d", i); // Метка равна значению M
            cairo_text_extents_t extents;
            cairo_text_extents(cr, label, &extents);
            cairo_move_to(cr, x_pos - extents.width / 2.0, height - margin_bottom + 15);
            cairo_show_text(cr, label);

            // Рисуем риску на оси X
            cairo_move_to(cr, x_pos, height - margin_bottom);
            cairo_line_to(cr, x_pos, height - margin_bottom + 3);
            cairo_stroke(cr);
        }
    }
     // Подпись оси X
    const char *x_axis_label_str = "M (период)";
    cairo_text_extents_t x_label_extents_calc;
    cairo_text_extents(cr, x_axis_label_str, &x_label_extents_calc);
    cairo_move_to(cr, margin_left + plot_width / 2.0 - x_label_extents_calc.width / 2.0, height - margin_bottom + 35);
    cairo_show_text(cr, x_axis_label_str);


    // Метки оси Y (как было)
    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; i++) {
        double y_value = y_min_val + (y_range_val_calc * i / num_y_ticks);
        // Преобразуем значение Y в координату на экране
        double y_pos = height - margin_bottom - (y_value - y_min_val) * y_scale;
        char label[32];
        // Форматирование зависит от типа графика (μ или α)
        snprintf(label, sizeof(label), is_alpha_graph ? "%.1E" : "%.2f", y_value);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, margin_left - extents.width - 5, y_pos + extents.height / 2.0); // Слева от оси Y
        cairo_show_text(cr, label);

        // Риска на оси Y
        cairo_move_to(cr, margin_left - 3, y_pos);
        cairo_line_to(cr, margin_left, y_pos);
        cairo_stroke(cr);
    }
    // Подпись оси Y (как было)
    const char *y_axis_label_str = is_alpha_graph ? "Значение α" : "Значение μ";
    cairo_text_extents_t y_label_extents_val;
    cairo_text_extents(cr, y_axis_label_str, &y_label_extents_val);
    cairo_save(cr); // Сохраняем текущее состояние cairo (трансформации)
    cairo_move_to(cr, margin_left - 55, margin_top + plot_height / 2.0 + y_label_extents_val.width / 2.0); // Перемещаем к оси Y
    cairo_rotate(cr, -M_PI / 2.0); // Поворачиваем на -90 градусов
    cairo_show_text(cr, y_axis_label_str); // Рисуем текст
    cairo_restore(cr); // Восстанавливаем состояние (отменяем поворот)


    // --- Определение цветов и легенды (как было) ---
    double colors[5][3] = {
        {1.0, 0.0, 0.0}, // Красный A=0.1
        {0.0, 0.7, 0.0}, // Зеленый A=0.4
        {0.0, 0.0, 1.0}, // Синий   A=0.7
        {0.8, 0.6, 0.0}, // Оранжевый/Коричневый A=0.9
        {0.3, 0.3, 0.3}  // Серый    Реальные данные
    };
    const char *legend_labels[] = {"A=0.1", "A=0.4", "A=0.7", "A=0.9", is_alpha_graph ? "Реальные α" : "Реальные μ"};

    // --- Отрисовка линий и точек сглаживания ---
    if(has_smoothing_data){
        for (int j_idx_color = 0; j_idx_color < 4; j_idx_color++) { // Итерация по 4 рядам сглаживания (A=0.1 до 0.9)
            cairo_set_source_rgb(cr, colors[j_idx_color][0], colors[j_idx_color][1], colors[j_idx_color][2]);
            cairo_set_line_width(cr, 1.5);

            gboolean first_point_in_series = TRUE;
             // Идем по M от 0 до graph_smoothing_row_count - 1 (это M=0 до M=N)
            for (int i = 0; i < graph_smoothing_row_count; i++) { // i здесь = M
                 // Проверка на NULL строку
                 if (graph_smoothing_data[i] == NULL) { first_point_in_series = TRUE; continue;}
                double val_y = graph_smoothing_data[i][j_idx_color]; // Берем значение из j-той колонки сглаживания
                if (isnan(val_y) || isinf(val_y)) { // Пропускаем некорректные точки
                    first_point_in_series = TRUE;
                    continue;
                }
                double x_pos = margin_left + i * x_scale; // x = поле + M * масштаб
                if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2; // Центрируем одну точку
                double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale; // y = низ - (значение - мин_y)*масштаб_y

                // Ограничиваем рисование видимой областью графика (опционально, но полезно)
                if (y_pos < margin_top) y_pos = margin_top;
                if (y_pos > height - margin_bottom) y_pos = height - margin_bottom;

                if (first_point_in_series) {
                    cairo_move_to(cr, x_pos, y_pos); // Начинаем новую линию
                    first_point_in_series = FALSE;
                } else {
                    cairo_line_to(cr, x_pos, y_pos); // Продолжаем линию
                }
            }
            if (!first_point_in_series) cairo_stroke(cr); // Рисуем линию, если были точки

            // Отрисовка точек поверх линий
             for (int i = 0; i < graph_smoothing_row_count; i++) {
                 if (graph_smoothing_data[i] == NULL) continue;
                double val_y = graph_smoothing_data[i][j_idx_color];
                 if (isnan(val_y) || isinf(val_y)) continue;
                 double x_pos = margin_left + i * x_scale;
                 if (x_num_points_axis == 1 && graph_smoothing_row_count == 1) x_pos = margin_left + plot_width / 2;
                 double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;
                 // Рисуем точку, только если она в видимой области
                 if (y_pos < margin_top -3 || y_pos > height - margin_bottom + 3 ) continue; // Не рисуем далеко за пределами
                 cairo_arc(cr, x_pos, y_pos, 2.2, 0, 2 * M_PI); // Рисуем кружок (маленький)
                 cairo_fill(cr); // Заполняем его
             }
        }
    }

    // --- Отрисовка линии и точек реальных данных ---
    if(has_real_data){
        int real_data_color_idx = 4; // Индекс для серого цвета
        cairo_set_source_rgb(cr, colors[real_data_color_idx][0], colors[real_data_color_idx][1], colors[real_data_color_idx][2]);
        cairo_set_line_width(cr, 2.0); // Линия чуть толще
        gboolean first_point_in_series = TRUE;
        // Идем по реальным данным. Индекс 'i' соответствует M
        // Если это график Mu, то M идет от 0 до N-1 (real_plot_data_count=N)
        // Если это график Alpha, то M идет от 0 до N-2 (real_plot_data_count=N-1)
        for(int i = 0; i < real_plot_data_count; ++i){ // i - индекс в real_plot_data
             int m_value = i; // Значение М на оси равно индексу в реальных данных (mu[0] это М=0, alpha[1] это M=0 на графике alpha)

            double val_y = real_plot_data[i];
            if (isnan(val_y) || isinf(val_y)) {
                first_point_in_series = TRUE;
                continue;
            }
            double x_pos = margin_left + m_value * x_scale; // x = поле + M * масштаб
            if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2; // Центрируем
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;

            // Ограничение по Y
            if (y_pos < margin_top) y_pos = margin_top;
            if (y_pos > height - margin_bottom) y_pos = height - margin_bottom;

            if (first_point_in_series) {
                cairo_move_to(cr, x_pos, y_pos);
                first_point_in_series = FALSE;
            } else {
                cairo_line_to(cr, x_pos, y_pos);
            }
        }
        if (!first_point_in_series) cairo_stroke(cr); // Рисуем линию

        // Точки для реальных данных
        for(int i = 0; i < real_plot_data_count; ++i){
             int m_value = i;
            double val_y = real_plot_data[i];
             if (isnan(val_y) || isinf(val_y)) continue;
            double x_pos = margin_left + m_value * x_scale;
             if (x_num_points_axis == 1 && real_plot_data_count == 1) x_pos = margin_left + plot_width / 2;
            double y_pos = height - margin_bottom - (val_y - y_min_val) * y_scale;
            if (y_pos < margin_top -3 || y_pos > height - margin_bottom + 3 ) continue; // Проверяем видимость
            cairo_arc(cr, x_pos, y_pos, 2.8, 0, 2 * M_PI); // Точки чуть больше
            cairo_fill(cr);
        }
    }


    // --- Отрисовка легенды (как было) ---
    double legend_item_width = 90; // Ширина одного элемента легенды
    double legend_x_start = margin_left;
    double legend_y_start = height - margin_bottom + 50; // Ниже подписи оси X
    double legend_x_current = legend_x_start;
    double legend_y_current = legend_y_start;

    cairo_set_font_size(cr, 10); // Размер шрифта для легенды

    for (int j_idx_leg = 0; j_idx_leg < 5; j_idx_leg++) { // Итерация по всем 5 возможным элементам
        // Проверяем, есть ли данные для этого элемента легенды
        if (j_idx_leg < 4 && !has_smoothing_data) continue; // Пропускаем A=... если нет данных сглаживания
        if (j_idx_leg == 4 && !has_real_data) continue;   // Пропускаем "Реальные" если нет реальных данных

        // Проверяем, помещается ли элемент в текущей строке
        if (legend_x_current + legend_item_width > width - margin_right) {
             legend_y_current += 15; // Переходим на новую строку
             legend_x_current = legend_x_start; // Сбрасываем X
             // Если даже на новой строке не помещается, прерываем (маловероятно)
             if (legend_x_current + legend_item_width > width - margin_right) break;
        }

        // Рисуем цветной квадратик
        cairo_set_source_rgb(cr, colors[j_idx_leg][0], colors[j_idx_leg][1], colors[j_idx_leg][2]);
        cairo_rectangle(cr, legend_x_current, legend_y_current, 15, 10); // Маленький прямоугольник
        cairo_fill(cr);

        // Рисуем текст легенды
        cairo_set_source_rgb(cr, 0, 0, 0); // Черный текст
        cairo_text_extents_t text_ext_legend;
        cairo_text_extents(cr, legend_labels[j_idx_leg], &text_ext_legend);
        // Размещаем текст правее квадратика, выравнивая по вертикали
        cairo_move_to(cr, legend_x_current + 20, legend_y_current + text_ext_legend.height -1);
        cairo_show_text(cr, legend_labels[j_idx_leg]);

        legend_x_current += legend_item_width; // Передвигаем позицию для следующего элемента
    }

    // --- Отрисовка заголовка графика (как было) ---
    const char *title_str_graph = is_alpha_graph ? "Прогнозные функции Zпр(α)" : "Прогнозные функции Zпр(μ)";
    cairo_set_font_size(cr, 14); // Крупнее для заголовка
    cairo_text_extents_t title_extents_val_graph;
    cairo_text_extents(cr, title_str_graph, &title_extents_val_graph);
    // Центрируем заголовок над областью построения
    cairo_move_to(cr, margin_left + plot_width / 2.0 - title_extents_val_graph.width / 2.0, margin_top - 15);
    cairo_show_text(cr, title_str_graph);

    // debug_print_l1("level1_draw_graph: ЗАВЕРШЕНИЕ отрисовки графика (%s)\n", is_alpha_graph ? "α" : "μ");
}

// Функция обновления содержимого Level 1 (бывшая update_level1_content)
// ПЕРЕНЕСЕНА СЮДА и сделана статической
// Важно: эта функция теперь ВЫЗЫВАЕТ `update_tree_view_columns`, которая осталась в main.c
// Это нормально, так как main.c её предоставляет (хоть и статической для main.c, но
// линковщик их свяжет, т.к. компилируются вместе).
// Также она вызывает cell_data_func (косвенно через tree_view) и show_message_dialog
// из main.c.
// Прямые вызовы show_message_dialog отсюда уберем, а для update_tree_view_columns/cell_data_func - они будут работать.
// ПРЯМОЙ ВЫЗОВ функции из другого файла, которая там static - НЕЛЬЗЯ.
// Поэтому update_tree_view_columns, cell_data_func, show_message_dialog ДОЛЖНЫ
// быть НЕ СТАТИЧЕСКИМИ в main.c и объявлены в каком-то общем заголовке,
// ЛИБО нам нужны функции-обертки/колбеки, передаваемые через AppData.
// Пока оставим их static в main.c и посмотрим, будет ли ругаться линковщик. Если да - нужно будет
// делать их non-static в main.c и добавить прототипы (возможно в level1_analysis.h или новом main_utils.h)
// ---->>> ОБНОВЛЕНИЕ: Они будут работать, т.к. компиляция идет в один бинарник, вызов из on_level_clicked <<<----
// Проблема с вызовом show_message_dialog остается, лучше ее здесь не вызывать, а вернуть статус успеха/неудачи.
// Проблема с вызовом update_tree_view_columns - это ЛУЧШЕ сделать публичной функцией модуля UI (main.c).
// Сейчас попробуем оставить так, вызывая её напрямую, но это не очень хорошо для модульности.

// НОВЫЕ ВЕРСИИ main.c потребуют сделать update_tree_view_columns и cell_data_func НЕ-СТАТИЧЕСКИМИ

// Объявления функций из main.c, которые мы будем вызывать
// Это ПЛОХОЕ РЕШЕНИЕ, правильнее вынести их в общий .h файл
extern void update_tree_view_columns(AppData *data, gboolean is_level1_context, gboolean is_smoothing_mu_table, gboolean is_smoothing_alpha_table);
extern void show_message_dialog(GtkWindow *parent, const char *message);
// cell_data_func вызывается GTK, объявлять не нужно.

// Функция для очистки ТОЛЬКО вычисленных данных
static void level1_clear_internal_data(AppData *data) {
     debug_print_l1("level1_clear_internal_data: Очистка массивов данных...\n");
    // Очистка массивов данных сглаживания μ
    if (data->smoothing_data) {
        for (int i = 0; i < data->smoothing_row_count; i++) {
            if (data->smoothing_data[i]) g_free(data->smoothing_data[i]);
        }
        g_free(data->smoothing_data);
        data->smoothing_data = NULL;
        debug_print_l1(" - smoothing_data (mu) очищен (был %d строк)\n", data->smoothing_row_count);
        data->smoothing_row_count = 0;
    } else {
         // debug_print_l1(" - smoothing_data (mu) уже был NULL\n");
         data->smoothing_row_count = 0; // На всякий случай
    }

    // Очистка массивов данных сглаживания α
    if (data->smoothing_alpha_data) {
        for (int i = 0; i < data->smoothing_alpha_row_count; i++) {
            if (data->smoothing_alpha_data[i]) g_free(data->smoothing_alpha_data[i]);
        }
        g_free(data->smoothing_alpha_data);
        data->smoothing_alpha_data = NULL;
         debug_print_l1(" - smoothing_alpha_data очищен (был %d строк)\n", data->smoothing_alpha_row_count);
        data->smoothing_alpha_row_count = 0;
    } else {
        // debug_print_l1(" - smoothing_alpha_data уже был NULL\n");
         data->smoothing_alpha_row_count = 0;
    }

    // Очистка μ
    if (data->mu_data) {
        g_free(data->mu_data);
        data->mu_data = NULL;
        debug_print_l1(" - mu_data очищен\n");
    }
     // Очистка α
    if (data->alpha_data) {
        g_free(data->alpha_data);
        data->alpha_data = NULL;
         debug_print_l1(" - alpha_data очищен (был %d элементов)\n", data->alpha_count);
        data->alpha_count = 0;
    } else {
        // debug_print_l1(" - alpha_data уже был NULL\n");
         data->alpha_count = 0;
    }

     debug_print_l1("level1_clear_internal_data: Очистка завершена.\n");
}


static void level1_perform_calculations_and_update_ui(AppData *data) {
    debug_print_l1("level1_perform_calculations_and_update_ui: НАЧАЛО\n");

    // 1. Очистка предыдущих результатов расчетов
    level1_clear_internal_data(data);

    // 2. Проверка наличия исходных данных (из основной таблицы)
    // Расчеты для Уровня 1 производятся на основе table_data_main
    if(data->table_data_main == NULL || data->table_data_main->row_count == 0) {
        debug_print_l1("level1_perform_calculations_and_update_ui: НЕТ ДАННЫХ в table_data_main. Расчеты невозможны.\n");
        // Очистка таблиц в UI окна L1
        if (data->level1_store) gtk_list_store_clear(data->level1_store);
        if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
        if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
        // Перерисовка пустых графиков
        if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
        if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
         debug_print_l1("level1_perform_calculations_and_update_ui: UI уровня 1 очищено.\n");
        return;
    }
    debug_print_l1("level1_perform_calculations_and_update_ui: Исходные данные: %d строк, %d колонок.\n",
                 data->table_data_main->row_count, data->table_data_main->column_count);

    // 3. Выполнение расчетов μ и α (из calculations.c)
    data->mu_data = compute_mu(data->table_data_main->table, data->table_data_main->row_count, data->table_data_main->column_count);
    if (data->mu_data) {
        debug_print_l1(" - compute_mu выполнен. mu_data[0] = %.4f (если есть)\n", (data->table_data_main->row_count > 0) ? data->mu_data[0] : NAN);
        data->alpha_data = compute_alpha(data->table_data_main->table, data->table_data_main->row_count,
                                       data->table_data_main->column_count, data->mu_data);
        if (data->alpha_data) {
            data->alpha_count = data->table_data_main->row_count;
            debug_print_l1(" - compute_alpha выполнен. alpha_count = %d. alpha[0]=%.2E, alpha[1]=%.2E (если есть)\n",
                         data->alpha_count,
                         (data->alpha_count > 0) ? data->alpha_data[0] : NAN,
                         (data->alpha_count > 1) ? data->alpha_data[1] : NAN);
        } else {
             debug_print_l1(" - compute_alpha вернул NULL.\n");
             data->alpha_count = 0;
        }
    } else {
         debug_print_l1(" - compute_mu вернул NULL. Расчет alpha пропущен.\n");
          data->alpha_count = 0;
    }


    // 4. Заполнение основной таблицы Уровня 1 (level1_store)
    if (data->level1_store) {
        debug_print_l1(" - Заполнение level1_store (основная таблица L1)...");
        gtk_list_store_clear(data->level1_store);
        int store_col_count = gtk_tree_model_get_n_columns(GTK_TREE_MODEL(data->level1_store));
        for (int i = 0; i < data->table_data_main->row_count; i++) {
            GtkTreeIter iter;
            gtk_list_store_append(data->level1_store, &iter);
            // Устанавливаем ID/Эпоху
            gtk_list_store_set(data->level1_store, &iter, 0, data->table_data_main->epochs ? data->table_data_main->epochs[i] : i, -1);
             // Устанавливаем данные X1, X2...
            for (int j = 0; j < data->table_data_main->column_count; j++) {
                // Проверяем, есть ли столбец в модели (защита)
                if ((j + 1) < store_col_count) {
                     // Проверка на NULL строку table_data_main->table[i]
                     double val_to_set = (data->table_data_main->table && data->table_data_main->table[i]) ? data->table_data_main->table[i][j] : NAN;
                     gtk_list_store_set(data->level1_store, &iter, j + 1, val_to_set, -1);
                } else { debug_print_l1(" ! Ошибка индекса при заполнении level1_store (данные) j+1=%d, n_cols=%d\n", j+1, store_col_count); break; }
            }
             // Устанавливаем μ
             int mu_model_idx = data->table_data_main->column_count + 1;
             if (mu_model_idx < store_col_count) {
                gtk_list_store_set(data->level1_store, &iter, mu_model_idx, (data->mu_data && i < data->table_data_main->row_count) ? data->mu_data[i] : NAN, -1);
             } else { debug_print_l1(" ! Ошибка индекса при заполнении level1_store (mu) idx=%d, n_cols=%d\n", mu_model_idx, store_col_count); }
             // Устанавливаем α
            int alpha_model_idx = data->table_data_main->column_count + 2;
             if (alpha_model_idx < store_col_count) {
                 gtk_list_store_set(data->level1_store, &iter, alpha_model_idx, (data->alpha_data && i < data->alpha_count) ? data->alpha_data[i] : NAN, -1);
            } else { debug_print_l1(" ! Ошибка индекса при заполнении level1_store (alpha) idx=%d, n_cols=%d\n", alpha_model_idx, store_col_count); }

        }
        debug_print_l1(" level1_store заполнен (%d строк).\n", gtk_tree_model_iter_n_children(GTK_TREE_MODEL(data->level1_store), NULL));
    } else { debug_print_l1(" ! level1_store не существует, пропуск заполнения.\n"); }

    // 5. Выполнение расчетов сглаживания
    double coefficients[] = {0.1, 0.4, 0.7, 0.9};
    int coeff_count = sizeof(coefficients) / sizeof(coefficients[0]);

    // Сглаживание μ
    if (data->level1_smoothing_store && data->mu_data && data->table_data_main->row_count > 0) {
        debug_print_l1(" - Запуск compute_smoothing для mu...");
        data->smoothing_data = compute_smoothing(data->mu_data, data->table_data_main->row_count, // Используем все mu_data
                                               &data->smoothing_row_count, coefficients, coeff_count);
        if (data->smoothing_data && data->smoothing_row_count > 0) {
             debug_print_l1(" Успешно (%d строк).\n", data->smoothing_row_count);
            // Заполняем таблицу сглаживания μ в UI
            gtk_list_store_clear(data->level1_smoothing_store);
             int sm_mu_store_cols = gtk_tree_model_get_n_columns(GTK_TREE_MODEL(data->level1_smoothing_store));
             if (sm_mu_store_cols == 6) { // Ожидаем M + 4xA + Real
                for (int i = 0; i < data->smoothing_row_count; i++) { // i это M=0..N
                    GtkTreeIter iter;
                    gtk_list_store_append(data->level1_smoothing_store, &iter);
                    gtk_list_store_set(data->level1_smoothing_store, &iter,
                                      0, i, // M
                                      1, (data->smoothing_data[i]) ? data->smoothing_data[i][0] : NAN, // A=0.1
                                      2, (data->smoothing_data[i]) ? data->smoothing_data[i][1] : NAN, // A=0.4
                                      3, (data->smoothing_data[i]) ? data->smoothing_data[i][2] : NAN, // A=0.7
                                      4, (data->smoothing_data[i]) ? data->smoothing_data[i][3] : NAN, // A=0.9
                                      5, (i < data->table_data_main->row_count) ? data->mu_data[i] : NAN, // Реальное μ (до N-1)
                                      -1);
                }
                debug_print_l1("   - level1_smoothing_store (mu) заполнен (%d строк).\n", data->smoothing_row_count);
             } else { debug_print_l1("   ! Неверное кол-во колонок в level1_smoothing_store: %d != 6\n", sm_mu_store_cols);}
        } else {
             debug_print_l1(" compute_smoothing для mu вернул NULL или 0 строк.\n");
              if (data->level1_smoothing_store) gtk_list_store_clear(data->level1_smoothing_store);
        }
    } else { debug_print_l1(" - Пропуск сглаживания mu (нет store, данных mu или >0 строк в main).\n"); }

    // Сглаживание α
    // Важно: сглаживаем alpha_data НАЧИНАЯ С ИНДЕКСА 1 (т.к. alpha_data[0]=0)
    // Количество данных для сглаживания: alpha_count - 1
    if (data->level1_alpha_smoothing_store && data->alpha_data && data->alpha_count > 1) {
         debug_print_l1(" - Запуск compute_smoothing для alpha (данных: %d)...", data->alpha_count - 1);
        data->smoothing_alpha_data = compute_smoothing(data->alpha_data + 1, // Указатель на второй элемент
                                                       data->alpha_count - 1, // Количество элементов
                                                       &data->smoothing_alpha_row_count,
                                                       coefficients, coeff_count);
         if (data->smoothing_alpha_data && data->smoothing_alpha_row_count > 0) {
            debug_print_l1(" Успешно (%d строк).\n", data->smoothing_alpha_row_count);
            // Заполняем таблицу сглаживания α в UI
             gtk_list_store_clear(data->level1_alpha_smoothing_store);
             int sm_a_store_cols = gtk_tree_model_get_n_columns(GTK_TREE_MODEL(data->level1_alpha_smoothing_store));
             if(sm_a_store_cols == 6) {
                 for (int i = 0; i < data->smoothing_alpha_row_count; i++) { // i это M=0..N-1 (где N - число alpha для сглаживания)
                    GtkTreeIter iter;
                    gtk_list_store_append(data->level1_alpha_smoothing_store, &iter);
                     // Реальное α для M=i соответствует alpha_data[i+1]
                     gtk_list_store_set(data->level1_alpha_smoothing_store, &iter,
                                   0, i, // M
                                   1, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][0] : NAN,
                                   2, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][1] : NAN,
                                   3, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][2] : NAN,
                                   4, (data->smoothing_alpha_data[i]) ? data->smoothing_alpha_data[i][3] : NAN,
                                   5, (i + 1 < data->alpha_count) ? data->alpha_data[i + 1] : NAN, // Реальное α[i+1]
                                   -1);
                }
                debug_print_l1("   - level1_alpha_smoothing_store заполнен (%d строк).\n", data->smoothing_alpha_row_count);
             } else { debug_print_l1("   ! Неверное кол-во колонок в level1_alpha_smoothing_store: %d != 6\n", sm_a_store_cols);}
         } else {
            debug_print_l1(" compute_smoothing для alpha вернул NULL или 0 строк.\n");
             if (data->level1_alpha_smoothing_store) gtk_list_store_clear(data->level1_alpha_smoothing_store);
        }
    } else { debug_print_l1(" - Пропуск сглаживания alpha (нет store, данных alpha или alpha_count <= 1).\n");}

    // 6. Запись результатов в файл
    if (data->mu_data && data->alpha_data && data->table_data_main && data->table_data_main->epochs) {
         debug_print_l1(" - Запись результатов в results.txt...");
        if (!write_results_to_file("results.txt",
                                 data->mu_data, data->table_data_main->row_count,
                                 data->alpha_data, data->alpha_count,
                                 data->smoothing_data, data->smoothing_row_count,
                                 data->smoothing_alpha_data, data->smoothing_alpha_row_count,
                                 data->table_data_main->epochs, data->table_data_main->row_count)) // Передаем эпохи основной таблицы
         {
             debug_print_l1(" ОШИБКА записи в results.txt!\n");
             // Здесь МОЖНО вызвать show_message_dialog, так как это явное действие пользователя
              if(data->level1_window) { // Проверка, существует ли еще окно
                   // Вызов show_message_dialog из main.c (требует его non-static)
                   show_message_dialog(GTK_WINDOW(data->level1_window), "Ошибка записи результатов в файл results.txt");
              }
        } else {
            debug_print_l1(" Успешно.\n");
             // Сообщение об успехе лучше показать один раз при открытии окна L1
        }
    } else { debug_print_l1(" - Пропуск записи в файл (не все данные готовы).\n"); }


    // 7. Запрос перерисовки графиков
     debug_print_l1(" - Запрос перерисовки графиков...");
    if (data->level1_drawing_area) gtk_widget_queue_draw(data->level1_drawing_area);
    if (data->level1_alpha_drawing_area) gtk_widget_queue_draw(data->level1_alpha_drawing_area);
    debug_print_l1(" Завершено.\n");

    debug_print_l1("level1_perform_calculations_and_update_ui: ЗАВЕРШЕНО\n");
}


// Обработчик закрытия окна Уровня 1 (бывший on_level1_window_destroy)
// ПЕРЕНЕСЕН СЮДА и сделан статическим
static void level1_on_window_destroy(GtkWidget *widget, gpointer user_data) {
    AppData *data = user_data;
    debug_print_l1("level1_on_window_destroy: Окно уровня 1 закрывается.\n");

    // Обнуляем указатели на виджеты окна L1 в AppData
    data->level1_window = NULL; // Самый важный флаг, что окно закрыто
    data->level1_container = NULL;
    data->level1_tree_view = NULL;
    data->level1_store = NULL; // Store управляется TreeView, но обнулим указатель
    data->level1_smoothing_view = NULL;
    data->level1_smoothing_store = NULL; // обнулим указатель
    data->level1_alpha_smoothing_view = NULL;
    data->level1_alpha_smoothing_store = NULL; // обнулим указатель
    data->level1_drawing_area = NULL;
    data->level1_alpha_drawing_area = NULL;

    // Очищаем связанные ДАННЫЕ расчетов
    level1_clear_internal_data(data);
    debug_print_l1("level1_on_window_destroy: Очистка завершена.\n");
}

// === Публичные функции ===

// Реализация level1_show_window (заменяет код из on_level_clicked)
void level1_show_window(AppData *data) {
    debug_print_l1("level1_show_window: Запрошено окно Уровня 1.\n");

    // Проверки входных данных (оставляем в вызывающей функции on_level_clicked в main.c)
    // if (!data->db_opened || !data->current_main_table_name) { return; }
    // if (data->table_data_main == NULL || data->table_data_main->row_count == 0){ return; }

    // Если окно еще не создано, создаем его
    if (!data->level1_window) {
        debug_print_l1("level1_show_window: Окно не существует, создаем новое...\n");
        data->level1_window = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(data->level1_window), "Уровень 1: Анализ данных");
        gtk_window_set_default_size(GTK_WINDOW(data->level1_window), 1450, 900);
        // Устанавливаем родительское окно (важно для модальности и позиционирования)
        if(data->parent) gtk_window_set_transient_for(GTK_WINDOW(data->level1_window), data->parent);
        // Подключаем обработчик закрытия окна
        g_signal_connect(data->level1_window, "destroy", G_CALLBACK(level1_on_window_destroy), data);

        // --- Создание структуры окна L1 (как было в main.c on_level_clicked) ---
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

        // Левая колонка
        GtkWidget *left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_hexpand(left_vbox, TRUE);
        gtk_box_append(GTK_BOX(data->level1_container), left_vbox);

        GtkWidget *main_table_label = gtk_label_new("Данные основной таблицы (с μ и α)");
        gtk_widget_set_halign(main_table_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(left_vbox), main_table_label);

        GtkWidget *main_table_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_table_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(main_table_scrolled, TRUE); // Растягиваем таблицу
        gtk_widget_set_size_request(main_table_scrolled, -1, 300); // Задаем высоту
        data->level1_tree_view = gtk_tree_view_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_table_scrolled), data->level1_tree_view);
        gtk_box_append(GTK_BOX(left_vbox), main_table_scrolled);

        GtkWidget *mu_graph_label = gtk_label_new("График сглаживания μ");
        gtk_widget_set_halign(mu_graph_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(left_vbox), mu_graph_label);

        data->level1_drawing_area = gtk_drawing_area_new();
        gtk_widget_set_size_request(data->level1_drawing_area, -1, 400); // Задаем высоту графика
        gtk_widget_set_vexpand(data->level1_drawing_area, TRUE); // Растягиваем график
        // Установка функции отрисовки (статическая level1_draw_graph из этого файла)
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(left_vbox), data->level1_drawing_area);

        // Правая колонка
        GtkWidget *right_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_hexpand(right_vbox, TRUE);
        gtk_box_append(GTK_BOX(data->level1_container), right_vbox);

        GtkWidget *smoothing_mu_label = gtk_label_new("Таблица сглаживания μ");
        gtk_widget_set_halign(smoothing_mu_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(right_vbox), smoothing_mu_label);
        GtkWidget *smoothing_mu_scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(smoothing_mu_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(smoothing_mu_scrolled, -1, 150); // Маленькая высота для таблиц сглаживания
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
        gtk_widget_set_size_request(data->level1_alpha_drawing_area, -1, 400); // Та же высота, что и у графика mu
        gtk_widget_set_vexpand(data->level1_alpha_drawing_area, TRUE); // Растягиваем
        // Установка функции отрисовки (та же level1_draw_graph, она определяет тип по виджету)
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->level1_alpha_drawing_area), level1_draw_graph, data, NULL);
        gtk_box_append(GTK_BOX(right_vbox), data->level1_alpha_drawing_area);

        // --- Создание колонок для TreeView ---
        // Вызываем update_tree_view_columns ИЗ main.c (пока предполагаем доступность)
        debug_print_l1("   Вызов update_tree_view_columns для L1_MAIN...");
        update_tree_view_columns(data, TRUE, FALSE, FALSE); // level1_tree_view (main + mu + alpha)
        debug_print_l1("   Вызов update_tree_view_columns для L1_SM_MU...");
        update_tree_view_columns(data, FALSE, TRUE, FALSE); // level1_smoothing_view (mu)
         debug_print_l1("   Вызов update_tree_view_columns для L1_SM_ALPHA...");
        update_tree_view_columns(data, FALSE, FALSE, TRUE); // level1_alpha_smoothing_view (alpha)
         debug_print_l1("   Структура окна L1 создана.");

         // При первом создании сразу выполняем расчеты и заполняем
        level1_perform_calculations_and_update_ui(data);
        // Показываем сообщение (можно убрать, если не нужно при каждом открытии)
         show_message_dialog(GTK_WINDOW(data->level1_window),
                           "Расчеты Уровня 1 выполнены. Результаты также в results.txt.");


    } else {
        // Окно уже существует, просто обновим его содержимое, если нужно
        debug_print_l1("level1_show_window: Окно уже существует. Обновление содержимого...\n");
        // Повторно выполняем расчеты и обновляем UI (таблицы/графики)
        level1_perform_calculations_and_update_ui(data);
    }


    // Делаем окно видимым (или поднимаем поверх, если уже было видимо)
    // Используем gtk_window_present для поднятия окна, если оно уже есть
    gtk_window_present(GTK_WINDOW(data->level1_window));
    debug_print_l1("level1_show_window: Окно показано/поднято.\n");
}


// Реализация level1_update_content_if_visible
void level1_update_content_if_visible(AppData *data) {
    // Проверяем, существует ли объект окна И видимо ли оно
    if (data && data->level1_window && gtk_widget_get_visible(data->level1_window)) {
        debug_print_l1("level1_update_content_if_visible: Окно L1 видимо, запускаем обновление...\n");
        level1_perform_calculations_and_update_ui(data);
    } else {
       // debug_print_l1("level1_update_content_if_visible: Окно L1 не существует или не видимо, обновление пропущено.\n");
    }
}

// Реализация level1_cleanup_calculated_data
void level1_cleanup_calculated_data(AppData *data) {
    if (data) {
        level1_clear_internal_data(data);
    }
}