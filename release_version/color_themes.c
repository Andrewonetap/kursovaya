// color_themes.c
// файл с темами короче

#include "color_themes.h" // свой заголовок
#include "raylib.h"       // raylib нужен для цветов

// массив со всеми темами
ThemeColors app_themes[TOTAL_THEMES];

// функция для создания всех тем
void InitializeAllApplicationThemes(void) {

    // --- СВЕТЛАЯ ТЕМА (ID: LIGHT_THEME_ID) ---
    ThemeColors *lt = &app_themes[LIGHT_THEME_ID]; // указатель на светлую тему для удобства

    // -- базовые цвета для светлой темы (взяты из старого кода) --
    unsigned int clr_panel_background_lt     = 0xf1f2f2FF; // фон панелей светло-серый
    unsigned int clr_list_item_bg_lt         = 0xe3e8ffFF; // фон строк в таблице светло-голубой
    unsigned int clr_text_primary_lt         = 0x303032FF; // основной текст темно-серый
    unsigned int clr_border_universal_lt     = 0x080808FF; // общая рамка почти черная
    unsigned int clr_button_base_normal_lt   = 0xdcdbe1FF; // кнопки светло-серые
    // для тулбара был 0xe9ecefff вот его и берем
    unsigned int clr_header_toolbar_bg_lt    = 0xE9ECEFFF; // фон тулбара светлый серо-голубой
    unsigned int clr_text_on_button_normal_lt= clr_text_primary_lt; // текст на кнопке как основной
    unsigned int clr_text_on_button_active_lt= clr_panel_background_lt; // текст на нажатой кнопке как фон панели (светлый)
    unsigned int clr_text_disabled_lt        = 0xA0A0A0FF; // неактивный текст средне-серый
    unsigned int clr_border_focused_accent_lt= 0x007BFFFF; // рамка активного элемента ярко-синяя
    unsigned int clr_textbox_bg_normal_lt    = 0xFFFFFFFF; // фон поля ввода белый

    // -- заполняем поля структуры для СВЕТЛОЙ ТЕМЫ --

    // основные цвета интерфейса
    lt->window_background     = clr_panel_background_lt;      // фон всего окна
    lt->panel_background      = clr_panel_background_lt;      // фон панелей диалогов
    lt->header_toolbar_bg     = clr_header_toolbar_bg_lt;     // фон тулбара и заголовков окон
    lt->text_primary          = clr_text_primary_lt;          // основной цвет текста
    lt->text_disabled         = clr_text_disabled_lt;         // цвет неактивного текста
    lt->border_universal      = clr_border_universal_lt;      // цвет рамок (кнопки панели)
    lt->border_focused_accent = clr_border_focused_accent_lt; // цвет рамки активного элемента

    // кнопки
    lt->button_base_normal    = clr_button_base_normal_lt;    // фон обычной кнопки
    lt->button_base_focused   = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.1f)); // фон кнопки в фокусе (темнее)
    lt->button_base_pressed   = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.30f)); // фон нажатой кнопки (сильно темнее)
    lt->button_base_disabled  = ColorToInt(Fade(GetColor(clr_button_base_normal_lt), 0.4f));   // фон неактивной кнопки (бледнее)
    lt->text_on_button_normal = clr_text_on_button_normal_lt; // текст на обычной кнопке
    lt->text_on_button_active = clr_text_on_button_active_lt; // текст на нажатой кнопке
    lt->button_text_disabled  = ColorToInt(Fade(GetColor(clr_text_on_button_normal_lt), 0.5f)); // текст на неактивной кнопке (бледнее)

    // списки таблицы
    lt->list_item_bg          = clr_list_item_bg_lt;          // фон строк таблицы
    lt->list_item_alt_bg      = ColorToInt(ColorBrightness(GetColor(clr_list_item_bg_lt), -0.05f)); // фон для чередования строк
    lt->list_item_selected_bg = ColorToInt(ColorBrightness(GetColor(clr_list_item_bg_lt), -0.2f)); // фон выделенной строки
    lt->list_item_selected_text = clr_text_primary_lt;        // текст на выделенной строке
    lt->list_header_bg        = clr_header_toolbar_bg_lt;     // фон заголовка таблицы
    // старый код для заголовка использовал clr_button_base_normal_lt если надо верни
    lt->list_selected_border  = clr_border_focused_accent_lt; // рамка выделенной строки

    // поля ввода
    lt->textbox_bg_normal     = clr_textbox_bg_normal_lt;     // фон поля ввода

    // скроллбары
    lt->scrollbar_track_bg    = ColorToInt(ColorBrightness(GetColor(clr_header_toolbar_bg_lt), -0.05f)); // фон дорожки скролла
    lt->scrollbar_thumb_bg    = clr_button_base_normal_lt;    // фон бегунка скролла
    lt->scrollbar_thumb_focused = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.1f)); // бегунок в фокусе
    lt->scrollbar_thumb_pressed = ColorToInt(ColorBrightness(GetColor(clr_button_base_normal_lt), -0.2f)); // нажатый бегунок

    // другие цвета
    lt->modal_overlay_color   = ColorToInt(Fade(BLACK, 0.65f)); // затемнение для модалок
    lt->error_text_color      = ColorToInt(RED);                // цвет ошибок
    lt->placeholder_text_color= ColorToInt(GRAY);               // цвет для текста-заполнителя


    // --- ТЕМНАЯ ТЕМА (ID: DARK_THEME_ID) ---
    ThemeColors *dt = &app_themes[DARK_THEME_ID]; // указатель на темную тему

    // -- палитра 2 глубокий океан --
    // основные цвета интерфейса
    dt->window_background     = 0x1A1D2BFF; // фон окна очень темно-сине-серый
    dt->panel_background      = 0x1A1D2BFF; // фон панелей такой же
    dt->header_toolbar_bg     = 0x24293EFF; // фон тулбара чуть светлее сине-серый
    dt->text_primary          = 0xD0D0FFFF; // основной текст светло-лавандовый
    dt->text_disabled         = 0x606880FF; // неактивный текст средне-серо-синий
    dt->border_universal      = 0x363C58FF; // рамка сине-серая
    dt->border_focused_accent = 0x007ACCFF; // рамка активного элемента ярко-синяя (как vscode)

    // кнопки
    dt->button_base_normal    = 0x363C58FF; // фон кнопки сине-серый
    dt->text_on_button_normal = dt->text_primary; // текст на кнопке как основной
    dt->text_on_button_active = 0xFFFFFFFF; // текст на активной кнопке белый
    dt->button_base_focused   = ColorToInt(ColorBrightness(GetColor(dt->button_base_normal), 0.1f)); // фон в фокусе (светлее)
    dt->button_base_pressed   = ColorToInt(ColorBrightness(GetColor(dt->button_base_normal), 0.25f)); // фон нажатой (еще светлее)
    dt->button_base_disabled  = ColorToInt(Fade(GetColor(dt->button_base_normal), 0.6f)); // фон неактивной (бледнее)
    dt->button_text_disabled  = ColorToInt(Fade(GetColor(dt->text_on_button_normal), 0.5f)); // текст на неактивной (бледнее)

    // списки таблицы
    dt->list_item_bg          = 0x24293EFF; // фон строк как у тулбара
    dt->list_item_alt_bg      = ColorToInt(ColorBrightness(GetColor(dt->list_item_bg), 0.05f)); // чередование строк
    dt->list_item_selected_bg = 0x005A9EFF; // фон выделенной строки акцентный синий
    dt->list_item_selected_text = 0xFFFFFFFF; // текст на выделенной строке белый
    dt->list_header_bg        = dt->header_toolbar_bg; // фон заголовка таблицы
    dt->list_selected_border  = dt->border_focused_accent; // рамка выделенной строки

    // поля ввода
    dt->textbox_bg_normal     = 0x202433FF; // темный фон для поля ввода

    // скроллбары
    dt->scrollbar_track_bg    = ColorToInt(ColorBrightness(GetColor(dt->header_toolbar_bg), -0.02f)); // дорожка скролла
    dt->scrollbar_thumb_bg    = dt->button_base_normal; // бегунок как кнопка
    dt->scrollbar_thumb_focused = ColorToInt(ColorBrightness(GetColor(dt->scrollbar_thumb_bg), 0.1f)); // бегунок в фокусе
    dt->scrollbar_thumb_pressed = ColorToInt(ColorBrightness(GetColor(dt->scrollbar_thumb_bg), 0.2f)); // нажатый бегунок

    // другие
    dt->modal_overlay_color   = ColorToInt(Fade(BLACK, 0.80f)); // затемнение для модалок (плотнее)
    dt->error_text_color      = 0xFF5555FF; // цвет ошибок ярко-красный
    dt->placeholder_text_color= 0x505870FF; // плейсхолдер темно-серо-синий

    // если будут другие темы то сюда их пихать по аналогии
}