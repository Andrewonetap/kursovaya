#include "sqlite_utils.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stdarg.h> 
#include <time.h>   
#include <math.h>   

static FILE *debug_log_sqlite = NULL; 

static void init_sqlite_debug_log() {
    if (!debug_log_sqlite) {
        debug_log_sqlite = fopen("debug.log", "a"); 
        if (!debug_log_sqlite) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (sqlite_utils.c)\n");
        } else {
            fseek(debug_log_sqlite, 0, SEEK_END);
            if (ftell(debug_log_sqlite) == 0) { 
                 fprintf(debug_log_sqlite, "\xEF\xBB\xBF");
            }
            // fprintf(debug_log_sqlite, "Лог-файл открыт/доступен (sqlite_utils.c)\n"); 
            fflush(debug_log_sqlite);
        }
    }
}

static void debug_print_sqlite(const char *format, ...) {
    init_sqlite_debug_log();
    if (debug_log_sqlite) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_sqlite, format, args);
        va_end(args);
        fflush(debug_log_sqlite);
    }
}

sqlite3 *open_database(const char *filename) {
    sqlite3 *db;
    if (sqlite3_open(filename, &db) == SQLITE_OK) {
        return db;
    }
    debug_print_sqlite("open_database: Не удалось открыть базу данных: %s (SQLite error: %s)\n", filename, sqlite3_errmsg(db));
    if(db) sqlite3_close(db); 
    return NULL;
}

GList *get_table_list(sqlite3 *db) {
    GList *tables = NULL;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';"; 

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *table_name_uchars = sqlite3_column_text(stmt, 0);
            if (table_name_uchars) {
                char *table_name = g_strdup((const char *)table_name_uchars);
                tables = g_list_append(tables, table_name); 
            }
        }
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite("Ошибка при подготовке/выполнении запроса списка таблиц: %s\n", sqlite3_errmsg(db));
    }
    return tables;
}

void load_table(sqlite3 *db, TableData *table_data, const char *table_name) {
    debug_print_sqlite("load_table: Начало загрузки таблицы '%s'\n", table_name); 

    if (!table_data || !table_name || !db) { /* ... */ return; }
    // ... (очистка table_data как в предыдущей версии) ...
    if (table_data->table) { for (int i = 0; i < table_data->row_count; i++) {if(table_data->table[i]) g_free(table_data->table[i]); } g_free(table_data->table); table_data->table = NULL; }
    if (table_data->epochs) { g_free(table_data->epochs); table_data->epochs = NULL; }
    if (table_data->column_names) { int num_names_to_free = table_data->column_count + 1; for (int i = 0; i < num_names_to_free; i++) { if (i < (table_data->column_count + 2) && table_data->column_names[i]) { g_free(table_data->column_names[i]); table_data->column_names[i] = NULL; }} g_free(table_data->column_names); table_data->column_names = NULL;}
    table_data->row_count = 0;
    table_data->column_count = 0; 

    sqlite3_stmt *stmt;
    char sql_buffer[512];
    int total_columns_from_pragma = 0;

    snprintf(sql_buffer, sizeof(sql_buffer), "PRAGMA table_info(\"%s\");", table_name); 
    if (sqlite3_prepare_v2(db, sql_buffer, -1, &stmt, NULL) == SQLITE_OK) {
        GList *temp_col_names = NULL;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
            temp_col_names = g_list_append(temp_col_names, g_strdup(col_name));
            total_columns_from_pragma++;
        }
        sqlite3_finalize(stmt);

        if (total_columns_from_pragma > 0) {
            table_data->column_names = g_new0(char *, total_columns_from_pragma); 
            GList *iter = temp_col_names;
            for (int i = 0; i < total_columns_from_pragma; i++) {
                table_data->column_names[i] = (char*)iter->data; 
                iter = iter->next;
            }
            // Вот здесь КЛЮЧЕВОЕ ИЗМЕНЕНИЕ для таблицы "Значения"
            if (strcmp(table_name, "Значения") == 0) {
                table_data->column_count = total_columns_from_pragma; // Все колонки из PRAGMA - это данные
                debug_print_sqlite("load_table: Table 'Значения', PRAGMA %d cols. ALL are data_cols (column_count)=%d.\n", 
                                   total_columns_from_pragma, table_data->column_count);
            } else { // Для других таблиц, как раньше
                table_data->column_count = total_columns_from_pragma - 1; 
                if (table_data->column_count < 0) table_data->column_count = 0;
                debug_print_sqlite("load_table: Table '%s', PRAGMA %d cols. data_cols (column_count)=%d. First col name: '%s'\n", 
                                   table_name, total_columns_from_pragma, table_data->column_count, 
                                   (total_columns_from_pragma > 0 && table_data->column_names[0]) ? table_data->column_names[0] : "N/A");
            }
        } else { /* ... обработка ошибок ... */ g_list_free_full(temp_col_names, g_free); return; }
        g_list_free(temp_col_names); 
    } else { /* ... обработка ошибок ... */ return; }

    snprintf(sql_buffer, sizeof(sql_buffer), "SELECT COUNT(*) FROM \"%s\";", table_name); 
    if (sqlite3_prepare_v2(db, sql_buffer, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) table_data->row_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    } else { /* ... ошибка ... */ return; }

    if (table_data->row_count == 0) { return; }

    table_data->epochs = g_new0(int, table_data->row_count); // Для "Значения" это будет ROWID
    table_data->table = g_new0(double *, table_data->row_count);
    for (int i = 0; i < table_data->row_count; i++) {
        table_data->table[i] = (table_data->column_count > 0) ? g_new0(double, table_data->column_count) : NULL;
    }

    GString *select_sql = g_string_new("SELECT ");
    // Для таблицы "Значения" нам нужно выбрать ВСЕ колонки из PRAGMA. И также ROWID.
    if (strcmp(table_name, "Значения") == 0) {
        g_string_append(select_sql, "ROWID, "); // Выбираем ROWID первым для "Значения"
    }
    // Добавляем имена всех колонок, как они есть в БД (из PRAGMA)
    for (int i = 0; i < total_columns_from_pragma; ++i) { 
        if (table_data->column_names[i] == NULL) { /* ... ошибка ... */ g_string_free(select_sql, TRUE); return; }
        g_string_append_printf(select_sql, "\"%s\"", table_data->column_names[i]);
        if (i < (total_columns_from_pragma - 1)) { 
            g_string_append(select_sql, ", ");
        }
    }
    // Для "Значения" сортировка по ROWID. Для других - по первой PRAGMA-колонке.
    g_string_append_printf(select_sql, " FROM \"%s\" ORDER BY \"%s\";", 
                            table_name, 
                            (strcmp(table_name, "Значения") == 0) ? "ROWID" : table_data->column_names[0]); 
    debug_print_sqlite("load_table SQL: %s\n", select_sql->str);
    
    if (sqlite3_prepare_v2(db, select_sql->str, -1, &stmt, NULL) == SQLITE_OK) {
        int current_row_idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && current_row_idx < table_data->row_count) {
            int sqlite_col_offset = 0; // Смещение для чтения колонок из stmt
            if (strcmp(table_name, "Значения") == 0) {
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0); // ROWID
                sqlite_col_offset = 1; // Данные начинаются со следующей колонки в stmt
            } else {
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0); // Первая PRAGMA-колонка
                sqlite_col_offset = 1; 
            }
            
            // j - индекс для table_data->table[current_row_idx][j] (от 0 до column_count-1)
            // col_idx_in_stmt - индекс колонки в результате SQLite 
            for (int j = 0; j < table_data->column_count; j++) { 
                int col_idx_in_stmt = j + sqlite_col_offset; 
                int col_type = sqlite3_column_type(stmt, col_idx_in_stmt);

                if (table_data->table[current_row_idx] == NULL) { continue; }
                const char *current_col_name_from_pragma = table_data->column_names[j]; // Имя текущей колонки ДАННЫХ (для "Значения")
                                                                                       // или table_data->column_names[j+1] для других таблиц
                if (strcmp(table_name, "Значения") != 0) {
                     current_col_name_from_pragma = table_data->column_names[j+1]; // j+1 потому что names[0] это Эпоха
                }

                // ---> ОТЛАДОЧНЫЙ БЛОК ДЛЯ ТАБЛИЦЫ "Значения" <---
                if (strcmp(table_name, "Значения") == 0) {
                    if (current_col_name_from_pragma != NULL) { // Здесь current_col_name_from_pragma это names[j]
                         if (strcmp(current_col_name_from_pragma, "A") == 0) {
                            debug_print_sqlite("LOAD_TABLE('Значения'): Строка %d (ROWID %d), Колонка 'A'(idx_stmt %d, idx_j %d): SQLiteType=%d, TextVal='%s', DoubleVal_direct=%f, IntVal=%d\n",
                                               current_row_idx, table_data->epochs[current_row_idx], col_idx_in_stmt, j, col_type,
                                               sqlite3_column_text(stmt, col_idx_in_stmt) ? (const char*)sqlite3_column_text(stmt, col_idx_in_stmt) : "NULL",
                                               sqlite3_column_double(stmt, col_idx_in_stmt),
                                               sqlite3_column_int(stmt, col_idx_in_stmt));
                        } else if (strcmp(current_col_name_from_pragma, "Количество") == 0) {
                             debug_print_sqlite("LOAD_TABLE('Значения'): Строка %d (ROWID %d), Колонка 'Количество'(idx_stmt %d, idx_j %d): SQLiteType=%d, TextVal='%s', DoubleVal_direct=%f, IntVal=%d\n",
                                               current_row_idx, table_data->epochs[current_row_idx], col_idx_in_stmt, j, col_type,
                                               sqlite3_column_text(stmt, col_idx_in_stmt) ? (const char*)sqlite3_column_text(stmt, col_idx_in_stmt) : "NULL",
                                               sqlite3_column_double(stmt, col_idx_in_stmt),
                                               sqlite3_column_int(stmt, col_idx_in_stmt));
                        }
                         else if (strcmp(current_col_name_from_pragma, "E") == 0) {
                             debug_print_sqlite("LOAD_TABLE('Значения'): Строка %d (ROWID %d), Колонка 'E'(idx_stmt %d, idx_j %d): SQLiteType=%d, TextVal='%s', DoubleVal_direct=%f, IntVal=%d\n",
                                               current_row_idx, table_data->epochs[current_row_idx], col_idx_in_stmt, j, col_type,
                                               sqlite3_column_text(stmt, col_idx_in_stmt) ? (const char*)sqlite3_column_text(stmt, col_idx_in_stmt) : "NULL",
                                               sqlite3_column_double(stmt, col_idx_in_stmt),
                                               sqlite3_column_int(stmt, col_idx_in_stmt));
                        }
                         else if (current_col_name_from_pragma && strcmp(current_col_name_from_pragma, "Схема") == 0) { 
                             debug_print_sqlite("LOAD_TABLE('Значения'): Строка %d (ROWID %d), Колонка 'Схема'(idx_stmt %d, idx_j %d): SQLiteType=%d (BLOB)\n",
                                               current_row_idx, table_data->epochs[current_row_idx], col_idx_in_stmt, j, col_type);
                         }
                    }
                }
                // ---> КОНЕЦ ОТЛАДОЧНОГО БЛОКА <---

                if (current_col_name_from_pragma && strcmp(current_col_name_from_pragma, "Схема") == 0) { 
                    table_data->table[current_row_idx][j] = NAN; 
                    continue;
                }

                if (col_type == SQLITE_FLOAT || col_type == SQLITE_INTEGER) {
                    table_data->table[current_row_idx][j] = sqlite3_column_double(stmt, col_idx_in_stmt);
                } else if (col_type == SQLITE_TEXT) {
                    const char *text_val = (const char *)sqlite3_column_text(stmt, col_idx_in_stmt);
                    if (text_val) { 
                        char *endptr;
                        char *text_copy = g_strdup(text_val);
                        for (char *p = text_copy; *p; ++p) { if (*p == ',') *p = '.'; }
                        table_data->table[current_row_idx][j] = g_ascii_strtod(text_copy, &endptr);
                        
                        if (strcmp(table_name, "Значения") == 0 && current_col_name_from_pragma && strcmp(current_col_name_from_pragma, "A") == 0) {
                            debug_print_sqlite("LOAD_TABLE('Значения'): Колонка 'A' ПОСЛЕ g_ascii_strtod('%s'), значение в table_data: %f\n",
                                               text_copy, table_data->table[current_row_idx][j]);
                        }
                        
                        if (endptr == text_copy || (*endptr != '\0' && !g_ascii_isspace(*endptr))) { 
                            if (strcmp(table_name, "Значения") == 0 && current_col_name_from_pragma) { 
                                 debug_print_sqlite("LOAD_TABLE('Значения'): Не удалось сконвертировать '%s' (ориг: '%s') для колонки '%s'. Установлено NAN\n",
                                     text_copy, text_val, current_col_name_from_pragma);
                            }
                            table_data->table[current_row_idx][j] = NAN; 
                        }
                        g_free(text_copy);
                    } else { table_data->table[current_row_idx][j] = NAN; }
                } else if (col_type == SQLITE_NULL) {
                    table_data->table[current_row_idx][j] = NAN; 
                } else {  table_data->table[current_row_idx][j] = NAN; }
                 
                if (strcmp(table_name, "Значения") == 0 && current_col_name_from_pragma && strcmp(current_col_name_from_pragma, "Количество") == 0) {
                    debug_print_sqlite("LOAD_TABLE('Значения'): Колонка 'Количество' ПОСЛЕ ВСЕХ ПРЕОБРАЗОВАНИЙ, значение в table_data: %f\n",
                                        table_data->table[current_row_idx][j]);
                }
            }
            current_row_idx++;
        }
        sqlite3_finalize(stmt);
        if (current_row_idx != table_data->row_count) table_data->row_count = current_row_idx; 
    } else { /* ... ошибка ... */ }
    g_string_free(select_sql, TRUE);
    debug_print_sqlite("load_table: Загрузка таблицы '%s' завершена. Строк: %d, Колонок данных: %d\n", 
                        table_name, table_data->row_count, table_data->column_count);
}

double *calculate_new_row(TableData *table_data) {
    if (!table_data || table_data->row_count < 2 || table_data->column_count == 0) {
        debug_print_sqlite("calculate_new_row: Недостаточно данных для вычисления (rows=%d, cols=%d)\n", 
                           table_data ? table_data->row_count : -1, table_data ? table_data->column_count : -1);
        return NULL;
    }
    
    srand(time(NULL)); // Вызов srand здесь не идеален, но для простоты пока оставим.

    int rows = table_data->row_count;
    int cols = table_data->column_count;
    double *new_coordinates = g_new(double, cols);

    for (int j = 0; j < cols; j++) {
        double max_delta = 0.0;
        for (int i = 0; i < rows - 1; i++) {
            // Убедимся, что table_data->table[i] и table_data->table[i+1] существуют
            if (table_data->table[i] == NULL || table_data->table[i+1] == NULL) {
                debug_print_sqlite("calculate_new_row: Пропуск расчета для колонки %d из-за NULL строки.\n", j);
                new_coordinates[j] = (table_data->table[rows-1] != NULL) ? table_data->table[rows - 1][j] : 0.0; // Возвращаем последнее известное или 0
                goto next_column; // Перейти к следующей колонке
            }
            double delta = fabs(table_data->table[i][j] - table_data->table[i + 1][j]);
            if (delta > max_delta) {
                max_delta = delta;
            }
        }
        double random_factor = (double)rand() / RAND_MAX; 
        double k_val = random_factor * 2.0 * max_delta - max_delta; 
        
        new_coordinates[j] = (table_data->table[rows-1] != NULL) ? (table_data->table[rows - 1][j] + k_val) : k_val;

        next_column:; // Метка для goto
    }
    return new_coordinates;
}

gboolean insert_row_into_table(sqlite3 *db, const char *table_name, int epoch, double *coordinates, int coordinate_count, char **column_names_from_tabledata) {
    if (!db || !table_name || (coordinate_count > 0 && !coordinates) || !column_names_from_tabledata || !column_names_from_tabledata[0]) {
        debug_print_sqlite("insert_row_into_table: Невалидные аргументы.\n");
        return FALSE;
    }

    GString *sql = g_string_new("INSERT INTO \""); 
    g_string_append(sql, table_name);
    g_string_append(sql, "\" (\"");
    g_string_append(sql, column_names_from_tabledata[0]); 
    g_string_append(sql, "\"");

    for (int i = 0; i < coordinate_count; i++) {
        if (column_names_from_tabledata[i + 1] == NULL) {
             debug_print_sqlite("insert_row_into_table: Ошибка: column_names_from_tabledata[%d] is NULL.\n", i+1);
             g_string_free(sql, TRUE); return FALSE;
        }
        g_string_append_printf(sql, ", \"%s\"", column_names_from_tabledata[i + 1]); 
    }
    g_string_append(sql, ") VALUES (?"); 
    for (int i = 0; i < coordinate_count; i++) {
        g_string_append(sql, ", ?"); 
    }
    g_string_append(sql, ");");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql->str, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite("Ошибка подготовки SQL-запроса INSERT: %s (для SQL: %s)\n", sqlite3_errmsg(db), sql->str);
        g_string_free(sql, TRUE);
        return FALSE;
    }
    g_string_free(sql, TRUE); // SQL больше не нужен после подготовки

    sqlite3_bind_int(stmt, 1, epoch); 
    for (int i = 0; i < coordinate_count; i++) {
        sqlite3_bind_double(stmt, i + 2, coordinates[i]); 
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        debug_print_sqlite("Ошибка выполнения SQL-запроса INSERT: %s (код: %d)\n", sqlite3_errmsg(db), rc);
        sqlite3_finalize(stmt);
        return FALSE;
    }

    sqlite3_finalize(stmt);
    return TRUE;
}

GBytes *load_blob_from_table_by_rowid(sqlite3 *db, 
                                      const char *table_name, 
                                      const char *blob_column_name, 
                                      int row_id_value) { 
    sqlite3_stmt *stmt = NULL;
    GString *sql = g_string_new("");
    g_string_printf(sql, "SELECT \"%s\" FROM \"%s\" WHERE ROWID = ? LIMIT 1;", 
                    blob_column_name, table_name);

    if (sqlite3_prepare_v2(db, sql->str, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite("load_blob_by_rowid: Ошибка подготовки SQL: %s\n", sqlite3_errmsg(db));
        g_string_free(sql, TRUE);
        return NULL;
    }
    g_string_free(sql, TRUE);

    sqlite3_bind_int(stmt, 1, row_id_value);

    GBytes *blob_data = NULL;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const void *blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        if (blob && blob_size > 0) {
            // Создаем копию данных, так как указатель от sqlite3_column_blob временный
            void* blob_copy = g_memdup2(blob, blob_size); 
            if (blob_copy) {
                blob_data = g_bytes_new_take(blob_copy, blob_size);
                debug_print_sqlite("load_blob_by_rowid: BLOB загружен, размер: %d байт\n", blob_size);
            } else {
                 debug_print_sqlite("load_blob_by_rowid: Ошибка g_memdup2 для BLOB.\n");
            }
        } else {
            debug_print_sqlite("load_blob_by_rowid: BLOB в ячейке пуст или имеет нулевой размер (rowid=%d).\n", row_id_value);
        }
    } else if (rc == SQLITE_DONE) {
        debug_print_sqlite("load_blob_by_rowid: Строка с ROWID=%d не найдена в таблице '%s'.\n", row_id_value, table_name);
    } else {
        debug_print_sqlite("load_blob_by_rowid: Ошибка выполнения sqlite3_step: %s (код: %d)\n", sqlite3_errmsg(db), rc);
    }

    sqlite3_finalize(stmt);
    return blob_data;
}

gboolean update_table_cell(sqlite3 *db, const char *table_name,
                           const char *data_column_name,
                           const char *id_column_name, /* NULL для WHERE ROWID = ? */
                           double value, int row_id) {
    if (!db || !table_name || !data_column_name) {
        debug_print_sqlite("update_table_cell: Неверные аргументы (db, table_name или data_column_name is NULL).\n");
        return FALSE;
    }
    // id_column_name МОЖЕТ быть NULL, если обновляем по ROWID

    GString *sql = g_string_new("");
    if (id_column_name != NULL) {
        // Используем имя колонки ID (например, "Эпоха")
        g_string_printf(sql, "UPDATE \"%s\" SET \"%s\" = ? WHERE \"%s\" = ?;",
                        table_name, data_column_name, id_column_name);
        debug_print_sqlite("update_table_cell SQL (по ID колонке '%s'): %s\n", id_column_name, sql->str);
    } else {
        // Используем ROWID (для таблицы "Значения")
        g_string_printf(sql, "UPDATE \"%s\" SET \"%s\" = ? WHERE ROWID = ?;",
                        table_name, data_column_name);
        debug_print_sqlite("update_table_cell SQL (по ROWID): %s\n", sql->str);
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql->str, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite("update_table_cell: Ошибка подготовки SQL: %s (SQL: %s)\n",
                           sqlite3_errmsg(db), sql->str);
        g_string_free(sql, TRUE);
        return FALSE;
    }
    g_string_free(sql, TRUE);

    // Биндим значение для SET
    sqlite3_bind_double(stmt, 1, value);
    // Биндим ID для WHERE
    sqlite3_bind_int(stmt, 2, row_id);

    int rc = sqlite3_step(stmt);
    gboolean success = FALSE;
    if (rc != SQLITE_DONE) {
        debug_print_sqlite("update_table_cell: Ошибка выполнения SQL: %s (код: %d)\n",
                           sqlite3_errmsg(db), rc);
    } else {
        // Проверяем, была ли реально затронута строка
        int changes = sqlite3_changes(db);
        if (changes > 0) {
             debug_print_sqlite("update_table_cell: Успешно обновлено %d строк(а) в '%s', колонка '%s', ID/ROWID=%d, значение=%.4f\n",
                           changes, table_name, data_column_name, row_id, value);
            success = TRUE;
        } else {
            debug_print_sqlite("update_table_cell: Внимание: Запрос выполнен, но строки не были изменены (ID/ROWID=%d не найден?).\n", row_id);
            // Можно считать это успехом, если ID действительно нет, или ошибкой.
            // Пока считаем успехом, но можно изменить логику.
             success = TRUE; // Или FALSE, если это критично
        }
    }

    sqlite3_finalize(stmt);
    return success;
}