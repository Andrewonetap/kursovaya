#ifndef SQLITE_UTILS_H
#define SQLITE_UTILS_H

#include <sqlite3.h>
#include <glib.h> 

typedef struct {
    int row_count;          
    int column_count; // Количество колонок ДАННЫХ (не считая первой колонки ID/Epoch)       
    double **table;         
    int *epochs; // Значения из первой колонки таблицы (ID/Epoch)            
    char **column_names; // Имена всех колонок, включая первую (ID/Epoch)    
} TableData;

sqlite3 *open_database(const char *filename_utf8);
GList *get_table_list(sqlite3 *db);
void load_table(sqlite3 *db, TableData *table_data, const char *table_name);
double *calculate_new_row(TableData *table_data);
gboolean insert_row_into_table(sqlite3 *db, const char *table_name,
                               int epoch, double *coordinates, int coordinate_count,
                               char **column_names_from_tabledata);

GBytes *load_blob_from_table_by_rowid(sqlite3 *db, 
                                      const char *table_name, 
                                      const char *blob_column_name, 
                                      int rowid);

#endif // SQLITE_UTILS_H