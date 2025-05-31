#include "calculations_rl.h"
#include <math.h>     
#include <stdio.h>   
#include <stdlib.h>
#include <string.h>
#include <stdarg.h> 

static FILE *debug_log_calc_rl = NULL; // файл лога для вычислений

// открывает лог если надо
static void init_calc_debug_log_rl() {
    if (!debug_log_calc_rl) { // если еще не открыт
        debug_log_calc_rl = fopen("debug.log", "a"); // пробуем открыть на дозапись
        if (!debug_log_calc_rl) {
            // не вышло ну и ладно
            fprintf(stderr, "Не удалось открыть debug.log для записи (calculations_rl.c)\n");
        } else {
            fseek(debug_log_calc_rl, 0, SEEK_END); // в конец файла
            if (ftell(debug_log_calc_rl) == 0) { // если файл новый
                 fprintf(debug_log_calc_rl, "\xEF\xBB\xBF"); // пишем bom для utf8
            }
            // типа лог открыт
            fflush(debug_log_calc_rl); // сбрасываем буфер
        }
    }
}

// пишет в лог отладки
static void debug_print_calc_rl(const char *format, ...) {
    init_calc_debug_log_rl(); // сначала убедимся что лог есть
    if (debug_log_calc_rl) { // если файл доступен
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_calc_rl, format, args); // пишем форматированную строку
        va_end(args);
        fflush(debug_log_calc_rl); // чтобы сразу записалось
    }
}

// --- дальше функции ---

// считает мю
double *compute_mu_rl(double **table_data, int row_count, int column_count) {
    if (!table_data || row_count <= 0) { // проверка входных данных
        debug_print_calc_rl("compute_mu_rl: Входные данные невалидны (нет таблицы или строк <= 0).\n");
        return NULL; // если фигня на входе то и на выходе фигня
    }

    // память под значения мю
    double *mu_values = (double*)calloc(row_count, sizeof(double));
    if (!mu_values) { // если не выделили
        debug_print_calc_rl("compute_mu_rl: Ошибка выделения памяти для mu_values.\n");
        return NULL; // то и делать нечего
    }
    debug_print_calc_rl("compute_mu_rl: Начало вычисления μ для %d строк, %d колонок.\n", row_count, column_count);

    for (int i = 0; i < row_count; i++) { // по строкам
        if (column_count > 0 && table_data[i] == NULL) { // если строка пустая а колонки вроде есть
             debug_print_calc_rl("compute_mu_rl: Ошибка, table_data[%d] == NULL, а column_count > 0. Устанавливаю mu[%d] = 0.\n", i, i);
             mu_values[i] = 0.0; // мю будет ноль
             continue;
        }
        if (column_count == 0) { // если колонок нет
            mu_values[i] = 0.0; // мю тоже ноль
            continue;
        }

        double sum_of_squares = 0.0; // сумма квадратов
        for (int j = 0; j < column_count; j++) { // по колонкам
            sum_of_squares += table_data[i][j] * table_data[i][j]; // считаем сумму
        }
        mu_values[i] = sqrt(sum_of_squares); // мю это корень из суммы квадратов
    }

    debug_print_calc_rl("compute_mu_rl: Вычислено %d значений μ.\n", row_count);
    return mu_values; // возвращаем массив мю
}

// закрывает файл лога
void close_calc_debug_log_rl(void) {
    if (debug_log_calc_rl) { // если был открыт
        fprintf(debug_log_calc_rl, "Лог-файл закрывается (calculations_rl.c)\n");
        fflush(debug_log_calc_rl); // дописать все
        fclose(debug_log_calc_rl); // закрыть
        debug_log_calc_rl = NULL;  // обнулить указатель
    }
}

// считает альфу
double *compute_alpha_rl(double **table_data, int row_count, int column_count, double *mu_values) {
    if (!table_data || !mu_values || row_count <= 0) { // проверка что все нужное есть
        debug_print_calc_rl("compute_alpha_rl: Входные данные невалидны (нет таблицы, μ или строк <= 0).\n");
        return NULL;
    }

    // память под альфу
    double *alpha_values = (double*)calloc(row_count, sizeof(double)); 
    if (!alpha_values) { // если не дали память
        debug_print_calc_rl("compute_alpha_rl: Ошибка выделения памяти для alpha_values.\n");
        return NULL;
    }
    debug_print_calc_rl("compute_alpha_rl: Начало вычисления α для %d строк, %d колонок.\n", row_count, column_count);


    if (column_count == 0) { // если колонок нет
        debug_print_calc_rl("compute_alpha_rl: Количество колонок данных равно 0. Все α будут 0.\n");
        return alpha_values; // то все альфы нули
    }
    
    if (table_data[0] == NULL) { // если первая строка (базовая) пустая
        debug_print_calc_rl("compute_alpha_rl: Ошибка, table_data[0] == NULL. Невозможно вычислить α.\n");
        return alpha_values; // то ничего не выйдет
    }
    // alpha_values[0] и так 0 из-за calloc (угол с самим собой)

    for (int i = 1; i < row_count; i++) { // по строкам начиная со второй
        if (table_data[i] == NULL) { // если текущая строка никакая
             debug_print_calc_rl("compute_alpha_rl: Ошибка, table_data[%d] == NULL. Устанавливаю alpha[%d] = 0.\n", i, i);
             alpha_values[i] = 0.0; // альфа для нее ноль
             continue;
        }

        double dot_product = 0.0; // скалярное произведение
        for (int j = 0; j < column_count; j++) { // по колонкам
            dot_product += table_data[0][j] * table_data[i][j]; // с первой строкой
        }

        double denominator = mu_values[0] * mu_values[i]; // знаменатель
        if (fabs(denominator) < 1e-9) { // если знаменатель почти ноль
            alpha_values[i] = 0.0; // альфа ноль
        } else {
            double cos_alpha_val = dot_product / denominator; // косинус альфы
            if (cos_alpha_val > 1.0) cos_alpha_val = 1.0;   // чтобы из acos не вылезло что попало
            if (cos_alpha_val < -1.0) cos_alpha_val = -1.0;
            alpha_values[i] = acos(cos_alpha_val); // сама альфа через арккосинус
        }
    }

    debug_print_calc_rl("compute_alpha_rl: Вычислено %d значений α.\n", row_count);
    return alpha_values; // возвращаем массив альф
}

// делает экспоненциальное сглаживание
double **compute_smoothing_rl(double *input_data, int data_count, int *out_smoothing_row_count,
                              double *smoothing_coeffs, int num_coeffs) {
    if (!input_data || data_count <= 0) { // если нет данных
        debug_print_calc_rl("compute_smoothing_rl: Нет входных данных для сглаживания (data_count <= 0).\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }
    if (!smoothing_coeffs || num_coeffs <= 0) { // если нет коэффициентов
        debug_print_calc_rl("compute_smoothing_rl: Нет коэффициентов сглаживания (num_coeffs <= 0).\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }

    int result_rows = data_count + 1; // N исходных строк + 1 прогнозная
    if(out_smoothing_row_count) *out_smoothing_row_count = result_rows;
    debug_print_calc_rl("compute_smoothing_rl: Начало. Входных данных: %d. Коэффициентов: %d. Строк результата: %d\n",
                data_count, num_coeffs, result_rows);

    // память под таблицу сглаживания
    double **smoothed_table = (double**)malloc(result_rows * sizeof(double*));
    if (!smoothed_table) {
        debug_print_calc_rl("compute_smoothing_rl: Ошибка malloc для smoothed_table.\n");
        if(out_smoothing_row_count) *out_smoothing_row_count = 0;
        return NULL;
    }
    for (int i = 0; i < result_rows; i++) {
        smoothed_table[i] = (double*)calloc(num_coeffs, sizeof(double)); // обнуляем
        if (!smoothed_table[i]) { // если для строки не выделили
            debug_print_calc_rl("compute_smoothing_rl: Ошибка calloc для smoothed_table[%d].\n", i);
            for(int k=0; k<i; ++k) free(smoothed_table[k]); // чистим что успели
            free(smoothed_table);
            if(out_smoothing_row_count) *out_smoothing_row_count = 0;
            return NULL;
        }
    }

    double data_sum_for_avg = 0.0; // сумма для среднего
    for (int i = 0; i < data_count; i++) {
        data_sum_for_avg += input_data[i];
    }
    // среднее по входным данным (если данных нет то первое значение)
    double data_average = (data_count > 0) ? (data_sum_for_avg / data_count) : (data_count > 0 ? input_data[0] : 0.0);


    // первая строка таблицы сглаживания s0
    for (int j = 0; j < num_coeffs; j++) { // j это индекс коэффициента
        smoothed_table[0][j] = input_data[0] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * data_average;
    }

    // строки сглаживания s1 до s(n-1)
    for (int i = 1; i < data_count; i++) { // i это индекс m в таблице также индекс y во входных данных
        for (int j = 0; j < num_coeffs; j++) { // j это индекс коэффициента
            smoothed_table[i][j] = input_data[i] * smoothing_coeffs[j] + (1.0 - smoothing_coeffs[j]) * smoothed_table[i-1][j];
        }
    }

    // прогнозная строка sn на момент n+1
    // прогноз это просто последнее сглаженное значение
    if (data_count > 0) { // data_count тут N (количество точек)
        for (int j = 0; j < num_coeffs; j++) {
            smoothed_table[data_count][j] = smoothed_table[data_count - 1][j];
        }
    } else { // если входных данных было 0 (или 1 но тут уже отработало для m=0)
        // эта ветка на случай data_count = 0 копирует smoothed_table[0] в smoothed_table[0] что не страшно
        // для data_count = 1 предыдущий if скопирует smoothed_table[0] в smoothed_table[1] что правильно
        for (int j = 0; j < num_coeffs; j++) {
             smoothed_table[data_count][j] = smoothed_table[0][j]; // для 0 данных будет копия из себя в себя
        }
    }

    debug_print_calc_rl("compute_smoothing_rl: Завершено. Таблица сглаживания: %d строк, %d столбцов.\n",
                result_rows, num_coeffs);
    return smoothed_table; // таблица сглаженных значений
}

// пишет результаты в файл
bool write_results_to_file_rl(const char *filename,
                              double *mu_data, int mu_data_count,
                              double *alpha_data, int alpha_data_count,
                              double **smoothing_mu_table, int smoothing_mu_rows,
                              double **smoothing_alpha_table, int smoothing_alpha_rows,
                              int *epoch_values, int epoch_count) {
    FILE *out_file = fopen(filename, "w"); // открываем файл на запись
    if (!out_file) { // если не открылся
        debug_print_calc_rl("write_results_to_file_rl: Не удалось открыть файл '%s' для записи.\n", filename);
        return false;
    }
    debug_print_calc_rl("write_results_to_file_rl: Запись результатов в файл '%s'\n", filename);

    fprintf(out_file, "\xEF\xBB\xBF"); // bom для utf8

    fprintf(out_file, "Эпоха\tμ (мю)\tα (альфа)\n"); // заголовок для мю и альфа
    int max_rows_initial = 0; // сколько всего строк для основной части
    if (mu_data_count > max_rows_initial) max_rows_initial = mu_data_count;
    if (alpha_data_count > max_rows_initial) max_rows_initial = alpha_data_count;
    if (epoch_count > max_rows_initial) max_rows_initial = epoch_count;


    for (int i = 0; i < max_rows_initial; i++) { // пишем мю и альфа
        fprintf(out_file, "%d\t", (epoch_values && i < epoch_count) ? epoch_values[i] : i + 1); // номер эпохи
        if (mu_data && i < mu_data_count) fprintf(out_file, "%.4f\t", mu_data[i]); // значение мю
        else fprintf(out_file, "N/A\t"); // или нет данных
        if (alpha_data && i < alpha_data_count) fprintf(out_file, "%.4E\n", alpha_data[i]); // значение альфа
        else fprintf(out_file, "N/A\n"); // или нет данных
    }

    // таблица сглаживания мю
    if (smoothing_mu_table && smoothing_mu_rows > 0 && mu_data_count > 0) { 
        fprintf(out_file, "\nТаблица сглаживания μ (мю)\n"); // заголовок
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tμ (реальное)\n"); // колонки
        for (int i = 0; i < smoothing_mu_rows; i++) { // по строкам сглаживания
            fprintf(out_file, "%d\t", i); // номер периода м
            if (smoothing_mu_table[i]) { // сглаженные значения для разных а
                 fprintf(out_file, "%.4f\t%.4f\t%.4f\t%.4f\t",
                        smoothing_mu_table[i][0], smoothing_mu_table[i][1],
                        smoothing_mu_table[i][2], smoothing_mu_table[i][3]);
            } else {
                 fprintf(out_file, "N/A\tN/A\tN/A\tN/A\t"); // если что-то не так
            }

            if (mu_data && i < mu_data_count) { // реальное мю для периода 0 .. n-1
                fprintf(out_file, "%.4f\n", mu_data[i]);
            } else if (i == mu_data_count && i == smoothing_mu_rows - 1) { // для прогнозной строки м=n
                 fprintf(out_file, "(прогноз)\n");
            } else {
                fprintf(out_file, "N/A\n"); // если не нашлось
            }
        }
    }

    // таблица сглаживания альфа
    if (smoothing_alpha_table && smoothing_alpha_rows > 0 && alpha_data_count > 1) { // alpha_data_count > 1 так как альфа[0] это 0
        fprintf(out_file, "\nТаблица сглаживания α (альфа)\n"); // заголовок
        fprintf(out_file, "M (период)\tA=0.1\tA=0.4\tA=0.7\tA=0.9\tα (реальное)\n"); // колонки
        for (int i = 0; i < smoothing_alpha_rows; i++) { // m=0 это s0alpha сглаженное для реального alpha_data[1]
            fprintf(out_file, "%d\t", i); // номер периода м
            if (smoothing_alpha_table[i]) { // сглаженные значения
                fprintf(out_file, "%.4E\t%.4E\t%.4E\t%.4E\t",
                        smoothing_alpha_table[i][0], smoothing_alpha_table[i][1],
                        smoothing_alpha_table[i][2], smoothing_alpha_table[i][3]);
            } else {
                fprintf(out_file, "N/A\tN/A\tN/A\tN/A\t"); // если нет
            }

            // реальное альфа для m=i это alpha_data[i+1] так как alpha_data[0] не используется для сглаживания
            if (alpha_data && (i + 1) < alpha_data_count) {
                fprintf(out_file, "%.4E\n", alpha_data[i + 1]);
            } else if (i == (alpha_data_count - 1) && i == smoothing_alpha_rows - 1) { // прогнозная строка
                // i == smoothing_alpha_rows-1 это последняя строка сглаживания (прогноз)
                // i == alpha_data_count-1 тоже означает последнюю доступную альфу
                // но так как реальные значения для сравнения берутся alpha_data[i+1]
                // для прогнозного i которое равно (количество_точек_сглаживания)-1 
                // реального значения уже нет (оно было для i-1), поэтому это прогноз
                // эта проверка немного запутана но в целом для прогнозной строки пишет (прогноз)
                fprintf(out_file, "(прогноз)\n");
            } else {
                fprintf(out_file, "N/A\n"); // если не нашлось
            }
        }
    }

    fclose(out_file); // закрываем файл
    debug_print_calc_rl("write_results_to_file_rl: Результаты успешно записаны в '%s'\n", filename);
    return true; // все ок
}

// вычисляет новую строку координат
double *calculate_new_row_rl(const TableData *table_data) {
    if (!table_data || table_data->row_count < 2 || table_data->column_count <= 0) { // надо хотя бы 2 строки данных
        debug_print_calc_rl("calculate_new_row_rl: Недостаточно данных для вычисления (rows=%d, cols=%d)\n",
                           table_data ? table_data->row_count : -1, table_data ? table_data->column_count : -1);
        return NULL; // мало данных
    }

    int rows = table_data->row_count; // количество строк
    int cols = table_data->column_count; // количество колонок
    double *new_coordinates = (double*)malloc(cols * sizeof(double)); // память под новую строку
    if (!new_coordinates) { // если не дали
        debug_print_calc_rl("calculate_new_row_rl: Ошибка malloc для new_coordinates.\n");
        return NULL;
    }

    for (int j = 0; j < cols; j++) { // по каждой координате (колонке)
        if (table_data->table[rows - 1] == NULL) { // последняя строка должна быть (от нее пляшем)
             debug_print_calc_rl("calculate_new_row_rl: Ошибка, последняя строка table_data->table[%d] == NULL.\n", rows-1);
             free(new_coordinates); return NULL; // без нее никак
        }
        double max_delta = 0.0; // максимальное изменение по координате
        for (int i = 0; i < rows - 1; i++) { // ищем максимальную дельту между соседними строками
            if (table_data->table[i] == NULL || table_data->table[i+1] == NULL) { // если где-то дырка в данных
                debug_print_calc_rl("calculate_new_row_rl: Пропуск расчета дельты для колонки %d из-за NULL строки в середине данных.\n", j);
                // max_delta останется прежней или 0 если все дырявое
                continue;
            }
            double delta = fabs(table_data->table[i][j] - table_data->table[i + 1][j]); // модуль разницы
            if (delta > max_delta) {
                max_delta = delta; // запоминаем максимум
            }
        }
        double random_factor = (double)rand() / RAND_MAX; // случайное число от 0 до 1
        double k_val = random_factor * 2.0 * max_delta - max_delta; // случайное значение в диапазоне [-max_delta, +max_delta]
        new_coordinates[j] = table_data->table[rows - 1][j] + k_val; // новая координата = последняя + случайное смещение
    }
    return new_coordinates; // готовая новая строка
}