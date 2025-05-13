#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <glib.h> // Для gboolean

// Вычисляет значения μ для каждой строки таблицы
// table_data: указатель на массив строк, каждая строка - массив double
// row_count: количество строк
// column_count: количество колонок данных в каждой строке
// Возвращает массив double* значений μ [row_count]. Освободить через g_free().
double *compute_mu(double **table_data, int row_count, int column_count);

// Вычисляет значения α для каждой строки таблицы относительно первой строки
// mu_values: предварительно вычисленные значения μ
// Возвращает массив double* значений α [row_count]. alpha[0] всегда 0. Освободить через g_free().
double *compute_alpha(double **table_data, int row_count, int column_count, double *mu_values);

// Выполняет экспоненциальное сглаживание для ряда данных
// input_data: одномерный массив данных для сглаживания
// data_count: количество элементов в input_data
// out_smoothing_row_count: указатель, куда будет записано количество строк в результате сглаживания (data_count + 1)
// smoothing_coeffs: массив коэффициентов сглаживания A
// num_coeffs: количество коэффициентов сглаживания
// Возвращает двумерный массив double** [out_smoothing_row_count][num_coeffs].
// Каждый элемент smoothed_table[i] нужно освободить через g_free(), а затем сам smoothed_table.
double **compute_smoothing(double *input_data, int data_count, int *out_smoothing_row_count,
                           double *smoothing_coeffs, int num_coeffs);

// Записывает вычисленные результаты в текстовый файл
// filename: имя файла для записи
// mu_data, alpha_data: массивы значений μ и α
// mu_data_count, alpha_data_count: их размеры
// smoothing_mu_table, smoothing_alpha_table: таблицы результатов сглаживания для μ и α
// smoothing_mu_rows, smoothing_alpha_rows: количество строк в таблицах сглаживания
// epoch_values: массив значений эпох
// epoch_count: количество эпох
// Возвращает TRUE в случае успеха.
gboolean write_results_to_file(const char *filename,
                               double *mu_data, int mu_data_count,
                               double *alpha_data, int alpha_data_count,
                               double **smoothing_mu_table, int smoothing_mu_rows,
                               double **smoothing_alpha_table, int smoothing_alpha_rows,
                               int *epoch_values, int epoch_count);

#endif // CALCULATIONS_H#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <glib.h> // Для gboolean

// Вычисляет значения μ для каждой строки таблицы
// table_data: указатель на массив строк, каждая строка - массив double
// row_count: количество строк
// column_count: количество колонок данных в каждой строке
// Возвращает массив double* значений μ [row_count]. Освободить через g_free().
double *compute_mu(double **table_data, int row_count, int column_count);

// Вычисляет значения α для каждой строки таблицы относительно первой строки
// mu_values: предварительно вычисленные значения μ
// Возвращает массив double* значений α [row_count]. alpha[0] всегда 0. Освободить через g_free().
double *compute_alpha(double **table_data, int row_count, int column_count, double *mu_values);

// Выполняет экспоненциальное сглаживание для ряда данных
// input_data: одномерный массив данных для сглаживания
// data_count: количество элементов в input_data
// out_smoothing_row_count: указатель, куда будет записано количество строк в результате сглаживания (data_count + 1)
// smoothing_coeffs: массив коэффициентов сглаживания A
// num_coeffs: количество коэффициентов сглаживания
// Возвращает двумерный массив double** [out_smoothing_row_count][num_coeffs].
// Каждый элемент smoothed_table[i] нужно освободить через g_free(), а затем сам smoothed_table.
double **compute_smoothing(double *input_data, int data_count, int *out_smoothing_row_count,
                           double *smoothing_coeffs, int num_coeffs);

// Записывает вычисленные результаты в текстовый файл
// filename: имя файла для записи
// mu_data, alpha_data: массивы значений μ и α
// mu_data_count, alpha_data_count: их размеры
// smoothing_mu_table, smoothing_alpha_table: таблицы результатов сглаживания для μ и α
// smoothing_mu_rows, smoothing_alpha_rows: количество строк в таблицах сглаживания
// epoch_values: массив значений эпох
// epoch_count: количество эпох
// Возвращает TRUE в случае успеха.
gboolean write_results_to_file(const char *filename,
                               double *mu_data, int mu_data_count,
                               double *alpha_data, int alpha_data_count,
                               double **smoothing_mu_table, int smoothing_mu_rows,
                               double **smoothing_alpha_table, int smoothing_alpha_rows,
                               int *epoch_values, int epoch_count);

#endif // CALCULATIONS_H