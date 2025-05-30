// color_themes.h
#ifndef COLOR_THEMES_H
#define COLOR_THEMES_H

#include "app_data_rl.h" // Для структуры ThemeColors

// Идентификаторы тем
#define LIGHT_THEME_ID 0
#define DARK_THEME_ID 1


#define TOTAL_THEMES 2 // Общее количество тем

// Глобальный массив для хранения всех тем (объявление extern)
extern ThemeColors app_themes[TOTAL_THEMES];

// Функция для инициализации всех доступных тем (прототип)
void InitializeAllApplicationThemes(void);

#endif // COLOR_THEMES_H