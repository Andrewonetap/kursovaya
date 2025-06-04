// color_themes.h
// заголовочный файл для цветовых тем

#ifndef COLOR_THEMES_H
#define COLOR_THEMES_H

#include "app_data_rl.h" // тут лежит структура ThemeColors

// id для тем
#define LIGHT_THEME_ID 0 // светлая тема
#define DARK_THEME_ID 1  // темная тема
// если будут еще то дальше 2 3 и тд

#define TOTAL_THEMES 2 // сколько всего тем в программе

// массив со всеми темами (определен в color_themes.c)
extern ThemeColors app_themes[TOTAL_THEMES];

// функция которая создает все темы (вызывается при старте)
void InitializeAllApplicationThemes(void);

#endif // COLOR_THEMES_H