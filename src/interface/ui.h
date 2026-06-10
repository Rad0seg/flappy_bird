#pragma once

#include <windows.h>
#include "../logic/game_logic.h"

// Контекст пользовательского интерфейса
typedef struct {
    HWND hwnd;              // Окно приложения
    HDC hdc;                // Контекст устройства основной
    HDC back_buffer;        // Контекст буфера для двойной буферизации
    HBITMAP back_bitmap;    // Битмап буфера
    int width;              // Ширина окна
    int height;             // Высота окна
    LARGE_INTEGER frequency;// Частота счетчика производительности
    LARGE_INTEGER last_time;// Время последнего кадра
    HFONT title_font;       // Шрифт заголовков
    HFONT score_font;       // Шрифт счета (большой)
    HFONT text_font;        // Шрифт обычного текста
} UIContext;

// Инициализация окна и контекстов рисования
int ui_init(UIContext* ctx, HINSTANCE hInstance);

// Главный игровой цикл с обработкой сообщений
int ui_run(UIContext* ctx, GameState* state);

// Отрисовка одного кадра
void ui_render(UIContext* ctx, const GameState* state);

// Освобождение ресурсов GDI
void ui_cleanup(UIContext* ctx);