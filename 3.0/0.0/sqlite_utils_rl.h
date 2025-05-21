#ifndef SQLITE_UTILS_RL_H
#define SQLITE_UTILS_RL_H

#include <sqlite3.h>
#include <stdbool.h> // Для bool

// Структура TableData остается без изменений - она уже "чистая"
typedef struct {
    int row_count;
    int column_count;    // Количество колонок ДАННЫХ (не считая первой колонки ID/Epoch)
    double **table;      // table[row_count][column_count]
    int *epochs;         // Значения из первой колонки таблицы (ID/Epoch) [row_count]
    char **column_names; // Имена всех колонок, включая первую (ID/Epoch) [column_count + 1]
                         // column_names[0] - имя колонки ID/Epoch
                         // column_names[1]...column_names[column_count] - имена колонок данных
} TableData;

// Структура для возврата списка таблиц
typedef struct {
    char **names;
    int count;
} TableList_RL;

// Структура для возврата BLOB-данных
typedef struct {
    unsigned char *data;
    int size;
    bool loaded; // Флаг, успешно ли загружены данные
} BlobData_RL;


sqlite3 *open_database_rl(const char *filename_utf8);

TableList_RL get_table_list_rl(sqlite3 *db);
void free_table_list_rl(TableList_RL *table_list); // Принимаем указатель, чтобы обнулить поля

// Возвращает true при успехе, false при ошибке
bool load_table_rl(sqlite3 *db, TableData *table_data, const char *table_name);
void free_table_data_content_rl(TableData *table_data); // Для очистки содержимого TableData

// calculate_new_row остается в calculations, т.к. это чистая математика, не sqlite
// double *calculate_new_row_rl(TableData *table_data);

bool insert_row_into_table_rl(sqlite3 *db, const char *table_name,
                              int epoch, double *coordinates, int coordinate_count,
                              char **column_names_from_tabledata_db_schema); // Уточнил имя параметра

BlobData_RL load_blob_from_table_by_rowid_rl(sqlite3 *db,
                                           const char *table_name,
                                           const char *blob_column_name,
                                           int rowid);
void free_blob_data_rl(BlobData_RL *blob_data); // Принимаем указатель

bool update_table_cell_rl(sqlite3 *db, const char *table_name,
                          const char *data_column_name,
                          const char *id_column_name, // Имя колонки ID (Эпоха и т.д.), или NULL если WHERE по ROWID
                          double value, int row_id_value); // row_id_value это либо значение из id_column_name, либо ROWID

#endif // SQLITE_UTILS_RL_H