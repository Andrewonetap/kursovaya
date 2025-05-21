#include "sqlite_utils_rl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h> // Для NAN, если понадобится где-то (хотя в load_table уже есть)
#include <ctype.h>

// Логгирование (остается как есть, только меняем имя функции для уникальности, если нужно)
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
            // fprintf(debug_log_sqlite_rl, "Лог-файл открыт/доступен (sqlite_utils_rl.c)\n");
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
    TableList_RL list = {0}; // Инициализация нулями (names = NULL, count = 0)
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";

    if (!db) {
        debug_print_sqlite_rl("get_table_list_rl: db is NULL.\n");
        return list;
    }

    // Сначала посчитаем количество таблиц
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
        return list; // count останется 0
    }
    list.count = count; // Устанавливаем реальное количество, под которое выделена память

    // Теперь получим имена
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        int current_idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && current_idx < count) {
            const unsigned char *table_name_uchars = sqlite3_column_text(stmt, 0);
            if (table_name_uchars) {
                list.names[current_idx] = strdup((const char *)table_name_uchars);
                if (!list.names[current_idx]) {
                    debug_print_sqlite_rl("get_table_list_rl: Ошибка strdup для имени таблицы.\n");
                    // Очистка уже выделенных имен и массива
                    for (int i = 0; i < current_idx; ++i) free(list.names[i]);
                    free(list.names);
                    list.names = NULL;
                    list.count = 0;
                    sqlite3_finalize(stmt);
                    return list;
                }
                current_idx++;
            }
        }
        list.count = current_idx; // Фактическое количество успешно считанных имен
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite_rl("get_table_list_rl: Ошибка при получении имен таблиц: %s\n", sqlite3_errmsg(db));
        free(list.names); // Освобождаем массив, если имена не были получены
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
            }
        }
        free(table_list->names);
        table_list->names = NULL;
        table_list->count = 0;
    }
}

void free_table_data_content_rl(TableData *table_data) {
    if (!table_data) return;
    debug_print_sqlite_rl("free_table_data_content_rl: Очистка TableData...\n");

    if (table_data->table) {
        for (int i = 0; i < table_data->row_count; i++) {
            if (table_data->table[i]) {
                free(table_data->table[i]);
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
        // column_names[0] - имя ID/Epoch
        // column_names[1]...column_names[column_count] - имена колонок данных
        // Всего column_count + 1 имен
        for (int i = 0; i <= table_data->column_count; i++) { // <= column_count, т.к. имен на 1 больше
            if (table_data->column_names[i]) {
                free(table_data->column_names[i]);
            }
        }
        free(table_data->column_names);
        table_data->column_names = NULL;
    }
    table_data->row_count = 0;
    table_data->column_count = 0;
    debug_print_sqlite_rl("free_table_data_content_rl: Очистка TableData завершена.\n");
}


bool load_table_rl(sqlite3 *db, TableData *table_data, const char *table_name) {
    debug_print_sqlite_rl("load_table_rl: Начало загрузки таблицы '%s'\n", table_name);

    if (!db || !table_data || !table_name) {
        debug_print_sqlite_rl("load_table_rl: Невалидные аргументы (db, table_data или table_name is NULL).\n");
        return false;
    }

    free_table_data_content_rl(table_data); // Очищаем предыдущее содержимое

    sqlite3_stmt *stmt = NULL;
    char sql_buffer[512];
    int total_columns_from_pragma = 0;

    // 1. Получаем информацию о колонках и их имена через PRAGMA
    snprintf(sql_buffer, sizeof(sql_buffer), "PRAGMA table_info(\"%s\");", table_name);
    if (sqlite3_prepare_v2(db, sql_buffer, -1, &stmt, NULL) == SQLITE_OK) {
        // Сначала посчитаем количество колонок
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            total_columns_from_pragma++;
        }
        sqlite3_finalize(stmt); // Завершаем первый prepare

        if (total_columns_from_pragma == 0) {
            debug_print_sqlite_rl("load_table_rl: PRAGMA table_info не вернула колонок для таблицы '%s'.\n", table_name);
            return false; // Таблица пуста или не существует
        }

        // Выделяем память под имена колонок (total_columns_from_pragma штук)
        table_data->column_names = (char**)calloc(total_columns_from_pragma, sizeof(char*));
        if (!table_data->column_names) {
            debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для column_names.\n");
            return false;
        }

        // Снова готовим запрос PRAGMA для чтения имен
        if (sqlite3_prepare_v2(db, sql_buffer, -1, &stmt, NULL) == SQLITE_OK) {
            int name_idx = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW && name_idx < total_columns_from_pragma) {
                const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
                if (col_name) {
                    table_data->column_names[name_idx] = strdup(col_name);
                    if (!table_data->column_names[name_idx]) {
                        debug_print_sqlite_rl("load_table_rl: Ошибка strdup для имени колонки '%s'.\n", col_name);
                        // Очистка уже выделенных имен
                        for(int k=0; k<name_idx; ++k) free(table_data->column_names[k]);
                        free(table_data->column_names);
                        table_data->column_names = NULL;
                        sqlite3_finalize(stmt);
                        return false;
                    }
                } else {
                     table_data->column_names[name_idx] = strdup("Unnamed"); // Заглушка
                }
                name_idx++;
            }
            sqlite3_finalize(stmt);
        } else {
            debug_print_sqlite_rl("load_table_rl: Ошибка PRAGMA table_info (второй вызов): %s\n", sqlite3_errmsg(db));
            free(table_data->column_names); // column_names[i] еще не содержат strdup'нутых строк
            table_data->column_names = NULL;
            return false;
        }
        
        // Определяем column_count (количество колонок ДАННЫХ)
        // Для таблицы "Значения" (или как она у тебя называется) все колонки из PRAGMA - это данные.
        // Для других таблиц первая колонка - это ID/Эпоха, остальные - данные.
        // Предположим, что имя таблицы "Значения" передается в AppData_Raylib.values_table_name
        // Здесь для простоты сделаем проверку по имени "Значения"
        if (strcmp(table_name, "Значения") == 0) { // Используй app_data.values_table_name в реальном коде
            table_data->column_count = total_columns_from_pragma;
        } else {
            table_data->column_count = total_columns_from_pragma > 0 ? total_columns_from_pragma - 1 : 0;
        }
        debug_print_sqlite_rl("load_table_rl: Таблица '%s', PRAGMA %d колонок. column_count (данных) = %d.\n",
                               table_name, total_columns_from_pragma, table_data->column_count);

    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка PRAGMA table_info (первый вызов): %s\n", sqlite3_errmsg(db));
        return false;
    }

    // 2. Получаем количество строк
    snprintf(sql_buffer, sizeof(sql_buffer), "SELECT COUNT(*) FROM \"%s\";", table_name);
    if (sqlite3_prepare_v2(db, sql_buffer, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            table_data->row_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка SELECT COUNT(*): %s\n", sqlite3_errmsg(db));
        // Очищаем column_names, так как они уже могли быть выделены
        if (table_data->column_names) {
            for(int k=0; k<total_columns_from_pragma; ++k) free(table_data->column_names[k]);
            free(table_data->column_names);
            table_data->column_names = NULL;
        }
        table_data->column_count = 0;
        return false;
    }

    if (table_data->row_count == 0) {
        debug_print_sqlite_rl("load_table_rl: Таблица '%s' пуста (0 строк).\n", table_name);
        // column_names остаются, так как структура таблицы известна
        return true; // Успешно загрузили пустую таблицу
    }

    // 3. Выделяем память для epochs и table
    table_data->epochs = (int*)calloc(table_data->row_count, sizeof(int));
    table_data->table = (double**)calloc(table_data->row_count, sizeof(double*));
    if (!table_data->epochs || !table_data->table) {
        debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для epochs или table.\n");
        if(table_data->epochs) free(table_data->epochs); table_data->epochs = NULL;
        if(table_data->table) free(table_data->table); table_data->table = NULL;
        if (table_data->column_names) { /* ... очистка column_names ... */ } // Полная очистка
        return false;
    }
    for (int i = 0; i < table_data->row_count; i++) {
        if (table_data->column_count > 0) {
            table_data->table[i] = (double*)calloc(table_data->column_count, sizeof(double));
            if (!table_data->table[i]) {
                debug_print_sqlite_rl("load_table_rl: Ошибка выделения памяти для table[%d].\n", i);
                // Нужна более сложная очистка всего, что уже выделено
                for(int k=0; k<i; ++k) free(table_data->table[k]);
                free(table_data->table); free(table_data->epochs);
                if (table_data->column_names) { /* ... очистка column_names ... */ }
                return false;
            }
        } else {
            table_data->table[i] = NULL; // Нет колонок данных
        }
    }

    // 4. Формируем и выполняем SELECT запрос для получения данных
    // Собираем строку запроса SELECT "col1", "col2", ... FROM "table" ORDER BY "first_col_from_pragma" (или ROWID)
    char *select_sql_str = (char*)malloc(total_columns_from_pragma * 256 + 256); // Грубая оценка размера
    if (!select_sql_str) { /* ... ошибка ... */ return false; }

    strcpy(select_sql_str, "SELECT ");
    bool is_values_table_type = (strcmp(table_name, "Значения") == 0); // Заменить на app_data.values_table_name

    if (is_values_table_type) {
        strcat(select_sql_str, "ROWID, "); // Для "Значения" первой читаем ROWID
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
    strcat(select_sql_str, "\" ORDER BY \"");
    if (is_values_table_type) {
        strcat(select_sql_str, "ROWID"); // Сортировка по ROWID для "Значения"
    } else {
        strcat(select_sql_str, table_data->column_names[0]); // Сортировка по первой колонке из PRAGMA
    }
    strcat(select_sql_str, "\";");

    debug_print_sqlite_rl("load_table_rl SQL: %s\n", select_sql_str);

    if (sqlite3_prepare_v2(db, select_sql_str, -1, &stmt, NULL) == SQLITE_OK) {
        int current_row_idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && current_row_idx < table_data->row_count) {
            int sqlite_col_offset_for_data = 0; // Смещение в результате SQLite для начала колонок ДАННЫХ

            if (is_values_table_type) {
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0); // ROWID
                sqlite_col_offset_for_data = 1; // Данные начинаются со следующей колонки SQLite
            } else {
                table_data->epochs[current_row_idx] = sqlite3_column_int(stmt, 0); // Первая колонка PRAGMA
                sqlite_col_offset_for_data = 1;
            }

            for (int j = 0; j < table_data->column_count; j++) { // j - индекс колонки данных в table_data->table
                // col_idx_in_stmt - индекс колонки в результате SQLite для текущей j-ой колонки данных
                int col_idx_in_stmt = j + sqlite_col_offset_for_data;
                
                // Имя текущей колонки данных (для проверки на "Схема" и для отладки)
                // Для "Значения": table_data->column_names[j] (т.к. column_count == total_columns_from_pragma)
                // Для других: table_data->column_names[j+1] (т.к. column_names[0] это ID)
                const char *current_data_col_name_in_db = is_values_table_type ?
                                                          table_data->column_names[j] :
                                                          table_data->column_names[j+1];

                // Пропускаем BLOB колонку "Схема"
                // Заменить "Схема" на app_data.schema_blob_column_name
                if (current_data_col_name_in_db && strcmp(current_data_col_name_in_db, "Схема") == 0) {
                    if (table_data->table[current_row_idx]) // Проверка, что строка аллоцирована
                        table_data->table[current_row_idx][j] = NAN; // Заполняем NAN
                    continue;
                }

                int col_type = sqlite3_column_type(stmt, col_idx_in_stmt);
                if (table_data->table[current_row_idx]) { // Еще одна проверка
                    if (col_type == SQLITE_FLOAT || col_type == SQLITE_INTEGER) {
                        table_data->table[current_row_idx][j] = sqlite3_column_double(stmt, col_idx_in_stmt);
                    } else if (col_type == SQLITE_TEXT) {
                        const char *text_val = (const char *)sqlite3_column_text(stmt, col_idx_in_stmt);
                        if (text_val) {
                            char *endptr;
                            // Копируем строку, чтобы можно было заменить запятые
                            char *text_copy = strdup(text_val);
                            if (text_copy) {
                                for (char *p = text_copy; *p; ++p) { if (*p == ',') *p = '.'; }
                                table_data->table[current_row_idx][j] = strtod(text_copy, &endptr);
                                if (endptr == text_copy || (*endptr != '\0' && !isspace(*endptr))) {
                                    table_data->table[current_row_idx][j] = NAN;
                                }
                                free(text_copy);
                            } else {
                                table_data->table[current_row_idx][j] = NAN;
                            }
                        } else {
                            table_data->table[current_row_idx][j] = NAN;
                        }
                    } else if (col_type == SQLITE_NULL) {
                        table_data->table[current_row_idx][j] = NAN;
                    } else { // BLOB (кроме "Схемы", которую обработали) или неизвестный тип
                        table_data->table[current_row_idx][j] = NAN;
                    }
                }
            }
            current_row_idx++;
        }
        sqlite3_finalize(stmt);
        // Если прочитали меньше строк, чем ожидали (маловероятно при правильном COUNT)
        if (current_row_idx != table_data->row_count) {
            debug_print_sqlite_rl("load_table_rl: Предупреждение: прочитано %d строк, ожидалось %d.\n", current_row_idx, table_data->row_count);
            table_data->row_count = current_row_idx; // Корректируем row_count
        }
    } else {
        debug_print_sqlite_rl("load_table_rl: Ошибка SELECT данных: %s\n", sqlite3_errmsg(db));
        free(select_sql_str);
        // Нужна полная очистка TableData, так как она в неконсистентном состоянии
        free_table_data_content_rl(table_data); // Это очистит и column_names
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
    if (!db || !table_name || (coordinate_count > 0 && !coordinates) ||
        !column_names_from_tabledata_db_schema || !column_names_from_tabledata_db_schema[0] ||
        (coordinate_count > 0 && !column_names_from_tabledata_db_schema[1])) {
        debug_print_sqlite_rl("insert_row_into_table_rl: Невалидные аргументы.\n");
        return false;
    }

    // Оценка размера SQL строки
    size_t sql_len = strlen("INSERT INTO \"\" (\"\") VALUES (?);") + strlen(table_name) + strlen(column_names_from_tabledata_db_schema[0]);
    for (int i = 0; i < coordinate_count; i++) {
        if (!column_names_from_tabledata_db_schema[i + 1]) {
             debug_print_sqlite_rl("insert_row_into_table_rl: Ошибка: имя колонки данных [%d] is NULL.\n", i+1);
             return false;
        }
        sql_len += strlen(", \"\"") + strlen(column_names_from_tabledata_db_schema[i + 1]);
        sql_len += strlen(", ?");
    }
    sql_len += 10; // Запас

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
        debug_print_sqlite_rl("Ошибка подготовки SQL-запроса INSERT: %s (для SQL: %s)\n", sqlite3_errmsg(db), sql_str);
        free(sql_str);
        return false;
    }
    free(sql_str); // SQL больше не нужен после подготовки

    sqlite3_bind_int(stmt, 1, epoch);
    for (int i = 0; i < coordinate_count; i++) {
        sqlite3_bind_double(stmt, i + 2, coordinates[i]);
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        debug_print_sqlite_rl("Ошибка выполнения SQL-запроса INSERT: %s (код: %d)\n", sqlite3_errmsg(db), rc);
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
    BlobData_RL blob_result = {0}; // data=NULL, size=0, loaded=false
    sqlite3_stmt *stmt = NULL;
    char sql[512];

    if (!db || !table_name || !blob_column_name) {
        debug_print_sqlite_rl("load_blob_by_rowid_rl: Невалидные аргументы.\n");
        return blob_result;
    }

    snprintf(sql, sizeof(sql), "SELECT \"%s\" FROM \"%s\" WHERE ROWID = ? LIMIT 1;",
             blob_column_name, table_name);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print_sqlite_rl("load_blob_by_rowid_rl: Ошибка подготовки SQL: %s\n", sqlite3_errmsg(db));
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
                debug_print_sqlite_rl("load_blob_by_rowid_rl: BLOB загружен, ROWID=%d, размер: %d байт\n", rowid_value, blob_size_bytes);
            } else {
                 debug_print_sqlite_rl("load_blob_by_rowid_rl: Ошибка malloc для BLOB.\n");
            }
        } else {
            debug_print_sqlite_rl("load_blob_by_rowid_rl: BLOB в ячейке пуст или имеет нулевой размер (ROWID=%d).\n", rowid_value);
        }
    } else if (rc == SQLITE_DONE) {
        debug_print_sqlite_rl("load_blob_by_rowid_rl: Строка с ROWID=%d не найдена в таблице '%s'.\n", rowid_value, table_name);
    } else {
        debug_print_sqlite_rl("load_blob_by_rowid_rl: Ошибка выполнения sqlite3_step: %s (код: %d)\n", sqlite3_errmsg(db), rc);
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
                          const char *id_column_name, /* NULL для WHERE ROWID = ? */
                          double value, int row_id_value) {
    if (!db || !table_name || !data_column_name) {
        debug_print_sqlite_rl("update_table_cell_rl: Неверные аргументы (db, table_name или data_column_name is NULL).\n");
        return false;
    }

    char sql[512];
    if (id_column_name != NULL) {
        snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET \"%s\" = ? WHERE \"%s\" = ?;",
                 table_name, data_column_name, id_column_name);
    } else {
        snprintf(sql, sizeof(sql), "UPDATE \"%s\" SET \"%s\" = ? WHERE ROWID = ?;",
                 table_name, data_column_name);
    }
    // debug_print_sqlite_rl("update_table_cell_rl SQL: %s\n", sql); // Можно включить для отладки

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