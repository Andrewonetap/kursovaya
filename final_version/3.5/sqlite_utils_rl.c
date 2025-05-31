// sqlite_utils_rl.c

#include "sqlite_utils_rl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h> // Для NAN
#include <ctype.h>

// Логгирование
static FILE *debug_log_sqlite_rl = NULL;

static void init_sqlite_debug_log_rl() {
    if (!debug_log_sqlite_rl) {
        debug_log_sqlite_rl = fopen("debug.log", "a");
        if (!debug_log_sqlite_rl) {
            fprintf(stderr, "Не удалось открыть debug.log для записи (sqlite_utils_rl.c)\n");
        } else {
            fseek(debug_log_sqlite_rl, 0, SEEK_END);
            if (ftell(debug_log_sqlite_rl) == 0) {
                 fprintf(debug_log_sqlite_rl, "\xEF\xBB\xBF"); // UTF-8 BOM
            }
            fflush(debug_log_sqlite_rl);
        }
    }
}

static void debug_print_sqlite_rl(const char *format, ...) {
    init_sqlite_debug_log_rl();
    if (debug_log_sqlite_rl) {
        va_list args;
        va_start(args, format);
        vfprintf(debug_log_sqlite_rl, format, args);
        va_end(args);
        fflush(debug_log_sqlite_rl);
    }
}

// Функция для закрытия лог-файла
void close_sqlite_debug_log_rl(void) {
    if (debug_log_sqlite_rl) {
        fprintf(debug_log_sqlite_rl, "Лог-файл закрывается (sqlite_utils_rl.c)\n");
        fflush(debug_log_sqlite_rl); // Убедимся, что все записано перед закрытием
        fclose(debug_log_sqlite_rl);
        debug_log_sqlite_rl = NULL;
    }
}

// --- Реализация функций ---

sqlite3 *open_database_rl(const char *filename_utf8) {
    sqlite3 *db;
    if (sqlite3_open(filename_utf8, &db) == SQLITE_OK) {
        debug_print_sqlite_rl("open_database_rl: База данных '%s' успешно открыта.\n", filename_utf8);
        return db;
    }
    debug_print_sqlite_rl("open_database_rl: Не удалось открыть базу данных: %s (SQLite error: %s)\n", filename_utf8, sqlite3_errmsg(db));
    if(db) sqlite3_close(db);
    return NULL;
}

TableList_RL get_table_list_rl(sqlite3 *db) {
    TableList_RL list = {0};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";

    if (!db) {
        debug_print_sqlite_rl("get_table_list_rl: db is NULL.\n");
        return list;
    }

    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            count++;
        }
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite_rl("get_table_list_rl: Ошибка при подсчете таблиц: %s\n", sqlite3_errmsg(db));
        return list;
    }

    if (count == 0) {
        debug_print_sqlite_rl("get_table_list_rl: Таблиц не найдено.\n");
        return list;
    }

    list.names = (char**)calloc(count, sizeof(char*));
    if (!list.names) {
        debug_print_sqlite_rl("get_table_list_rl: Ошибка выделения памяти для имен таблиц.\n");
        return list;
    }
    // list.count будет установлен ниже на фактическое количество успешно считанных имен

    int current_idx = 0; // Для отслеживания успешно считанных имен
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && current_idx < count) {
            const unsigned char *table_name_uchars = sqlite3_column_text(stmt, 0);
            if (table_name_uchars) {
                list.names[current_idx] = strdup((const char *)table_name_uchars);
                if (!list.names[current_idx]) {
                    debug_print_sqlite_rl("get_table_list_rl: Ошибка strdup для имени таблицы.\n");
                    for (int i = 0; i < current_idx; ++i) free(list.names[i]); // Освобождаем уже strdup'нутые
                    free(list.names);
                    list.names = NULL;
                    list.count = 0;
                    sqlite3_finalize(stmt);
                    return list;
                }
                current_idx++;
            }
        }
        list.count = current_idx; // Устанавливаем фактическое количество успешно считанных имен
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite_rl("get_table_list_rl: Ошибка при получении имен таблиц: %s\n", sqlite3_errmsg(db));
        // Так как current_idx = 0 на этом этапе (если prepare не удался), никакие list.names[i] не были strdup'нуты.
        // Освобождаем только сам массив list.names
        free(list.names);
        list.names = NULL;
        list.count = 0;
    }
    return list;
}

void free_table_list_rl(TableList_RL *table_list) {
    if (table_list && table_list->names) {
        for (int i = 0; i < table_list->count; i++) {
            if (table_list->names[i]) {
                free(table_list->names[i]);
                table_list->names[i] = NULL; 
            }
        }
        free(table_list->names);
        table_list->names = NULL;
        table_list->count = 0;
    }
}

void free_table_data_content_rl(TableData *table_data) {
    if (!table_data) return;
    debug_print_sqlite_rl("free_table_data_content_rl: Очистка TableData (rows: %d, cols_alloc: %d)...\n",
                           table_data->row_count, table_data->num_allocated_column_names);

    if (table_data->table) {
        for (int i = 0; i < table_data->row_count; i++) { // row_count - фактическое число строк данных
            if (table_data->table[i]) {
                free(table_data->table[i]);
                table_data->table[i] = NULL;
            }
        }
        free(table_data->table);
        table_data->table = NULL;
    }
    if (table_data->epochs) {
        free(table_data->epochs);
        table_data->epochs = NULL;
    }
    if (table_data->column_names) {
        // Используем num_allocated_column_names для корректного освобождения
        for (int i = 0; i < table_data->num_allocated_column_names; i++) {
            if (table_data->column_names[i]) {
                free(table_data->column_names[i]);
                table_data->column_names[i] = NULL;
            }
        }
        free(table_data->column_names);
        table_data->column_names = NULL;
    }
    table_data->row_count = 0;
    table_data->column_count = 0;
    table_data->num_allocated_column_names = 0; // Сброс нового поля
    debug_print_sqlite_rl("free_table_data_content_rl: Очистка TableData завершена.\n");
}


bool load_table_rl(sqlite3 *db, TableData *table_data, const char *table_name) {
    debug_print_sqlite_rl("load_table_rl: Начало загрузки таблицы '%s'\n", table_name);

    if (!db || !table_data || !table_name) {
        debug_print_sqlite_rl("load_table_rl: Невалидные аргументы (db, table_data или table_name is NULL).\n");
        return false;
    }

    free_table_data_content_rl(table_data); // Очищаем предыдущее содержимое (обнулит и num_allocated_column_names)

    sqlite3_stmt *stmt = NULL;
    char sql_buffer_pragma[512]; // Отдельный буфер для PRAGMA
    int total_columns_from_pragma = 0;

    // 1. Получаем информацию о колонках и их имена через PRAGMA
    snprintf(sql_buffer_pragma, sizeof(sql_buffer_pragma), "PRAGMA table_info(\"%s\");", table_name);
    if (sqlite3_prepare_v2(db, sql_buffer_pragma, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            total_columns_from_pragma++;
        }
        sqlite3_finalize(stmt); // Завершаем первый prepare (подсчет колонок)

        if (total_columns_from_pragma == 0) {
            debug_print_sqlite_rl("load_table_rl: PRAGMA table_info не вернула колонок для таблицы '%s'.\n", table_name);
            // free_table_data_content_rl(table_data); // Уже вызван в начале, здесь table_data пуст
            return false; 
        }

        table_data->column_names = (char**)calloc(total_columns_from_pragma, sizeof(char*));
        if (!table_data->column_names) {
            debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для column_names.\n");
            // free_table_data_content_rl(table_data); // Уже вызван, table_data пуст
            return false;
        }
        table_data->num_allocated_column_names = total_columns_from_pragma; // Устанавливаем фактическое кол-во

        // Снова готовим запрос PRAGMA для чтения имен
        if (sqlite3_prepare_v2(db, sql_buffer_pragma, -1, &stmt, NULL) == SQLITE_OK) {
            int name_idx = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW && name_idx < total_columns_from_pragma) {
                const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
                if (col_name) {
                    table_data->column_names[name_idx] = strdup(col_name);
                    if (!table_data->column_names[name_idx]) {
                        debug_print_sqlite_rl("load_table_rl: Ошибка strdup для имени колонки '%s'.\n", col_name);
                        sqlite3_finalize(stmt);
                        free_table_data_content_rl(table_data); // Полная очистка
                        return false;
                    }
                } else {
                     table_data->column_names[name_idx] = strdup("Unnamed"); // Заглушка
                     if (!table_data->column_names[name_idx]) { /* Маловероятно, но для полноты */
                        debug_print_sqlite_rl("load_table_rl: Ошибка strdup для 'Unnamed'.\n");
                        sqlite3_finalize(stmt);
                        free_table_data_content_rl(table_data);
                        return false;
                     }
                }
                name_idx++;
            }
            sqlite3_finalize(stmt);
        } else {
            debug_print_sqlite_rl("load_table_rl: Ошибка PRAGMA table_info (второй вызов): %s\n", sqlite3_errmsg(db));
            free_table_data_content_rl(table_data); // Полная очистка
            return false;
        }
        
        // Предполагаем, что `values_table_name` берется из `AppData` в вызывающем коде
        // Для простоты используем строковый литерал "Значения"
        if (strcmp(table_name, "Значения") == 0) { 
            table_data->column_count = total_columns_from_pragma; // Все колонки - данные
        } else {
            table_data->column_count = total_columns_from_pragma > 0 ? total_columns_from_pragma - 1 : 0; // Первая ID/Epoch, остальные данные
        }
        debug_print_sqlite_rl("load_table_rl: Таблица '%s', PRAGMA %d колонок. column_count (данных) = %d.\n",
                               table_name, total_columns_from_pragma, table_data->column_count);

    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка PRAGMA table_info (первый вызов): %s\n", sqlite3_errmsg(db));
        // free_table_data_content_rl(table_data); // Уже вызван, table_data пуст
        return false;
    }

    // 2. Получаем количество строк
    char sql_count_buffer[512];
    snprintf(sql_count_buffer, sizeof(sql_count_buffer), "SELECT COUNT(*) FROM \"%s\";", table_name);
    if (sqlite3_prepare_v2(db, sql_count_buffer, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            table_data->row_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка SELECT COUNT(*): %s\n", sqlite3_errmsg(db));
        free_table_data_content_rl(table_data); // Полная очистка (включая column_names)
        return false;
    }

    if (table_data->row_count == 0) {
        debug_print_sqlite_rl("load_table_rl: Таблица '%s' пуста (0 строк).\n", table_name);
        return true; 
    }

    // 3. Выделяем память для epochs и table
    table_data->epochs = (int*)calloc(table_data->row_count, sizeof(int));
    table_data->table = (double**)calloc(table_data->row_count, sizeof(double*));
    if (!table_data->epochs || !table_data->table) {
        debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для epochs или table.\n");
        free_table_data_content_rl(table_data); // Полная очистка
        return false;
    }
    for (int i = 0; i < table_data->row_count; i++) {
        if (table_data->column_count > 0) { // Только если есть колонки данных
            table_data->table[i] = (double*)calloc(table_data->column_count, sizeof(double));
            if (!table_data->table[i]) {
                debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для table[%d].\n", i);
                free_table_data_content_rl(table_data); // Полная очистка
                return false;
            }
        } else {
            table_data->table[i] = NULL; // Нет колонок данных
        }
    }

    // 4. Формируем и выполняем SELECT запрос
    size_t select_sql_len_estimate = strlen("SELECT ROWID, ") + (total_columns_from_pragma * 258) + strlen(" FROM \"\" ORDER BY \"\";") + (2 * strlen(table_name)) + 256;
    char *select_sql_str = (char*)malloc(select_sql_len_estimate);
    if (!select_sql_str) {
        debug_print_sqlite_rl("load_table_rl: Ошибка malloc для select_sql_str.\n");
        free_table_data_content_rl(table_data); // Полная очистка
        return false;
    }

    strcpy(select_sql_str, "SELECT ");
    bool is_values_table_type = (strcmp(table_name, "Значения") == 0);

    if (is_values_table_type && table_data->num_allocated_column_names > 0) {
        // Если это "Значения" и есть хоть одна колонка, сначала читаем ROWID.
        // Однако, ROWID не входит в column_names, полученные из PRAGMA.
        // Правильнее всего будет всегда читать по именам колонок из PRAGMA,
        // а ROWID (эпохи) для "Значения" устанавливать как 1, 2, 3... или читать его отдельно,
        // если он не является одной из PRAGMA колонок.
        // Здесь я упрощу: для "Значения" эпоха - это ROWID, которую прочитаем как первую.
        // Для других - первая колонка из PRAGMA.
        strcat(select_sql_str, "ROWID"); 
        if (total_columns_from_pragma > 0) strcat(select_sql_str, ", ");
    }

    for (int i = 0; i < total_columns_from_pragma; ++i) {
        strcat(select_sql_str, "\"");
        strcat(select_sql_str, table_data->column_names[i]); // Имена из PRAGMA
        strcat(select_sql_str, "\"");
        if (i < total_columns_from_pragma - 1) {
            strcat(select_sql_str, ", ");
        }
    }
    strcat(select_sql_str, " FROM \"");
    strcat(select_sql_str, table_name);
    strcat(select_sql_str, "\" ORDER BY ");
    if (is_values_table_type) {
        strcat(select_sql_str, "ROWID");
    } else if (total_columns_from_pragma > 0 && table_data->column_names[0]) {
        strcat(select_sql_str, "\"");
        strcat(select_sql_str, table_data->column_names[0]); // Первая колонка из PRAGMA
        strcat(select_sql_str, "\"");
    } else {
        // Если нет колонок или имен, то сортировка не нужна или по ROWID по умолчанию.
        // SQLite все равно может требовать ORDER BY для LIMIT. Чтобы избежать ошибки, можно убрать ORDER BY
        // Но лучше чтобы имя для сортировки было. Здесь оставляем как есть, что может вызвать ошибку,
        // если total_columns_from_pragma == 0, но это должно было отсечься ранее.
        // Удалим часть с ORDER BY, если нет явного столбца для сортировки, или используем ROWID по умолчанию
        // Безопаснее всего: если для "не Значения" нет первой колонки, убрать ORDER BY или сделать по ROWID
        // strcpy(select_sql_str + strlen(select_sql_str) - strlen(" ORDER BY "), ""); // Удалить " ORDER BY "
        // Однако, если table_data->column_names[0] NULL (маловероятно после strdup("Unnamed")),
        // это вызовет ошибку. Поэтому выше защита total_columns_from_pragma > 0 && table_data->column_names[0]
        strcat(select_sql_str, "ROWID"); // Резервный вариант, если логика выше не дала колонку.
    }
    strcat(select_sql_str, ";");
    debug_print_sqlite_rl("load_table_rl SQL: %s\n", select_sql_str);

    if (sqlite3_prepare_v2(db, select_sql_str, -1, &stmt, NULL) == SQLITE_OK) {
        int current_row_idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && current_row_idx < table_data->row_count) {
            int sqlite_col_idx_for_data_start = 0; // Индекс в sqlite3_column_... откуда начинаются колонки ДАННЫХ
            
            if (is_values_table_type) {
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0); // Это ROWID
                sqlite_col_idx_for_data_start = 1; // Колонки PRAGMA (они же данные) начинаются со второго столбца SELECT
            } else {
                // Для "обычных" таблиц, table_data->column_names[0] - это имя эпохи.
                // В SELECT мы выбрали все column_names из PRAGMA.
                // Поэтому столбец 0 из SELECT - это и есть эпоха.
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0);
                sqlite_col_idx_for_data_start = 1; // Колонки данных (X1, X2...) начинаются со второго столбца SELECT
            }

            for (int j = 0; j < table_data->column_count; j++) { 
                // j - индекс колонки ДАННЫХ в table_data->table[current_row_idx][j]

                // Определяем, какой колонке в table_data->column_names (из PRAGMA)
                // соответствует j-ая колонка ДАННЫХ
                int pragma_col_name_idx;
                if (is_values_table_type) {
                    pragma_col_name_idx = j; // Данные - это j-ая колонка из PRAGMA
                } else {
                    pragma_col_name_idx = j + 1; // Данные - это (j+1)-ая колонка из PRAGMA (0-я это эпоха)
                }
                
                // Индекс в результате SELECT для этой колонки данных
                int col_idx_in_select_result = sqlite_col_idx_for_data_start + j;

                // Имя колонки из PRAGMA (используется для проверки "Схема")
                const char *current_data_col_name_in_db = (pragma_col_name_idx < table_data->num_allocated_column_names) ?
                                                          table_data->column_names[pragma_col_name_idx] : "INVALID_NAME_IDX";

                // Предположим, schema_blob_column_name берется из AppData
                if (strcmp(current_data_col_name_in_db, "Схема") == 0) { 
                    if (table_data->table[current_row_idx]) 
                        table_data->table[current_row_idx][j] = NAN; 
                    continue;
                }

                int col_type = sqlite3_column_type(stmt, col_idx_in_select_result);
                if (table_data->table[current_row_idx]) {
                    if (col_type == SQLITE_FLOAT || col_type == SQLITE_INTEGER) {
                        table_data->table[current_row_idx][j] = sqlite3_column_double(stmt, col_idx_in_select_result);
                    } else if (col_type == SQLITE_TEXT) {
                        const char *text_val = (const char *)sqlite3_column_text(stmt, col_idx_in_select_result);
                        if (text_val) {
                            char *endptr;
                            char *text_copy = strdup(text_val); // Копируем, чтобы можно было менять
                            if (text_copy) {
                                for (char *p = text_copy; *p; ++p) { if (*p == ',') *p = '.'; }
                                table_data->table[current_row_idx][j] = strtod(text_copy, &endptr);
                                if (endptr == text_copy || (*endptr != '\0' && !isspace((unsigned char)*endptr))) {
                                    table_data->table[current_row_idx][j] = NAN;
                                }
                                free(text_copy);
                            } else {
                                table_data->table[current_row_idx][j] = NAN; // Ошибка strdup
                            }
                        } else {
                            table_data->table[current_row_idx][j] = NAN; // text_val is NULL
                        }
                    } else if (col_type == SQLITE_NULL) {
                        table_data->table[current_row_idx][j] = NAN;
                    } else { // BLOB (кроме "Схемы") или неизвестный тип
                        table_data->table[current_row_idx][j] = NAN;
                    }
                }
            }
            current_row_idx++;
        }
        sqlite3_finalize(stmt);
        if (current_row_idx != table_data->row_count) {
            debug_print_sqlite_rl("load_table_rl: Предупреждение: прочитано %d строк, ожидалось %d. Корректируем row_count.\n", current_row_idx, table_data->row_count);
            table_data->row_count = current_row_idx; // Корректируем, если прочитали меньше
        }
    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка SELECT данных: %s\n", sqlite3_errmsg(db));
        free(select_sql_str);
        free_table_data_content_rl(table_data); // Полная очистка
        return false;
    }
    free(select_sql_str);

    debug_print_sqlite_rl("load_table_rl: Загрузка таблицы '%s' завершена. Строк: %d, Колонок данных: %d\n",
                        table_name, table_data->row_count, table_data->column_count);
    return true;
}


bool insert_row_into_table_rl(sqlite3 *db, const char *table_name,
                              int epoch, double *coordinates, int coordinate_count,
                              char **column_names_from_tabledata_db_schema) {
    // column_names_from_tabledata_db_schema[0] - имя колонки ID/Epoch
    // column_names_from_tabledata_db_schema[1]...[coordinate_count] - имена колонок данных
    // Важно: num_allocated_column_names из TableData должен соответствовать этому.
    if (!db || !table_name || (coordinate_count > 0 && !coordinates) ||
        !column_names_from_tabledata_db_schema || !column_names_from_tabledata_db_schema[0] ||
        (coordinate_count > 0 && !column_names_from_tabledata_db_schema[1])) { // Проверяем хотя бы первое имя колонки данных
        debug_print_sqlite_rl("insert_row_into_table_rl: Невалидные аргументы.\n");
        return false;
    }

    size_t sql_len = strlen("INSERT INTO \"\" (\"\") VALUES (?);") + strlen(table_name) + strlen(column_names_from_tabledata_db_schema[0]);
    for (int i = 0; i < coordinate_count; i++) {
        // Здесь нужно убедиться, что column_names_from_tabledata_db_schema[i + 1] существует и не NULL
        // Эта проверка уже была выше, но стоит помнить об этом.
        if (!column_names_from_tabledata_db_schema[i + 1]) {
             debug_print_sqlite_rl("insert_row_into_table_rl: Ошибка: имя колонки данных [%d] is NULL.\n", i+1);
             return false;
        }
        sql_len += strlen(", \"\"") + strlen(column_names_from_tabledata_db_schema[i + 1]);
        sql_len += strlen(", ?");
    }
    sql_len += 20; // Дополнительный запас

    char *sql_str = (char*)malloc(sql_len);
    if (!sql_str) {
        debug_print_sqlite_rl("insert_row_into_table_rl: Ошибка malloc для SQL строки.\n");
        return false;
    }

    sprintf(sql_str, "INSERT INTO \"%s\" (\"%s\"", table_name, column_names_from_tabledata_db_schema[0]);
    for (int i = 0; i < coordinate_count; i++) {
        sprintf(sql_str + strlen(sql_str), ", \"%s\"", column_names_from_tabledata_db_schema[i + 1]);
    }
    strcat(sql_str, ") VALUES (?");
    for (int i = 0; i < coordinate_count; i++) {
        strcat(sql_str, ", ?");
    }
    strcat(sql_str, ");");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_str, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite_rl("insert_row_into_table_rl: Ошибка подготовки SQL-запроса INSERT: %s (для SQL: %s)\n", sqlite3_errmsg(db), sql_str);
        free(sql_str);
        return false;
    }
    free(sql_str); 

    sqlite3_bind_int(stmt, 1, epoch);
    for (int i = 0; i < coordinate_count; i++) {
        sqlite3_bind_double(stmt, i + 2, coordinates[i]);
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        debug_print_sqlite_rl("insert_row_into_table_rl: Ошибка выполнения SQL-запроса INSERT: %s (код: %d)\n", sqlite3_errmsg(db), rc);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    debug_print_sqlite_rl("insert_row_into_table_rl: Строка успешно добавлена в '%s'.\n", table_name);
    return true;
}

BlobData_RL load_blob_from_table_by_rowid_rl(sqlite3 *db,
                                           const char *table_name,
                                           const char *blob_column_name,
                                           int rowid_value) {
    BlobData_RL blob_result = {0}; 
    sqlite3_stmt *stmt = NULL;
    char sql[512];

    if (!db || !table_name || !blob_column_name) {
        debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: Невалидные аргументы.\n");
        return blob_result;
    }

    snprintf(sql, sizeof(sql), "SELECT \"%s\" FROM \"%s\" WHERE ROWID = ? LIMIT 1;",
             blob_column_name, table_name);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: Ошибка подготовки SQL: %s\n", sqlite3_errmsg(db));
        return blob_result;
    }

    sqlite3_bind_int(stmt, 1, rowid_value);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const void *blob_ptr = sqlite3_column_blob(stmt, 0);
        int blob_size_bytes = sqlite3_column_bytes(stmt, 0);
        if (blob_ptr && blob_size_bytes > 0) {
            blob_result.data = (unsigned char*)malloc(blob_size_bytes);
            if (blob_result.data) {
                memcpy(blob_result.data, blob_ptr, blob_size_bytes);
                blob_result.size = blob_size_bytes;
                blob_result.loaded = true;
                debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: BLOB загружен, ROWID=%d, '%s', размер: %d байт\n",
                                       rowid_value, blob_column_name, blob_size_bytes);
            } else {
                 debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: Ошибка malloc для BLOB.\n");
            }
        } else {
            debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: BLOB в ячейке пуст или имеет нулевой размер (ROWID=%d, '%s').\n",
                                   rowid_value, blob_column_name);
        }
    } else if (rc == SQLITE_DONE) {
        debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: Строка с ROWID=%d не найдена в таблице '%s'.\n", rowid_value, table_name);
    } else {
        debug_print_sqlite_rl("load_blob_from_table_by_rowid_rl: Ошибка выполнения sqlite3_step: %s (код: %d)\n", sqlite3_errmsg(db), rc);
    }

    sqlite3_finalize(stmt);
    return blob_result;
}

void free_blob_data_rl(BlobData_RL *blob_data) {
    if (blob_data && blob_data->data) {
        free(blob_data->data);
        blob_data->data = NULL;
        blob_data->size = 0;
        blob_data->loaded = false;
    }
}

bool update_table_cell_rl(sqlite3 *db, const char *table_name,
                          const char *data_column_name,
                          const char *id_column_name, 
                          double value, int row_id_value) {
    if (!db || !table_name || !data_column_name) {
        debug_print_sqlite_rl("update_table_cell_rl: Неверные аргументы (db, table_name или data_column_name is NULL).\n");
        return false;
    }

    char sql[512];
    if (id_column_name != NULL && strlen(id_column_name) > 0) { // Явная проверка на непустое имя ID колонки
        snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET \"%s\" = ? WHERE \"%s\" = ?;",
                 table_name, data_column_name, id_column_name);
    } else {
        snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET \"%s\" = ? WHERE ROWID = ?;",
                 table_name, data_column_name);
    }
    // debug_print_sqlite_rl("update_table_cell_rl SQL: %s, val: %.4f, id: %d\n", sql, value, row_id_value);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite_rl("update_table_cell_rl: Ошибка подготовки SQL: %s (SQL: %s)\n",
                           sqlite3_errmsg(db), sql);
        return false;
    }

    sqlite3_bind_double(stmt, 1, value);
    sqlite3_bind_int(stmt, 2, row_id_value);

    int rc = sqlite3_step(stmt);
    bool success = false;
    if (rc != SQLITE_DONE) {
        debug_print_sqlite_rl("update_table_cell_rl: Ошибка выполнения SQL: %s (код: %d)\n",
                           sqlite3_errmsg(db), rc);
    } else {
        int changes = sqlite3_changes(db);
        if (changes > 0) {
             debug_print_sqlite_rl("update_table_cell_rl: Успешно обновлено %d строк(а) в '%s', колонка '%s', ID/ROWID=%d, значение=%.4f\n",
                           changes, table_name, data_column_name, row_id_value, value);
            success = true;
        } else {
            debug_print_sqlite_rl("update_table_cell_rl: Внимание: Запрос выполнен, но строки не были изменены (ID/ROWID=%d не найден?).\n", row_id_value);
            success = true; // Считаем успехом, если ID не найден (нет ошибки запроса)
        }
    }

    sqlite3_finalize(stmt);
    return success;
}