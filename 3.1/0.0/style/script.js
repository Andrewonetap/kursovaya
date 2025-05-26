document.addEventListener('DOMContentLoaded', () => {
    const themeButtons = document.querySelectorAll('.theme-switcher button');
    const body = document.body;
    const THEME_STORAGE_KEY = 'selectedTheme'; // Ключ для хранения темы в localStorage

    // Функция для применения темы
    const applyTheme = (themeName) => {
        // Удаляем все существующие классы тем
        body.classList.remove('theme-light', 'theme-dark', 'theme-protanopia', 'theme-deuteranopia', 'theme-tritanopia', 'theme-monochromacy');

        // Добавляем класс для новой темы (если это не 'light', т.к. светлая тема по умолчанию)
        if (themeName && themeName !== 'light') {
            body.classList.add(`theme-${themeName}`);
        } else {
             // Убедимся, что для 'light' темы нет дополнительных классов
             // (хотя remove выше уже это делает)
        }

        // Сохраняем выбранную тему в localStorage
        localStorage.setItem(THEME_STORAGE_KEY, themeName);
    };

    // При загрузке страницы проверяем, есть ли сохраненная тема
    const storedTheme = localStorage.getItem(THEME_STORAGE_KEY);
    if (storedTheme) {
        applyTheme(storedTheme);
    } else {
        // Если нет, применяем светлую тему по умолчанию
        applyTheme('light');
    }

    // Добавляем слушатели событий на кнопки смены темы
    themeButtons.forEach(button => {
        button.addEventListener('click', () => {
            const themeName = button.dataset.theme; // Получаем название темы из data-атрибута
            applyTheme(themeName); // Применяем тему
        });
    });
});