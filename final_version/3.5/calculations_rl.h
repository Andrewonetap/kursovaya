// calculations_rl.h
// типа заголовок для модуля вычислений

#ifndef CALCULATIONS_RL_H
#define CALCULATIONS_RL_H

#include <stdbool.h>         // для bool
#include "sqlite_utils_rl.h" // нужны типы оттуда типа tabledata

// считает мю для каждой строки таблицы
// table_data указатель на строки (каждая строка массив double)
// row_count сколько строк
// column_count сколько колонок с данными
// вернет массив double со значениями мю (размером row_count) чистить через free
double *compute_mu_rl(double **table_data, int row_count, int column_count);

// считает альфу для каждой строки относительно первой
// mu_values уже посчитанные значения мю
// вернет массив double со значениями альфа (размером row_count) alpha[0] всегда 0 чистить free
double *compute_alpha_rl(double **table_data, int row_count, int column_count, double *mu_values);

// делает экспоненциальное сглаживание для ряда
// input_data одномерный массив что сглаживаем
// data_count сколько элементов в input_data
// out_smoothing_row_count сюда запишется сколько строк получилось (data_count + 1)
// smoothing_coeffs массив коэффов сглаживания а
// num_coeffs сколько этих коэффов
// вернет таблицу сглаживания (массив массивов) [out_smoothing_row_count][num_coeffs]
// каждую строку smoothed_table[i] чистить free потом сам smoothed_table free
double **compute_smoothing_rl(double *input_data, int data_count, int *out_smoothing_row_count,
                              double *smoothing_coeffs, int num_coeffs);

// пишет вычисленные результаты в текстовый файл
// filename имя файла куда писать
// mu_data alpha_data сами массивы мю и альфа
// mu_data_count alpha_data_count их размеры
// smoothing_mu_table smoothing_alpha_table таблицы сглаживания для мю и альфа
// smoothing_mu_rows smoothing_alpha_rows сколько строк в таблицах сглаживания
// epoch_values массив значений для колонки эпох
// epoch_count сколько эпох
// вернет true если все ок
bool write_results_to_file_rl(const char *filename,
                              double *mu_data, int mu_data_count,
                              double *alpha_data, int alpha_data_count,
                              double **smoothing_mu_table, int smoothing_mu_rows,
                              double **smoothing_alpha_table, int smoothing_alpha_rows,
                              int *epoch_values, int epoch_count);

// расчет новой строки (раньше был в sqlite_utils)
// вернет массив double с новыми координатами (размером column_count) чистить free
// null если ошибка или мало данных
double *calculate_new_row_rl(const TableData *table_data);

// закрывает лог файл который использует этот модуль вычислений
// вызывать когда прога заканчивается
void close_calc_debug_log_rl(void);

#endif // CALCULATIONS_RL_H