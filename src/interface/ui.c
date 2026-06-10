#include "ui.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char* WINDOW_CLASS_NAME = "FlappyBirdWindowClass";
static const char* WINDOW_TITLE = "Flappy Bird";

static GameState* g_game_state = NULL;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// Интерполяция между двумя цветами (для плавных переходов)
static COLORREF interpolate_color(COLORREF c1, COLORREF c2, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * t);
    int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * t);
    int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * t);
    return RGB(r, g, b);
}

// Получить цвета неба для текущего времени суток
static void get_sky_colors(TimeOfDay tod, float transition, COLORREF* top, COLORREF* bottom) {
    COLORREF tops[]    = { RGB(135,206,250), RGB(255,140,100), RGB(20,24,82) };
    COLORREF bottoms[] = { RGB(180,220,255), RGB(255,200,150), RGB(50,60,120) };

    int cur = (int)tod;
    int next = (cur + 1) % 3;

    *top    = interpolate_color(tops[cur],    tops[next],    transition);
    *bottom = interpolate_color(bottoms[cur], bottoms[next], transition);
}

// Заполнение прямоугольника цветом
static void fill_rect_color(HDC hdc, int x, int y, int w, int h, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT r = { x, y, x + w, y + h };
    FillRect(hdc, &r, brush);
    DeleteObject(brush);
}

// Отрисовка прямоугольника с контуром
static void draw_outlined_rect(HDC hdc, int x, int y, int w, int h,
                                COLORREF fill, COLORREF outline) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 2, outline);
    HBRUSH old_b = (HBRUSH)SelectObject(hdc, brush);
    HPEN old_p = (HPEN)SelectObject(hdc, pen);

    Rectangle(hdc, x, y, x + w, y + h);

    SelectObject(hdc, old_b);
    SelectObject(hdc, old_p);
    DeleteObject(brush);
    DeleteObject(pen);
}

// Отрисовка птицы пиксель-артом
static void draw_pixel_bird(HDC hdc, const Bird* bird) {
    int bx = (int)bird->x;
    int by = (int)bird->y;
    float scale = bird->size_multiplier;
    int p = (int)(4.0f * scale);
    if (p < 2) p = 2;

    COLORREF body = bird->alive ? RGB(255,220,0) : RGB(150,150,150);
    COLORREF wing = bird->alive ? RGB(255,180,0) : RGB(120,120,120);

    // Рисуем тело птицы построчно
    draw_outlined_rect(hdc, bx+p*2, by,       p*4, p, body, RGB(0,0,0));
    draw_outlined_rect(hdc, bx+p,   by+p,     p*6, p, body, RGB(0,0,0));
    draw_outlined_rect(hdc, bx,     by+p*2,   p*7, p, body, RGB(0,0,0));
    draw_outlined_rect(hdc, bx,     by+p*3,   p*7, p, body, RGB(0,0,0));
    draw_outlined_rect(hdc, bx+p,   by+p*4,   p*6, p, body, RGB(0,0,0));
    draw_outlined_rect(hdc, bx+p*2, by+p*5,   p*4, p, body, RGB(0,0,0));

    // Глаз птицы
    fill_rect_color(hdc, bx+p*5, by+p, p, p, RGB(255,255,255));
    fill_rect_color(hdc, bx+p*5+(p/3), by+p, p/2, p/2, RGB(0,0,0));

    // Клюв
    draw_outlined_rect(hdc, bx+p*7, by+p*2, p*2, p, RGB(255,140,0), RGB(0,0,0));

    // Крыло (анимируется в зависимости от wing_state)
    if (bird->wing_state == 0) {
        draw_outlined_rect(hdc, bx+p*2, by+p*3, p*2, p, wing, RGB(0,0,0));
    } else {
        draw_outlined_rect(hdc, bx+p*2, by+p*2, p*2, p, wing, RGB(0,0,0));
    }
}

// Отрисовка трубы (верхней или нижней)
static void draw_pixel_pipe(HDC hdc, int x, int top_height, int is_bottom, float gap) {
    COLORREF pipe_body = RGB(50,200,80);
    COLORREF pipe_dark = RGB(30,150,60);
    COLORREF pipe_hi   = RGB(80,230,110);

    int pipe_y, pipe_h;
    if (is_bottom) {
        pipe_y = (int)(top_height + gap);
        pipe_h = WINDOW_HEIGHT - 80 - pipe_y;
    } else {
        pipe_y = 0;
        pipe_h = top_height;
    }

    if (pipe_h <= 0) return;

    // Основное тело трубы
    draw_outlined_rect(hdc, x, pipe_y, PIPE_WIDTH, pipe_h, pipe_body, RGB(0,0,0));

    // Блик слева для 3D эффекта
    fill_rect_color(hdc, x + 6, pipe_y + 2, 10, pipe_h - 4, pipe_hi);
    // Тень справа
    fill_rect_color(hdc, x + PIPE_WIDTH - 16, pipe_y + 2, 10, pipe_h - 4, pipe_dark);

    // Крышка трубы
    int cap_h = 24;
    int cap_w = PIPE_WIDTH + 16;
    int cap_x = x - 8;
    int cap_y = is_bottom ? pipe_y : top_height - cap_h;

    draw_outlined_rect(hdc, cap_x, cap_y, cap_w, cap_h, pipe_dark, RGB(0,0,0));
    fill_rect_color(hdc, cap_x + 4, cap_y + 4, cap_w - 8, cap_h / 3, pipe_hi);
}

// Отрисовка паверапа (буффа/дебаффа)
static void draw_powerup(HDC hdc, const PowerUp* pu, float game_time) {
    if (!pu->active) return;

    // Покачивание паверапа
    float bob = sinf(pu->bob_time * 5.0f) * 6.0f;
    int px = (int)pu->x;
    int py = (int)(pu->y + bob);

    COLORREF fill, outline;
    const char* symbol;

    // Определение цвета и символа по типу баффа
    if (pu->type == POWERUP_SMALL_BIRD) {
        fill = RGB(100, 255, 100);
        outline = RGB(0, 150, 0);
        symbol = "S";
    } else if (pu->type == POWERUP_CLEAR_PIPES) {
        fill = RGB(100, 200, 255);
        outline = RGB(0, 100, 200);
        symbol = "C";
    } else {
        fill = RGB(255, 100, 100);
        outline = RGB(180, 0, 0);
        symbol = "H";
    }

    // Пульсация размера паверапа
    float pulse = sinf(game_time * 4.0f) * 2.0f;
    int size = POWERUP_SIZE + (int)pulse;

    draw_outlined_rect(hdc, px, py, size, size, fill, outline);

    // Вывод буквы баффа
    HFONT font = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0,0,0));
    TextOutA(hdc, px + size / 4, py + size / 4, symbol, 1);
    SelectObject(hdc, old);
    DeleteObject(font);
}

// Отрисовка полоски активного эффекта (баффа/дебаффа)
static void draw_effect_hud(HDC hdc, const GameState* state, HFONT text_font) {
    if (state->active_effect.type == POWERUP_NONE) return;

    const char* name = powerup_name(state->active_effect.type);
    COLORREF bg_color;
    
    // Определение цвета полоски по типу эффекта
    if (state->active_effect.type == POWERUP_SMALL_BIRD) {
        bg_color = RGB(0, 150, 0);
    } else if (state->active_effect.type == POWERUP_CLEAR_PIPES) {
        bg_color = RGB(0, 100, 200);
    } else {
        bg_color = RGB(180, 0, 0);
    }

    // Масштабирование полоски по оставшемуся времени
    int bar_w = (int)(200.0f * (state->active_effect.time_left / 5.0f));
    if (bar_w < 0) bar_w = 0;
    if (bar_w > 200) bar_w = 200;

    // Фон полоски
    fill_rect_color(hdc, WINDOW_WIDTH / 2 - 105, 10, 210, 30, RGB(50,50,50));
    // Активная часть
    fill_rect_color(hdc, WINDOW_WIDTH / 2 - 100, 12, bar_w, 26, bg_color);

    // Название эффекта
    HFONT old = (HFONT)SelectObject(hdc, text_font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255,255,255));
    TextOutA(hdc, WINDOW_WIDTH / 2 - 60, 14, name, (int)strlen(name));
    SelectObject(hdc, old);
}

// Отрисовка плавного облака
static void draw_smooth_cloud(HDC hdc, float x, float y, float size, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));

    int cx = (int)x;
    int cy = (int)y;
    int r = (int)size;

    // Несколько кругов образуют облако
    Ellipse(hdc, cx - r, cy - r/2, cx + r, cy + r/2);
    Ellipse(hdc, cx - r/2, cy - r, cx + r/2, cy + r/2);
    Ellipse(hdc, cx, cy - r/2, cx + r*2, cy + r/2);
    Ellipse(hdc, cx + r/2, cy - r/3, cx + r*3/2, cy + r/3);

    SelectObject(hdc, old_brush);
    DeleteObject(brush);
}

// Отрисовка мерцающих звезд для ночного неба
static void draw_stars(HDC hdc, float game_time) {
    static const int star_pos[][2] = {
        {50,50}, {150,80}, {300,40}, {450,90}, {600,60}, {700,100},
        {100,150}, {400,130}, {650,170}, {200,200}, {500,180}, {750,150},
        {350,70}, {550,110}, {250,160}
    };

    for (int i = 0; i < 15; i++) {
        // Синусоидальное мерцание каждой звезды
        float twinkle = sinf(game_time * 2.0f + i * 0.7f) * 0.5f + 0.5f;
        if (twinkle > 0.3f) {
            int bright = (int)(200 + twinkle * 55);
            fill_rect_color(hdc, star_pos[i][0], star_pos[i][1], 4, 4,
                            RGB(bright, bright, bright));
        }
    }
}

// Отрисовка земли с кустиками (синхронный параллакс)
static void draw_ground(HDC hdc, int width, int height, float scroll_offset) {
    int ground_y = height - 80;

    // Трава
    fill_rect_color(hdc, 0, ground_y, width, 30, RGB(100,200,80));

    // Кустики (движутся со скоростью игры)
    int off = (int)scroll_offset;
    for (int i = -40 - (off % 40); i < width + 40; i += 40) {
        fill_rect_color(hdc, i,     ground_y + 12, 8, 8, RGB(70,170,50));
        fill_rect_color(hdc, i + 4, ground_y + 6,  8, 8, RGB(80,180,60));
        fill_rect_color(hdc, i + 8, ground_y + 12, 8, 8, RGB(70,170,50));
    }

    // Земля
    fill_rect_color(hdc, 0, ground_y + 30, width, 50, RGB(139,90,43));

    // Камушки на земле
    for (int i = -30 - (off % 30); i < width + 30; i += 30) {
        fill_rect_color(hdc, i, ground_y + 35, 6, 6, RGB(120,70,30));
        fill_rect_color(hdc, i + 15, ground_y + 50, 5, 5, RGB(110,65,28));
    }
}

// Отрисовка большой цифры для обратного отсчета
static void draw_countdown(HDC hdc, int width, int height, int value) {
    char text[4];
    sprintf(text, "%d", value);

    HFONT huge = CreateFontA(200, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    HFONT old = (HFONT)SelectObject(hdc, huge);
    SetBkMode(hdc, TRANSPARENT);

    // Тень для читаемости
    SetTextColor(hdc, RGB(50,50,50));
    TextOutA(hdc, width/2 - 50 + 5, height/2 - 100 + 5, text, (int)strlen(text));

    // Основной текст
    SetTextColor(hdc, RGB(255,255,0));
    TextOutA(hdc, width/2 - 50, height/2 - 100, text, (int)strlen(text));

    SelectObject(hdc, old);
    DeleteObject(huge);
}

// Создание совместимого буфера для двойной буферизации
static int create_back_buffer(UIContext* ctx) {
    ctx->hdc = GetDC(ctx->hwnd);
    if (!ctx->hdc) return 0;

    ctx->back_buffer = CreateCompatibleDC(ctx->hdc);
    if (!ctx->back_buffer) {
        ReleaseDC(ctx->hwnd, ctx->hdc);
        return 0;
    }

    ctx->back_bitmap = CreateCompatibleBitmap(ctx->hdc, ctx->width, ctx->height);
    if (!ctx->back_bitmap) {
        DeleteDC(ctx->back_buffer);
        ReleaseDC(ctx->hwnd, ctx->hdc);
        return 0;
    }

    SelectObject(ctx->back_buffer, ctx->back_bitmap);
    return 1;
}

// Инициализация окна и контекстов рисования
int ui_init(UIContext* ctx, HINSTANCE hInstance) {
    memset(ctx, 0, sizeof(UIContext));

    // Точность системного таймера 1 мс
    // (иначе таймауты ожидания кратны ~15.6 мс и кадры неравномерны)
    timeBeginPeriod(1);

    ctx->width = WINDOW_WIDTH;
    ctx->height = WINDOW_HEIGHT;

    // Регистрация класса окна
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) return 0;

    // Расчет размера окна с учетом рамок
    RECT wr = {0, 0, ctx->width, ctx->height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // Создание окна
    ctx->hwnd = CreateWindowExA(0, WINDOW_CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);

    if (!ctx->hwnd) return 0;
    
    // Создание буфера для отрисовки
    if (!create_back_buffer(ctx)) { 
        DestroyWindow(ctx->hwnd); 
        return 0; 
    }

    // Создание шрифтов для разных целей
    ctx->title_font = CreateFontA(60,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Courier New");
    
    ctx->score_font = CreateFontA(80,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Courier New");
    
    ctx->text_font = CreateFontA(22,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Courier New");

    // Инициализация таймера высокой точности
    QueryPerformanceFrequency(&ctx->frequency);
    QueryPerformanceCounter(&ctx->last_time);

    ShowWindow(ctx->hwnd, SW_SHOW);
    UpdateWindow(ctx->hwnd);
    return 1;
}

// Главная функция отрисовки кадра
void ui_render(UIContext* ctx, const GameState* state) {
    // Отрисовка неба с градиентом
    COLORREF sky_top, sky_bottom;
    get_sky_colors(state->time_of_day, state->time_transition, &sky_top, &sky_bottom);

    int sky_h = ctx->height - 80;
    for (int y = 0; y < sky_h; y += 4) {
        float t = (float)y / (float)sky_h;
        COLORREF c = interpolate_color(sky_top, sky_bottom, t);
        fill_rect_color(ctx->back_buffer, 0, y, ctx->width, 4, c);
    }

    // Звезды ночью
    if (state->time_of_day == TIME_NIGHT) {
        draw_stars(ctx->back_buffer, state->game_time);
    }

    // Облака (независимое плавное движение)
    COLORREF cloud_c = RGB(255,255,255);
    if (state->time_of_day == TIME_SUNSET) {
        cloud_c = RGB(255,220,220);
    } else if (state->time_of_day == TIME_NIGHT) {
        cloud_c = RGB(120,120,160);
    }

    float cloud_time = state->game_time * 15.0f;

    float c1_x = 150.0f + sinf(cloud_time * 0.02f) * 100.0f;
    float c1_y = 80.0f + cosf(cloud_time * 0.03f) * 20.0f;
    draw_smooth_cloud(ctx->back_buffer, c1_x, c1_y, 35.0f, cloud_c);

    float c2_x = 450.0f + sinf(cloud_time * 0.025f) * 120.0f;
    float c2_y = 140.0f + cosf(cloud_time * 0.022f) * 25.0f;
    draw_smooth_cloud(ctx->back_buffer, c2_x, c2_y, 45.0f, cloud_c);

    float c3_x = 600.0f + sinf(cloud_time * 0.018f) * 90.0f;
    float c3_y = 60.0f + cosf(cloud_time * 0.028f) * 15.0f;
    draw_smooth_cloud(ctx->back_buffer, c3_x, c3_y, 30.0f, cloud_c);

    float c4_x = 250.0f + sinf(cloud_time * 0.022f) * 110.0f;
    float c4_y = 180.0f + cosf(cloud_time * 0.02f) * 30.0f;
    draw_smooth_cloud(ctx->back_buffer, c4_x, c4_y, 40.0f, cloud_c);

    // Отрисовка труб
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!state->pipes[i].active) continue;
        
        int px = (int)state->pipes[i].x;
        int ph = (int)state->pipes[i].top_height;
        
        draw_pixel_pipe(ctx->back_buffer, px, ph, 0, state->current_pipe_gap);
        draw_pixel_pipe(ctx->back_buffer, px, ph, 1, state->current_pipe_gap);
    }

    // Отрисовка паверапов
    for (int i = 0; i < MAX_POWERUPS; i++) {
        draw_powerup(ctx->back_buffer, &state->powerups[i], state->game_time);
    }

    // Отрисовка земли
    draw_ground(ctx->back_buffer, ctx->width, ctx->height, state->scroll_offset);

    // Отрисовка птицы
    draw_pixel_bird(ctx->back_buffer, &state->bird);

    // HUD во время игры (счет и полоска эффекта)
    if (state->state == GAME_STATE_PLAYING || 
        state->state == GAME_STATE_PAUSED ||
        state->state == GAME_STATE_COUNTDOWN) {

        draw_effect_hud(ctx->back_buffer, state, ctx->text_font);

        char score_text[32];
        sprintf(score_text, "%d", state->score);

        HFONT old_font = (HFONT)SelectObject(ctx->back_buffer, ctx->score_font);
        SetBkMode(ctx->back_buffer, TRANSPARENT);
        
        // Тень счета
        SetTextColor(ctx->back_buffer, RGB(0,0,0));
        TextOutA(ctx->back_buffer, ctx->width/2 - 20 + 4, 54, score_text, (int)strlen(score_text));
        
        // Основной счет
        SetTextColor(ctx->back_buffer, RGB(255,255,255));
        TextOutA(ctx->back_buffer, ctx->width/2 - 20, 50, score_text, (int)strlen(score_text));
        
        SelectObject(ctx->back_buffer, old_font);
    }

    // Экран меню
    if (state->state == GAME_STATE_MENU) {
        draw_outlined_rect(ctx->back_buffer, ctx->width/2-230, 80, 460, 440,
                           RGB(255,255,200), RGB(0,0,0));

        HFONT old_f = (HFONT)SelectObject(ctx->back_buffer, ctx->title_font);
        SetBkMode(ctx->back_buffer, TRANSPARENT);
        
        // Заголовок
        SetTextColor(ctx->back_buffer, RGB(255,100,0));
        TextOutA(ctx->back_buffer, ctx->width/2-180, 105, "FLAPPY", 6);
        TextOutA(ctx->back_buffer, ctx->width/2-180, 165, " BIRD", 5);

        // Инструкции
        SelectObject(ctx->back_buffer, ctx->text_font);
        SetTextColor(ctx->back_buffer, RGB(50,50,50));

        TextOutA(ctx->back_buffer, ctx->width/2-130, 245, "SPACE - Start & Jump", 20);
        TextOutA(ctx->back_buffer, ctx->width/2-80,  275, "P - Pause", 9);
        TextOutA(ctx->back_buffer, ctx->width/2-80,  305, "ESC - Exit", 10);
        
        // Разделитель
        SetTextColor(ctx->back_buffer, RGB(100,100,100));
        TextOutA(ctx->back_buffer, ctx->width/2-100, 340, "== POWER-UPS ==", 15);
        
        // Баффы
        SetTextColor(ctx->back_buffer, RGB(0,150,0));
        TextOutA(ctx->back_buffer, ctx->width/2-130, 370, "S = Small (good)", 16);
        
        SetTextColor(ctx->back_buffer, RGB(0,100,200));
        TextOutA(ctx->back_buffer, ctx->width/2-130, 395, "C = Jump Points (BEST!)", 24);
        
        // Дебаффы
        SetTextColor(ctx->back_buffer, RGB(180,0,0));
        TextOutA(ctx->back_buffer, ctx->width/2-130, 425, "H = Heavy (bad)", 15);

        // Подсказка
        SetTextColor(ctx->back_buffer, RGB(100,100,100));
        const char* hint = "Speed = 3.0, Gets +0.5 per 10 pipes";
        TextOutA(ctx->back_buffer, ctx->width/2-160, 455, hint, (int)strlen(hint));

        // Лучший счет
        char best[64];
        sprintf(best, "Best: %d", state->best_score);
        SetTextColor(ctx->back_buffer, RGB(200,150,0));
        TextOutA(ctx->back_buffer, ctx->width/2-60, 490, best, (int)strlen(best));

        SelectObject(ctx->back_buffer, old_f);
    }

    // Экран паузы
    if (state->state == GAME_STATE_PAUSED) {
        // Затемнение шахматным паттерном
        for (int y = 0; y < ctx->height; y += 4) {
            for (int x = 0; x < ctx->width; x += 4) {
                if ((x + y) % 8 == 0) {
                    fill_rect_color(ctx->back_buffer, x, y, 4, 4, RGB(0,0,0));
                }
            }
        }

        HFONT old_f = (HFONT)SelectObject(ctx->back_buffer, ctx->title_font);
        SetBkMode(ctx->back_buffer, TRANSPARENT);
        
        SetTextColor(ctx->back_buffer, RGB(255,255,0));
        TextOutA(ctx->back_buffer, ctx->width/2-120, ctx->height/2-50, "PAUSED", 6);

        SelectObject(ctx->back_buffer, ctx->text_font);
        SetTextColor(ctx->back_buffer, RGB(255,255,255));
        const char* msg = "Press P to continue";
        TextOutA(ctx->back_buffer, ctx->width/2-120, ctx->height/2+50, msg, (int)strlen(msg));
        
        SelectObject(ctx->back_buffer, old_f);
    }

    // Обратный отсчет перед началом
    if (state->state == GAME_STATE_COUNTDOWN) {
        draw_countdown(ctx->back_buffer, ctx->width, ctx->height, state->countdown_value);
    }

    // Экран проигрыша
    if (state->state == GAME_STATE_GAME_OVER) {
        draw_outlined_rect(ctx->back_buffer, ctx->width/2-200, 130, 400, 320,
                           RGB(255,250,220), RGB(0,0,0));

        HFONT old_f = (HFONT)SelectObject(ctx->back_buffer, ctx->title_font);
        SetBkMode(ctx->back_buffer, TRANSPARENT);
        
        // Заголовок Game Over
        SetTextColor(ctx->back_buffer, RGB(220,20,60));
        TextOutA(ctx->back_buffer, ctx->width/2-150, 155, "GAME", 4);
        TextOutA(ctx->back_buffer, ctx->width/2-150, 215, "OVER", 4);

        // Результаты
        SelectObject(ctx->back_buffer, ctx->text_font);
        SetTextColor(ctx->back_buffer, RGB(50,50,50));

        char buf[64];
        sprintf(buf, "Score: %d", state->score);
        TextOutA(ctx->back_buffer, ctx->width/2-70, 295, buf, (int)strlen(buf));

        sprintf(buf, "Best:  %d", state->best_score);
        TextOutA(ctx->back_buffer, ctx->width/2-70, 325, buf, (int)strlen(buf));

        // Кнопка рестарта
        draw_outlined_rect(ctx->back_buffer, ctx->width/2-110, 370, 220, 50,
                           RGB(100,200,100), RGB(0,100,0));

        SetTextColor(ctx->back_buffer, RGB(255,255,255));
        const char* restart = "R - Restart";
        TextOutA(ctx->back_buffer, ctx->width/2-70, 383, restart, (int)strlen(restart));

        SelectObject(ctx->back_buffer, old_f);
    }

    // Копирование буфера на экран (двойная буферизация)
    BitBlt(ctx->hdc, 0, 0, ctx->width, ctx->height, ctx->back_buffer, 0, 0, SRCCOPY);
}

// Главный игровой цикл с фиксированным шагом логики (60 обновлений/сек).
// Sleep не используется: ожидание следующего тика выполняется через
// MsgWaitForMultipleObjects, которое одновременно "просыпается" при
// приходе сообщений Windows, поэтому ввод обрабатывается без задержки.
int ui_run(UIContext* ctx, GameState* state) {
    g_game_state = state;

    const float FIXED_DT = 1.0f / 60.0f;  // фиксированный шаг логики
    float accumulator = 0.0f;             // накопитель необработанного времени

    MSG msg;
    msg.wParam = 0;
    int running = 1;

    // Сброс отметки времени, чтобы первый кадр не получил огромный dt
    QueryPerformanceCounter(&ctx->last_time);

    while (running) {
        // Обработка всех сообщений Windows (неблокирующая)
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running) break;

        // Реальное время, прошедшее с прошлой итерации
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float frame_time = (float)(now.QuadPart - ctx->last_time.QuadPart) /
                           (float)ctx->frequency.QuadPart;
        ctx->last_time = now;

        // Защита от "прыжка времени" (перетаскивание окна, лаг системы):
        // не даем логике выполнить больше 15 шагов за одну итерацию
        if (frame_time > 0.25f) frame_time = 0.25f;

        accumulator += frame_time;

        // Логика выполняется фиксированными шагами, сколько накопилось.
        // Если кадр занял 1/30 c - выполнится 2 шага, если 1/120 c - 0 или 1.
        // Скорость игры одинакова на любом железе независимо от FPS.
        while (accumulator >= FIXED_DT) {
            game_update(state, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // Отрисовка текущего состояния (один раз за итерацию)
        ui_render(ctx, state);

        // Ожидание до следующего шага логики БЕЗ Sleep:
        // поток блокируется до истечения таймаута ЛИБО до прихода
        // любого сообщения (нажатие клавиши разбудит цикл мгновенно)
        float wait_s = FIXED_DT - accumulator;
        if (wait_s > 0.0f) {
            DWORD wait_ms = (DWORD)(wait_s * 1000.0f);
            if (wait_ms > 0) {
                MsgWaitForMultipleObjects(0, NULL, FALSE, wait_ms, QS_ALLINPUT);
            }
        }
    }

    return (int)msg.wParam;
}

// Очистка всех ресурсов GDI
void ui_cleanup(UIContext* ctx) {
    if (ctx->title_font)  DeleteObject(ctx->title_font);
    if (ctx->score_font)  DeleteObject(ctx->score_font);
    if (ctx->text_font)   DeleteObject(ctx->text_font);
    if (ctx->back_bitmap) DeleteObject(ctx->back_bitmap);
    if (ctx->back_buffer) DeleteDC(ctx->back_buffer);
    if (ctx->hdc)         ReleaseDC(ctx->hwnd, ctx->hdc);
    if (ctx->hwnd)        DestroyWindow(ctx->hwnd);

    // Парный вызов к timeBeginPeriod из ui_init
    timeEndPeriod(1);
}

// Оконная процедура для обработки сообщений Win32
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (g_game_state) {
                if (wparam == VK_SPACE) {
                    game_jump(g_game_state);
                } else if (wparam == 'P' || wparam == 'p') {
                    if (g_game_state->state == GAME_STATE_PLAYING ||
                        g_game_state->state == GAME_STATE_PAUSED) {
                        game_toggle_pause(g_game_state);
                    }
                } else if (wparam == 'R' || wparam == 'r') {
                    if (g_game_state->state == GAME_STATE_GAME_OVER) {
                        game_restart(g_game_state);
                    }
                } else if (wparam == VK_ESCAPE) {
                    PostQuitMessage(0);
                }
            }
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
    return 0;
}