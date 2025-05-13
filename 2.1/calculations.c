#include "calculations.h"
#include <math.h>   // Для sqrt, acos, M_PI (если нужен)
#include <stdio.h>
#include <stdlib.h> // Для g_new, g_free (хотя это из glib.h, но часто включают)
#include <stdarg.h> // Для va_list
#include <glib.h>   // Для g_new, g_free, g_strdup_printf

// Логирование (аналогично другим файлам, для самодостаточности)
static FILE *debug_log_calc = NULL;

static void init_calc_debug_log() {
    if (!debug_log_calc) {
        debug_log_calc = fopen("debug.log", "a");
        if (!debug_log_calc) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (calculations.c)\n");
        } else {
            fseek(debug_log_calc, 0, SEEK_END);
            if (ftell(debug_log_calc) == 0) {
                 fprintf(debug_log_calc, "\xEF\xBB\xBF");
            }
            fprintf(debug_log_calc, "Лог-файл открыт/доступен (calculations.c)\n");
            fflush(debug_log_calc);
        }
    }
}

static void debug_print_calc(const char *format, ...) {
    init_calc_debug_log();
    if (debug_log_calc) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_calc, format, args);
        va_end(args);
        fflush(debug_log_calc);
    }
}


double *compute_mu(double **table_data, int row_count, int column_count) {
    if (!table_data || row_count == 0) { // column_count может быть 0, тогда mu будет 0
        debug_print_calc("compute_mu: Входные данные невалидны (нет таблицы или строк).\n");
        return NULL;
    }
    if (column_count == 0) {
        debug_print_calc("compute_mu: Количество колонок данных равно 0. Все μ будут 0.\n");
        // Создадим массив нулей, если строки есть, но колонок данных нет.
        double *mu_zeros = g_new0(double, row_count);
        return mu_zeros;
    }


    double *mu_values = g_new(double, row_count);
    debug_print_calc("compute_mu: Начало вычисления μ для %d строк, %d колонок.\n", row_count, column_count);

    for (int i = 0; i < row_count; i++) {
        if (table_data[i] == NULL && column_count > 0) { // Проверка на случай, если строка не аллоцирована
             debug_print_calc("compute_mu: Ошибка, table_data[%d] == NULL. Устанавливаю mu[%d] = 0.\n", i, i);
             mu_values[i] = 0.0;
             continue;
        }
        double sum_of_squares = 0.0;
        for (int j = 0; j < column_count; j++) {
            sum_of_squares += table_data[i][j] * table_data[i][j];
        }
        mu_values[i] = sqrt(sum_of_squares);
        // debug_print_calc("Строка %d: μ = %.4f\n", i, mu_values[i]); // Слишком много логов
    }

    debug_print_calc("compute_mu: Вычислено %d значений μ.\n", row_count);
    return mu_values;
}

double *compute_alpha(double **table_data, int row_count, int column_count, double *mu_values) {
    if (!table_data || !mu_values || row_count == 0) { // column_count может быть 0
        debug_print_calc("compute_alpha: Входные данные невалидны (нет таблицы, μ или строк).\n");
        return NULL;
    }
     if (column_count == 0) {
        debug_print_calc("compute_alpha: Количество колонок данных равно 0. Все α будут 0.\n");
        double *alpha_zeros = g_new0(double, row_count); // alpha[0] = 0, остальные тоже
        return alpha_zeros;
    }
    if (row_count < 1) { // Хотя бы одна строка для alpha[0]
        return g_new0(double, 0); // Пустой массив
    }


    double *alpha_values = g_new(double, row_count);
    debug_print_calc("compute_alpha: Начало вычисления α для %d строк, %d колонок.\n", row_count, column_count);

    alpha_values[0] = 0.0; // По определению, α для первой эпохи (с собой) = 0
    // debug_print_calc("Строка 0: α = %.4E (определено как 0)\n", alpha_values[0]);

    if (row_count > 0 && (table_data[0] == NULL && column_count > 0)) {
        debug_print_calc("compute_alpha: Ошибка, table_data[0] == NULL. Невозможно вычислить α.\n");
        for(int i=1; i < row_count; ++i) alpha_values[i] = 0.0; // Или NAN
        return alpha_values;
    }


    for (int i = 1; i < row_count; i++) {
        if (table_data[i] == NULL && column_count > 0) {
             debug_print_calc("compute_alpha: Ошибка, table_data[%d] == NULL. Устанавливаю alpha[%d] = 0.\n", i, i);
             alpha_values[i] = 0.0;
             continue;
        }

        double dot_product = 0.0;
        for (int j = 0; j < column_count; j++) {
            dot_product += table_data[0][j] * table_data[i][j];
        }

        double denominator = mu_values[0] * mu_values[i];
        if (denominator == 0.0) {
            // debug_print_calc("Строка %d: Предупреждение! Знаменатель (μ0*μi) = 0. α устанавливается в 0.\n", i);
            alpha_values[i] = 0.0; // Или M_PI/2 если считать ортогональными? Но 0 безопаснее.
                                  // Если один из векторов нулевой, угол не определен.
                                  // Если оба ненулевые, но их произведение μ=0, это странно.
                                  // μ это длина, она >=0. Если μ=0, то вектор нулевой.
        } else {
            double cos_alpha_val = dot_product / denominator;
            // Коррекция из-за ошибок округления
            if (cos_alpha_val > 1.0) cos_alpha_val = 1.0;
            if (cos_alpha_val < -1.0) cos_alpha_val = -1.0;
            
            alpha_values[i] = acos(cos_alpha_val);
            // debug_print_calc("Строка %d: dot=%.4f, den=%.4f, cos_α=%.4f, α = %.4E\n",
            //                 i, dot_product, denominator, cos_alpha_val, alpha_values[i]);
        }
    }

    debug_print_calc("compute_alpha: Вычислено %d значений α.\n", row_count);
    return alpha_values;
}

double **compute_smoothing(double *input_data, int data_count, int *out_smoothing_row_count, 
                           double *smoothing_coeffs, int num_coeffs) {
    if (!input_data || data_count == 0) {
        debug_print_calc("compute_smoothing: Нет входных данных для сглаживания.\n");
        *out_smoothing_row_count = 0;
        return NULL;
    }
    if (!smoothing_coeffs || num_coeffs == 0) {
        debug_print_calc("compute_smoothing: Нет коэффициентов сглаживания.\n");
        *out_smoothing_row_count = 0;
        return NULL;
    }

    // Количество строк в таблице сглаживания = N (исходные) + 1 (прогноз)
    *out_smoothing_row_count = data_count + 1; 
    debug_print_calc("compute_smoothing: Начало. Входных данных: %d. Коэффициентов: %d. Строк результата: %d\n",
                data_count, num_coeffs, *out_smoothing_row_count);


    double **smoothed_table = g_new(double *, *out_smoothing_row_count);
    for (int i = 0; i < *out_smoothing_row_count; i++) {
        smoothed_table[i] = g_new0(double, num_coeffs); // Инициализация нулями
    }

    // S_0 (для M=0 в таблице сглаживания)
    // Оригинальная формула: S_0 = Y_1 * A + (1-A) * Y_avg (где Y_1 это input_data[0])
    // Если Y_avg не задан, часто S_0 = Y_1.
    // В коде используется Y_avg. Рассчитаем его.
    double data_sum_for_avg = 0.0;
    for (int i = 0; i < data_count; i++) {
        data_sum_for_avg += input_data[i];
    }
    double data_average = (data_count > 0) ? (data_sum_for_avg / data_count) : input_data[0]; // Защита от деления на 0
    debug_print_calc("Среднее значение входных данных для S_0: %.4f\n", data_average);


    // Строка M=0 в таблице сглаживания (соответствует S_0 прогноз для Y_1, используя Y_1)
    for (int j = 0; j < num_coeffs; j++) { // j - индекс коэффициента
        smoothed_table[0][j] = input_data[0] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * data_average;
       // debug_print_calc("M=0, A=%.2f: Y_0=%.4f, S_0=%.4f\n", smoothing_coeffs[j], input_data[0], smoothed_table[0][j]);
    }

    // Строки M=1 до M=data_count-1 (соответствуют S_1 до S_{data_count-1})
    // S_t = Y_t * A + (1-A) * S_{t-1}
    // В нашей таблице smoothed_table[i] соответствует S_i
    // input_data[i] соответствует Y_i
    for (int i = 1; i < data_count; i++) { // i - индекс M в таблице сглаживания, также индекс Y в input_data
        for (int j = 0; j < num_coeffs; j++) { // j - индекс коэффициента
            smoothed_table[i][j] = input_data[i] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * smoothed_table[i-1][j];
            // debug_print_calc("M=%d, A=%.2f: Y_%d=%.4f, S_prev=%.4f, S_curr=%.4f\n",
            //                 i, smoothing_coeffs[j], i, input_data[i], smoothed_table[i-1][j], smoothed_table[i][j]);
        }
    }

    // Строка M=data_count (прогнозная S_N на момент N+1, используя S_{N-1})
    // S_{N, N+1} = Y_N * A + (1-A) * S_{N-1}  - это smoothed_table[data_count-1][j] по идее уже содержит прогноз Y_N
    // Формула в коде: smoothed_table[N][j] = column_avg * A + (1-A) * S_{N-1} [j]
    // Это более сложный прогноз, использующий среднее по сглаженному ряду.
    // S_{N-1} это smoothed_table[data_count-1][j]
    int last_data_idx_m = data_count -1; // Индекс последнего M, для которого есть Y. M_N-1 (0-based)

    for (int j = 0; j < num_coeffs; j++) {
         // Прогноз на один шаг вперед (M = data_count, то есть N+1-й элемент)
         // Используется S_t = Y_t * A + (1-A)S_{t-1}
         // Прогноз для Y_{t+1} в момент t это S_t.
         // Значит, smoothed_table[data_count-1][j] это прогноз для Y_{data_count} (N-й точки, 0-based)
         // А нам нужен прогноз для Y_{data_count+1} (N+1 точки)
         // Прогноз_на_M+1 = S_M = α * Y_M + (1-α) * S_M-1.
         // То есть smoothed_table[data_count-1][j] уже является прогнозом на следующий период (data_count).
         // Мы его и записываем в M = data_count
        if (data_count > 0) {
             // Простой прогноз: следующий равен последнему сглаженному значению.
             // S_prognoz = S_{N-1} (где N-1 это последний индекс данных)
             smoothed_table[data_count][j] = smoothed_table[data_count -1][j];
        } else { // data_count == 0, но мы это отсекли ранее. Если бы было 1 значение data
            smoothed_table[data_count][j] = smoothed_table[0][j]; // Если только одно значение, то прогноз такой же
        }

       // debug_print_calc("M=%d (прогноз), A=%.2f: S_last=%.4f, S_prognoz=%.4f\n",
       //                  data_count, smoothing_coeffs[j], (data_count > 0 ? smoothed_table[data_count-1][j] : smoothed_table[0][j]), smoothed_table[data_count][j]);
    }


    debug_print_calc("compute_smoothing: Завершено. Таблица сглаживания: %d строк, %d столбцов.\n",
                *out_smoothing_row_count, num_coeffs);
    return smoothed_table;
}


gboolean write_results_to_file(const char *filename,
                               double *mu_data, int mu_data_count,
                               double *alpha_data, int alpha_data_count,
                               double **smoothing_mu_table, int smoothing_mu_rows,
                               double **smoothing_alpha_table, int smoothing_alpha_rows,
                               int *epoch_values, int epoch_count) {
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        debug_print_calc("write_results_to_file: Не удалось открыть файл '%s' для записи.\n", filename);
        return FALSE;
    }
    debug_print_calc("write_results_to_file: Запись результатов в файл '%s'\n", filename);

    fprintf(out_file, "\xEF\xBB\xBF"); // UTF-8 BOM

    // Таблица Эпоха, μ, α
    fprintf(out_file, "Эпоха\tμ (мю)\tα (альфа)\n");
    int max_rows_initial = mu_data_count > alpha_data_count ? mu_data_count : alpha_data_count;
    if(epoch_count > max_rows_initial) max_rows_initial = epoch_count;

    for (int i = 0; i < max_rows_initial; i++) {
        fprintf(out_file, "%d\t", (i < epoch_count) ? epoch_values[i] : i + 1); // Эпоха или номер строки
        if (mu_data && i < mu_data_count) fprintf(out_file, "%.4f\t", mu_data[i]);
        else fprintf(out_file, "N/A\t");
        if (alpha_data && i < alpha_data_count) fprintf(out_file, "%.4E\n", alpha_data[i]);
        else fprintf(out_file, "N/A\n");
    }

    // Таблица сглаживания μ
    if (smoothing_mu_table && smoothing_mu_rows > 0) {
        fprintf(out_file, "\nТаблица сглаживания μ (мю)\n");
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tμ (реальное)\n");
        for (int i = 0; i < smoothing_mu_rows; i++) {
            fprintf(out_file, "%d\t", i); // M = 0, 1, ..., n, n+1(прогноз)
            fprintf(out_file, "%.4f\t%.4f\t%.4f\t%.4f\t",
                    smoothing_mu_table[i][0], smoothing_mu_table[i][1],
                    smoothing_mu_table[i][2], smoothing_mu_table[i][3]);
            if (mu_data && i < mu_data_count) { // Реальное μ для M = 0..n-1
                fprintf(out_file, "%.4f\n", mu_data[i]);
            } else if (i == mu_data_count && i == smoothing_mu_rows -1) { // Для прогнозной строки М=n (N+1)
                 fprintf(out_file, "(прогноз)\n");
            }
             else {
                fprintf(out_file, "N/A\n");
            }
        }
    } else {
         debug_print_calc("Данные для сглаживания μ отсутствуют, таблица не будет записана.\n");
    }

    // Таблица сглаживания α
    // Для α, данные начинаются с alpha_data[1]. В smoothing_alpha_table передаются данные без alpha_data[0].
    // Поэтому, реальные α значения это alpha_data[i+1] для M=i.
    if (smoothing_alpha_table && smoothing_alpha_rows > 0) {
        fprintf(out_file, "\nТаблица сглаживания α (альфа)\n");
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tα (реальное)\n");
        for (int i = 0; i < smoothing_alpha_rows; i++) { // i это M = 0, 1, ... , (alpha_count-1)-1, (alpha_count-1) (прогноз)
            fprintf(out_file, "%d\t", i);
            fprintf(out_file, "%.4E\t%.4E\t%.4E\t%.4E\t", // Формат E для α
                    smoothing_alpha_table[i][0], smoothing_alpha_table[i][1],
                    smoothing_alpha_table[i][2], smoothing_alpha_table[i][3]);
            // alpha_data имеет alpha_count элементов. Первый (alpha_data[0]) равен 0.
            // Сглаживаются alpha_count-1 элементов (начиная с alpha_data[1]).
            // Значит, i-й элемент сглаживания (M=i) соответствует alpha_data[i+1]
            if (alpha_data && (i + 1) < alpha_data_count) { // Реальное α для M = 0 .. (alpha_count-2)
                fprintf(out_file, "%.4E\n", alpha_data[i + 1]);
            } else if ( i == (alpha_data_count -1) && i == smoothing_alpha_rows -1 ) { // Для прогнозной строки
                fprintf(out_file, "(прогноз)\n");
            }
            else {
                fprintf(out_file, "N/A\n");
            }
        }
    } else {
        debug_print_calc("Данные для сглаживания α отсутствуют, таблица не будет записана.\n");
    }

    fclose(out_file);
    debug_print_calc("write_results_to_file: Результаты успешно записаны в '%s'\n", filename);
    return TRUE;
}