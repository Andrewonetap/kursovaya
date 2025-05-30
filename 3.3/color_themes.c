// color_themes.c
#include "color_themes.h" // Содержит объявление ThemeColors, app_themes, InitializeAllApplicationThemes, и ID тем
#include "raylib.h"       // Для ColorToInt, ColorBrightness, GetColor, Fade, BLACK, RED, GRAY и т.д.

// Определение глобального массива тем
// Этот массив будет содержать определения для каждой темы.
ThemeColors app_themes[TOTAL_THEMES];

// Функция для инициализации всех доступных тем
void InitializeAllApplicationThemes(void) {

    // --- СВЕТЛАЯ ТЕМА (ID: LIGHT_THEME_ID) ---
    // Используем указатель для удобства доступа к полям текущей темы
    ThemeColors *lt = &app_themes[LIGHT_THEME_ID];

    // --- Базовые цвета для светлой темы (взяты из твоего InitApp) ---
    unsigned int clr_panel_background_lt     = 0xf1f2f2FF; // HEX: #f1f2f2 (Светло-серый, почти белый)
    unsigned int clr_list_item_bg_lt         = 0xe3e8ffFF; // HEX: #e3e8ff (Очень светло-голубой)
    unsigned int clr_text_primary_lt         = 0x303032FF; // HEX: #303032 (Темно-серый, почти черный)
    unsigned int clr_border_universal_lt     = 0x080808FF; // HEX: #080808 (Очень темно-серый, почти черный)
    unsigned int clr_button_base_normal_lt   = 0xdcdbe1FF; // HEX: #dcdbe1 (Светло-серый)
    // Для тулбара ты использовал 0xE9ECEFFF в DrawTopNavigationBar, его и возьмем для header_toolbar_bg
    unsigned int clr_header_toolbar_bg_lt    = 0xE9ECEFFF; // HEX: #E9ECEF (Очень светлый серо-голубой)
    unsigned int clr_text_on_button_normal_lt= clr_text_primary_lt;
    unsigned int clr_text_on_button_active_lt= clr_panel_background_lt; // Текст на нажатой кнопке будет как фон панели (светлый)
    unsigned int clr_text_disabled_lt        = 0xA0A0A0FF; // HEX: #A0A0A0 (Средне-серый)
    unsigned int clr_border_focused_accent_lt= 0x007BFFFF; // HEX: #007BFF (Ярко-синий)
    unsigned int clr_textbox_bg_normal_lt    = 0xFFFFFFFF; // HEX: #FFFFFF (Белый)

    // --- Присвоение значений полям структуры ThemeColors для СВЕТЛОЙ ТЕМЫ ---

    // Основные цвета интерфейса
    lt->window_background     = clr_panel_background_lt;      // Фон всего окна приложения
    lt->panel_background      = clr_panel_background_lt;      // Фон для основных панелей, контейнеров, диалоговых окон
    lt->header_toolbar_bg     = clr_header_toolbar_bg_lt;     // Фон для верхнего тулбара и заголовков окон (например, GuiWindowBox)
    lt->text_primary          = clr_text_primary_lt;          // Основной цвет текста для меток, содержимого полей и т.д.
    lt->text_disabled         = clr_text_disabled_lt;         // Цвет для неактивного/отключенного текста
    lt->border_universal      = clr_border_universal_lt;      // Общий цвет для большинства рамок (кнопки, панели, списки)
    lt->border_focused_accent = clr_border_focused_accent_lt; // Цвет рамки для элементов в фокусе (например, активный TextBox)

    // Кнопки (BUTTON)
    lt->button_base_normal    = clr_button_base_normal_lt;    // Фоновый цвет кнопки в обычном состоянии
    lt->button_base_focused   = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.1f)); // Фон кнопки при наведении/фокусе (чуть темнее)
    lt->button_base_pressed   = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.30f)); // Фон кнопки в нажатом состоянии (значительно темнее)
    lt->button_base_disabled  = ColorToInt(Fade(GetColor(clr_button_base_normal_lt), 0.4f));   // Фон неактивной кнопки (блеклый)
    lt->text_on_button_normal = clr_text_on_button_normal_lt; // Цвет текста на кнопке в обычном состоянии
    lt->text_on_button_active = clr_text_on_button_active_lt; // Цвет текста на кнопке в нажатом/активном состоянии
    lt->button_text_disabled  = ColorToInt(Fade(GetColor(clr_text_on_button_normal_lt), 0.5f)); // Цвет текста на неактивной кнопке (блеклый)

    // Списки/Таблицы (LISTVIEW)
    lt->list_item_bg          = clr_list_item_bg_lt;          // Фон для обычных строк в таблице или списке
    lt->list_item_alt_bg      = ColorToInt(ColorBrightness(GetColor(clr_list_item_bg_lt), -0.05f)); // Альтернативный фон для строк (для чередования, если используется)
    lt->list_item_selected_bg = ColorToInt(ColorBrightness(GetColor(clr_list_item_bg_lt), -0.2f)); // Фон для выделенной строки
    lt->list_item_selected_text = clr_text_primary_lt;        // Цвет текста на выделенной строке
    lt->list_header_bg        = clr_header_toolbar_bg_lt;     // Фон для заголовка таблицы (если отличается от обычных строк)
                                                              // В твоем коде это был clr_button_base_normal_lt, если хочешь так, измени здесь.
    lt->list_selected_border  = clr_border_focused_accent_lt; // Цвет рамки вокруг выделенной строки (если есть)

    // Поля ввода (TEXTBOX)
    lt->textbox_bg_normal     = clr_textbox_bg_normal_lt;     // Фоновый цвет для полей ввода текста

    // Скроллбары (SCROLLBAR и SLIDER для бегунка)
    lt->scrollbar_track_bg    = ColorToInt(ColorBrightness(GetColor(clr_header_toolbar_bg_lt), -0.05f)); // Фон "дорожки" скроллбара
    lt->scrollbar_thumb_bg    = clr_button_base_normal_lt;    // Фон бегунка скроллбара в обычном состоянии
    lt->scrollbar_thumb_focused = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.1f)); // Фон бегунка при наведении/фокусе
    lt->scrollbar_thumb_pressed = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.2f)); // Фон бегунка при нажатии

    // Другие специфичные цвета
    lt->modal_overlay_color   = ColorToInt(Fade(BLACK, 0.65f)); // Цвет для полупрозрачного фона при открытии модальных диалогов
    lt->error_text_color      = ColorToInt(RED);                // Цвет для сообщений об ошибках
    lt->placeholder_text_color= ColorToInt(GRAY);               // Цвет для текста-заполнителя (например, "Нет данных")


    // --- ТЕМНАЯ ТЕМА (ID: DARK_THEME_ID) ---
    // Базовый вариант, который ты можешь настроить
    ThemeColors *dt = &app_themes[DARK_THEME_ID];

    // Основные цвета интерфейса
    dt->window_background     = 0x2b2b2bFF; // Темно-серый, почти черный фон окна
    dt->panel_background      = 0x2b2b2bFF; // Такой же для панелей
    dt->header_toolbar_bg     = 0x3c3f41FF; // Чуть светлее для тулбара и заголовков окон
    dt->text_primary          = 0xdededeFF; // Очень светло-серый (почти белый) основной текст
    dt->text_disabled         = 0x777777FF; // Средне-серый для неактивного текста
    dt->border_universal      = 0x4f5254FF; // Более светлая серая рамка на темном фоне
    dt->border_focused_accent = 0x0078d4FF; // Ярко-синий для акцента (как в VSCode)

    // Кнопки
    dt->button_base_normal    = 0x4a4d4fFF; // Темно-серая кнопка
    dt->text_on_button_normal = dt->text_primary;          // Текст на кнопке такой же, как основной
    dt->text_on_button_active = 0xFFFFFFFF; // Ярко-белый текст на нажатой кнопке для контраста
    dt->button_base_focused   = ColorToInt(ColorBrightness(GetColor(dt->button_base_normal), 0.1f));  // Фон кнопки при фокусе (чуть светлее)
    dt->button_base_pressed   = ColorToInt(ColorBrightness(GetColor(dt->button_base_normal), 0.25f)); // Фон кнопки при нажатии (еще светлее)
    dt->button_base_disabled  = ColorToInt(Fade(GetColor(dt->button_base_normal), 0.6f));    // Блеклый фон неактивной кнопки
    dt->button_text_disabled  = ColorToInt(Fade(GetColor(dt->text_on_button_normal), 0.5f));  // Блеклый текст неактивной кнопки

    // Списки/Таблицы
    dt->list_item_bg          = 0x3c3f41FF; // Фон строк таблицы (чуть светлее фона панели)
    dt->list_item_alt_bg      = ColorToInt(ColorBrightness(GetColor(dt->list_item_bg), 0.05f)); // Альтернативный фон строк (еще чуть светлее)
    dt->list_item_selected_bg = 0x005A9EFF; // Акцентный синий для выделенной строки
    dt->list_item_selected_text = 0xFFFFFFFF; // Белый текст на выделенной строке
    dt->list_header_bg        = dt->header_toolbar_bg; // Фон заголовка таблицы как у тулбара
    dt->list_selected_border  = dt->border_focused_accent; // Рамка выделенной строки

    // Поля ввода
    dt->textbox_bg_normal     = 0x313335FF; // Темный фон для поля ввода, чуть светлее основного фона

    // Скроллбары
    dt->scrollbar_track_bg    = ColorToInt(ColorBrightness(GetColor(dt->header_toolbar_bg), 0.05f)); // Дорожка скроллбара
    dt->scrollbar_thumb_bg    = dt->button_base_normal; // Бегунок как обычная кнопка
    dt->scrollbar_thumb_focused = ColorToInt(ColorBrightness(GetColor(dt->scrollbar_thumb_bg), 0.1f)); // Бегунок при фокусе
    dt->scrollbar_thumb_pressed = ColorToInt(ColorBrightness(GetColor(dt->scrollbar_thumb_bg), 0.2f)); // Бегунок при нажатии

    // Другие
    dt->modal_overlay_color   = ColorToInt(Fade(BLACK, 0.75f)); // Затемнение для модальных окон (чуть темнее, чем в светлой)
    dt->error_text_color      = 0xFF6060FF; // Светло-красный для ошибок
    dt->placeholder_text_color= 0x888888FF; // Темно-серый для текста-заполнителя
}   