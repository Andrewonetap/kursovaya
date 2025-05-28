#include "calculations_rl.h"
#include <math.h>   // Для sqrt, acos, fabs, NAN, isinf, M_PI (если нужен)
#include <stdio.h>
#include <stdlib.h> // Для malloc, calloc, free, strtod
#include <string.h> // Для strcpy, strcat, sprintf (если понадобятся, но в write_results_to_file)
#include <stdarg.h> // Для va_list

// Логгирование (аналогично другим файлам)
static FILE *debug_log_calc_rl = NULL;

static void init_calc_debug_log_rl() {
    if (!debug_log_calc_rl) {
        debug_log_calc_rl = fopen("debug.log", "a");
        if (!debug_log_calc_rl) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (calculations_rl.c)\n");
        } else {
            fseek(debug_log_calc_rl, 0, SEEK_END);
            if (ftell(debug_log_calc_rl) == 0) {
                 fprintf(debug_log_calc_rl, "\xEF\xBB\xBF"); // UTF-8 BOM
            }
            // fprintf(debug_log_calc_rl, "Лог-файл открыт/доступен (calculations_rl.c)\n");
            fflush(debug_log_calc_rl);
        }
    }
}

static void debug_print_calc_rl(const char *format, ...) {
    init_calc_debug_log_rl();
    if (debug_log_calc_rl) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_calc_rl, format, args);
        va_end(args);
        fflush(debug_log_calc_rl);
    }
}

// --- Реализация функций ---

double *compute_mu_rl(double **table_data, int row_count, int column_count) {
    if (!table_data || row_count <= 0) {
        debug_print_calc_rl("compute_mu_rl: Входные данные невалидны (нет таблицы или строк <= 0).\n");
        return NULL;
    }
    // column_count может быть 0, тогда mu будет 0 для каждой строки

    double *mu_values = (double*)calloc(row_count, sizeof(double));
    if (!mu_values) {
        debug_print_calc_rl("compute_mu_rl: Ошибка выделения памяти для mu_values.\n");
        return NULL;
    }
    debug_print_calc_rl("compute_mu_rl: Начало вычисления μ для %d строк, %d колонок.\n", row_count, column_count);

    for (int i = 0; i < row_count; i++) {
        if (column_count > 0 && table_data[i] == NULL) { // Проверка на случай, если строка не аллоцирована, но колонки должны быть
             debug_print_calc_rl("compute_mu_rl: Ошибка, table_data[%d] == NULL, а column_count > 0. Устанавливаю mu[%d] = 0.\n", i, i);
             mu_values[i] = 0.0; // Уже 0 из-за calloc, но для ясности
             continue;
        }
        if (column_count == 0) {
            mu_values[i] = 0.0; // Если нет колонок данных, mu = 0
            continue;
        }

        double sum_of_squares = 0.0;
        for (int j = 0; j < column_count; j++) {
            sum_of_squares += table_data[i][j] * table_data[i][j];
        }
        mu_values[i] = sqrt(sum_of_squares);
    }

    debug_print_calc_rl("compute_mu_rl: Вычислено %d значений μ.\n", row_count);
    return mu_values;
}

void close_calc_debug_log_rl(void) {
    if (debug_log_calc_rl) {
        // Можно добавить fprintf перед закрытием, если нужно
        fprintf(debug_log_calc_rl, "Лог-файл закрывается (calculations_rl.c)\n");
        fflush(debug_log_calc_rl); // Убедимся, что все записано
        fclose(debug_log_calc_rl);
        debug_log_calc_rl = NULL;
    }
}

double *compute_alpha_rl(double **table_data, int row_count, int column_count, double *mu_values) {
    if (!table_data || !mu_values || row_count <= 0) {
        debug_print_calc_rl("compute_alpha_rl: Входные данные невалидны (нет таблицы, μ или строк <= 0).\n");
        return NULL;
    }
    // column_count может быть 0

    double *alpha_values = (double*)calloc(row_count, sizeof(double)); // calloc обнулит, alpha_values[0] будет 0.0
    if (!alpha_values) {
        debug_print_calc_rl("compute_alpha_rl: Ошибка выделения памяти для alpha_values.\n");
        return NULL;
    }
    debug_print_calc_rl("compute_alpha_rl: Начало вычисления α для %d строк, %d колонок.\n", row_count, column_count);

    // alpha_values[0] = 0.0; // Уже 0 из-за calloc

    if (column_count == 0) { // Если нет колонок данных, все alpha (кроме alpha[0]) не определены или можно считать 0
        debug_print_calc_rl("compute_alpha_rl: Количество колонок данных равно 0. Все α будут 0.\n");
        // alpha_values уже заполнен нулями
        return alpha_values;
    }
    
    if (table_data[0] == NULL) { // Нужна хотя бы первая строка для вычисления остальных alpha
        debug_print_calc_rl("compute_alpha_rl: Ошибка, table_data[0] == NULL. Невозможно вычислить α.\n");
        // alpha_values уже заполнен нулями, что является приемлемым значением по умолчанию
        return alpha_values;
    }

    for (int i = 1; i < row_count; i++) { // Начинаем с i=1, так как alpha[0] = 0
        if (table_data[i] == NULL) {
             debug_print_calc_rl("compute_alpha_rl: Ошибка, table_data[%d] == NULL. Устанавливаю alpha[%d] = 0.\n", i, i);
             alpha_values[i] = 0.0; // Уже 0, но для ясности
             continue;
        }

        double dot_product = 0.0;
        for (int j = 0; j < column_count; j++) {
            dot_product += table_data[0][j] * table_data[i][j];
        }

        double denominator = mu_values[0] * mu_values[i];
        if (fabs(denominator) < 1e-9) { // Проверка на почти нулевой знаменатель
            alpha_values[i] = 0.0; // Если один из векторов нулевой, или они ортогональны и μ=0 (что странно для длины)
                                   // Угол не определен или равен Pi/2. 0 - безопасное значение.
        } else {
            double cos_alpha_val = dot_product / denominator;
            if (cos_alpha_val > 1.0) cos_alpha_val = 1.0;
            if (cos_alpha_val < -1.0) cos_alpha_val = -1.0;
            alpha_values[i] = acos(cos_alpha_val);
        }
    }

    debug_print_calc_rl("compute_alpha_rl: Вычислено %d значений α.\n", row_count);
    return alpha_values;
}

double **compute_smoothing_rl(double *input_data, int data_count, int *out_smoothing_row_count,
                              double *smoothing_coeffs, int num_coeffs) {
    if (!input_data || data_count <= 0) {
        debug_print_calc_rl("compute_smoothing_rl: Нет входных данных для сглаживания (data_count <= 0).\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }
    if (!smoothing_coeffs || num_coeffs <= 0) {
        debug_print_calc_rl("compute_smoothing_rl: Нет коэффициентов сглаживания (num_coeffs <= 0).\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }

    int result_rows = data_count + 1; // N исходных + 1 прогноз
    if(out_smoothing_row_count) *out_smoothing_row_count = result_rows;
    debug_print_calc_rl("compute_smoothing_rl: Начало. Входных данных: %d. Коэффициентов: %d. Строк результата: %d\n",
                data_count, num_coeffs, result_rows);

    double **smoothed_table = (double**)malloc(result_rows * sizeof(double*));
    if (!smoothed_table) {
        debug_print_calc_rl("compute_smoothing_rl: Ошибка malloc для smoothed_table.\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }
    for (int i = 0; i < result_rows; i++) {
        smoothed_table[i] = (double*)calloc(num_coeffs, sizeof(double)); // calloc обнуляет
        if (!smoothed_table[i]) {
            debug_print_calc_rl("compute_smoothing_rl: Ошибка calloc для smoothed_table[%d].\n", i);
            for(int k=0; k<i; ++k) free(smoothed_table[k]);
            free(smoothed_table);
            if(out_smoothing_row_count) *out_smoothing_row_count = 0;
            return NULL;
        }
    }

    double data_sum_for_avg = 0.0;
    for (int i = 0; i < data_count; i++) {
        data_sum_for_avg += input_data[i];
    }
    double data_average = (data_count > 0) ? (data_sum_for_avg / data_count) : input_data[0]; // Защита от деления на 0
    // Если data_count=0, мы бы вышли раньше. Если data_count=1, avg = input_data[0].

    // Строка M=0 в таблице сглаживания (S_0)
    for (int j = 0; j < num_coeffs; j++) { // j - индекс коэффициента
        smoothed_table[0][j] = input_data[0] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * data_average;
    }

    // Строки M=1 до M=data_count-1 (S_1 до S_{N-1})
    for (int i = 1; i < data_count; i++) { // i - индекс M в таблице сглаживания, также индекс Y в input_data
        for (int j = 0; j < num_coeffs; j++) { // j - индекс коэффициента
            smoothed_table[i][j] = input_data[i] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * smoothed_table[i-1][j];
        }
    }

    // Строка M=data_count (прогнозная S_N на момент N+1)
    // Прогноз на один шаг вперед: S_prognoz(N+1) = S_N (последнее сглаженное значение)
    // S_N соответствует smoothed_table[data_count-1]
    if (data_count > 0) { // data_count здесь это N (количество точек)
        for (int j = 0; j < num_coeffs; j++) {
            smoothed_table[data_count][j] = smoothed_table[data_count - 1][j];
        }
    } else { // data_count == 0, но мы это отсекли ранее. Этот блок не должен выполниться.
             // Если бы data_count был 1, то data_count-1 = 0.
             // smoothed_table[1][j] = smoothed_table[0][j]
        for (int j = 0; j < num_coeffs; j++) {
             smoothed_table[data_count][j] = smoothed_table[0][j];
        }
    }

    debug_print_calc_rl("compute_smoothing_rl: Завершено. Таблица сглаживания: %d строк, %d столбцов.\n",
                result_rows, num_coeffs);
    return smoothed_table;
}

bool write_results_to_file_rl(const char *filename,
                              double *mu_data, int mu_data_count,
                              double *alpha_data, int alpha_data_count,
                              double **smoothing_mu_table, int smoothing_mu_rows,
                              double **smoothing_alpha_table, int smoothing_alpha_rows,
                              int *epoch_values, int epoch_count) {
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        debug_print_calc_rl("write_results_to_file_rl: Не удалось открыть файл '%s' для записи.\n", filename);
        return false;
    }
    debug_print_calc_rl("write_results_to_file_rl: Запись результатов в файл '%s'\n", filename);

    fprintf(out_file, "\xEF\xBB\xBF"); // UTF-8 BOM

    fprintf(out_file, "Эпоха\tμ (мю)\tα (альфа)\n");
    int max_rows_initial = 0;
    if (mu_data_count > max_rows_initial) max_rows_initial = mu_data_count;
    if (alpha_data_count > max_rows_initial) max_rows_initial = alpha_data_count;
    if (epoch_count > max_rows_initial) max_rows_initial = epoch_count;


    for (int i = 0; i < max_rows_initial; i++) {
        fprintf(out_file, "%d\t", (epoch_values && i < epoch_count) ? epoch_values[i] : i + 1);
        if (mu_data && i < mu_data_count) fprintf(out_file, "%.4f\t", mu_data[i]);
        else fprintf(out_file, "N/A\t");
        if (alpha_data && i < alpha_data_count) fprintf(out_file, "%.4E\n", alpha_data[i]);
        else fprintf(out_file, "N/A\n");
    }

    if (smoothing_mu_table && smoothing_mu_rows > 0 && mu_data_count > 0) { // mu_data_count для реальных значений
        fprintf(out_file, "\nТаблица сглаживания μ (мю)\n");
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tμ (реальное)\n");
        for (int i = 0; i < smoothing_mu_rows; i++) { // i это M = 0, 1, ..., N-1 (данные), N (прогноз)
            fprintf(out_file, "%d\t", i);
            if (smoothing_mu_table[i]) {
                 fprintf(out_file, "%.4f\t%.4f\t%.4f\t%.4f\t",
                        smoothing_mu_table[i][0], smoothing_mu_table[i][1],
                        smoothing_mu_table[i][2], smoothing_mu_table[i][3]);
            } else {
                 fprintf(out_file, "N/A\tN/A\tN/A\tN/A\t");
            }

            if (mu_data && i < mu_data_count) { // Реальное μ для M = 0..N-1
                fprintf(out_file, "%.4f\n", mu_data[i]);
            } else if (i == mu_data_count && i == smoothing_mu_rows - 1) { // Для прогнозной строки М=N
                 fprintf(out_file, "(прогноз)\n");
            } else {
                fprintf(out_file, "N/A\n");
            }
        }
    }

    // alpha_data_count - общее число альф, включая alpha[0]=0.
    // Сглаживаются alpha_data_count-1 элементов (начиная с alpha_data[1]).
    // smoothing_alpha_rows будет (alpha_data_count-1) + 1 = alpha_data_count.
    if (smoothing_alpha_table && smoothing_alpha_rows > 0 && alpha_data_count > 1) {
        fprintf(out_file, "\nТаблица сглаживания α (альфа)\n");
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tα (реальное)\n");
        for (int i = 0; i < smoothing_alpha_rows; i++) { // i это M = 0, 1, ... , (N_alpha-1)-1, (N_alpha-1) (прогноз)
                                                        // где N_alpha = alpha_data_count - 1 (количество сглаживаемых альф)
            fprintf(out_file, "%d\t", i);
            if (smoothing_alpha_table[i]) {
                fprintf(out_file, "%.4E\t%.4E\t%.4E\t%.4E\t",
                        smoothing_alpha_table[i][0], smoothing_alpha_table[i][1],
                        smoothing_alpha_table[i][2], smoothing_alpha_table[i][3]);
            } else {
                fprintf(out_file, "N/A\tN/A\tN/A\tN/A\t");
            }

            // Реальное α для M=i соответствует alpha_data[i+1]
            if (alpha_data && (i + 1) < alpha_data_count) {
                fprintf(out_file, "%.4E\n", alpha_data[i + 1]);
            } else if (i == (alpha_data_count - 1) && i == smoothing_alpha_rows - 1) { // Прогнозная строка
                fprintf(out_file, "(прогноз)\n");
            } else {
                fprintf(out_file, "N/A\n");
            }
        }
    }

    fclose(out_file);
    debug_print_calc_rl("write_results_to_file_rl: Результаты успешно записаны в '%s'\n", filename);
    return true;
}


double *calculate_new_row_rl(const TableData *table_data) {
    if (!table_data || table_data->row_count < 2 || table_data->column_count <= 0) {
        debug_print_calc_rl("calculate_new_row_rl: Недостаточно данных для вычисления (rows=%d, cols=%d)\n",
                           table_data ? table_data->row_count : -1, table_data ? table_data->column_count : -1);
        return NULL;
    }


    int rows = table_data->row_count;
    int cols = table_data->column_count;
    double *new_coordinates = (double*)malloc(cols * sizeof(double));
    if (!new_coordinates) {
        debug_print_calc_rl("calculate_new_row_rl: Ошибка malloc для new_coordinates.\n");
        return NULL;
    }

    for (int j = 0; j < cols; j++) {
        if (table_data->table[rows - 1] == NULL) { // Проверка последней строки
             debug_print_calc_rl("calculate_new_row_rl: Ошибка, последняя строка table_data->table[%d] == NULL.\n", rows-1);
             free(new_coordinates); return NULL; // Не можем продолжить, если нет последней точки
        }
        double max_delta = 0.0;
        for (int i = 0; i < rows - 1; i++) {
            if (table_data->table[i] == NULL || table_data->table[i+1] == NULL) {
                debug_print_calc_rl("calculate_new_row_rl: Пропуск расчета дельты для колонки %d из-за NULL строки в середине данных.\n", j);
                // Можно установить new_coordinates[j] в какое-то значение по умолчанию или вернуть ошибку
                // Пока что max_delta останется неизменной, что может быть неверно.
                // Для простоты, если есть NULL в середине, результат для этой колонки может быть непредсказуем.
                // Лучше обеспечить консистентность данных перед вызовом.
                continue; // Пропускаем эту пару для delta
            }
            double delta = fabs(table_data->table[i][j] - table_data->table[i + 1][j]);
            if (delta > max_delta) {
                max_delta = delta;
            }
        }
        double random_factor = (double)rand() / RAND_MAX;
        double k_val = random_factor * 2.0 * max_delta - max_delta;
        new_coordinates[j] = table_data->table[rows - 1][j] + k_val;
    }
    return new_coordinates;
}