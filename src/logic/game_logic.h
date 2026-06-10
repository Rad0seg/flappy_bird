#pragma once

#include <stdint.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIPE_WIDTH 80
#define PIPE_GAP_BASE 180
#define BIRD_SIZE 34
#define GRAVITY 0.5f
#define JUMP_FORCE -8.5f
#define PIPE_SPEED_BASE 2.5f
#define MAX_PIPES 4
#define MIN_PIPE_HEIGHT 80
#define MAX_PIPE_HEIGHT 300
#define MAX_POWERUPS 3
#define POWERUP_SIZE 28

// Типы баффов и дебаффов
typedef enum {
    POWERUP_NONE,
    POWERUP_SMALL_BIRD,
    POWERUP_CLEAR_PIPES,
    DEBUFF_GRAVITY
} PowerUpType;

// Структура баффа на карте
typedef struct {
    float x;
    float y;
    PowerUpType type;
    int active;
    float bob_time;
} PowerUp;

// Структура активного эффекта (счетчик времени)
typedef struct {
    PowerUpType type;
    float time_left;
} ActiveEffect;

// Структура птицы игрока
typedef struct {
    float x;
    float y;
    float velocity;
    int alive;
    float rotation;
    int wing_state;
    float size_multiplier;
} Bird;

// Структура трубы препятствия
typedef struct {
    float x;
    float top_height;
    int scored;
    int active;
} Pipe;

// Состояния игры (конечный автомат)
typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_COUNTDOWN,
    GAME_STATE_GAME_OVER
} GameStateType;

// Времена суток (для смены неба)
typedef enum {
    TIME_DAY,
    TIME_SUNSET,
    TIME_NIGHT
} TimeOfDay;

// Главная структура состояния игры
typedef struct {
    Bird bird;
    Pipe pipes[MAX_PIPES];
    PowerUp powerups[MAX_POWERUPS];
    ActiveEffect active_effect;
    int score;
    int best_score;
    GameStateType state;
    float game_time;
    float countdown_timer;
    int countdown_value;
    float scroll_offset;
    TimeOfDay time_of_day;
    float time_transition;
    int pipes_passed;
    float current_pipe_speed;
    float current_pipe_gap;
    float current_pipe_distance;
    float current_gravity;
    float powerup_spawn_timer;
    int pipes_cleared;
} GameState;

// Инициализация нового состояния игры
void game_init(GameState* state);

// Обновление логики каждый кадр
void game_update(GameState* state, float delta_time);

// Обработка прыжка птицы
void game_jump(GameState* state);

// Полный перезапуск игры
void game_restart(GameState* state);

// Переключение паузы
void game_toggle_pause(GameState* state);

// Проверка столкновения птицы с трубами
int check_collision(const GameState* state);

// Загрузка рекорда из файла
int load_best_score(void);

// Сохранение рекорда в файл
void save_best_score(int score);

// Получить название эффекта баффа
const char* powerup_name(PowerUpType type);