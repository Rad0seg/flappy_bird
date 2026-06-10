#include "game_logic.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#define SAVE_FILE "flappy_best.dat"
#define SPEED_INCREASE_PER_10_PIPES 0.5f
#define DISTANCE_INCREASE_PER_10_PIPES 3.0f
#define POWERUP_DURATION 5.0f
#define POINTS_PER_JUMP 1

static int s_best_score = 0;

// Загрузка лучшего счета из файла
int load_best_score(void) {
    FILE* file = fopen(SAVE_FILE, "rb");
    if (!file) return 0;

    int score = 0;
    size_t rd = fread(&score, sizeof(int), 1, file);
    fclose(file);

    if (rd != 1 || score < 0 || score > 99999) return 0;
    return score;
}

// Сохранение лучшего счета в файл
void save_best_score(int score) {
    FILE* file = fopen(SAVE_FILE, "wb");
    if (!file) return;
    fwrite(&score, sizeof(int), 1, file);
    fclose(file);
}

// Получить текстовое название эффекта баффа/дебаффа
const char* powerup_name(PowerUpType type) {
    switch (type) {
        case POWERUP_SMALL_BIRD: return "MINI BIRD!";
        case POWERUP_CLEAR_PIPES: return "JUMP!";
        case DEBUFF_GRAVITY:     return "HEAVY!";
        default:                 return "";
    }
}

// Обновление параметров сложности каждые 10 пройденных труб
static void update_difficulty(GameState* state) {
    int level = state->pipes_passed / 10;
    // Скорость: начинаем с 3.0, растет на +0.5 за каждые 10 труб
    state->current_pipe_speed = 3.0f + (level * SPEED_INCREASE_PER_10_PIPES);
    // Промежуток между верхней и нижней трубой не меняется
    state->current_pipe_gap = PIPE_GAP_BASE;
    // Расстояние между трубами (горизонтальное) растет очень медленно
    state->current_pipe_distance = 250.0f + (level * DISTANCE_INCREASE_PER_10_PIPES);
}

// Пересоздание труб при завершении эффекта Clear (исчезновения труб)
static void respawn_pipes(GameState* state) {
    for (int i = 0; i < MAX_PIPES; i++) {
        state->pipes[i].x = WINDOW_WIDTH + i * state->current_pipe_distance + 100.0f;
        state->pipes[i].top_height = (float)(MIN_PIPE_HEIGHT +
            (rand() % (MAX_PIPE_HEIGHT - MIN_PIPE_HEIGHT)));
        state->pipes[i].scored = 0;
        state->pipes[i].active = 1;
    }
}

// Спавн нового баффа или дебаффа на карте
static void spawn_powerup(GameState* state) {
    // Найти свободный слот для паверапа
    int slot = -1;
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!state->powerups[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;

    // Случайный выбор типа: 25% Mini, 15% Clear, 60% Heavy
    int roll = rand() % 100;
    PowerUpType type;
    if (roll < 25) {
        type = POWERUP_SMALL_BIRD;
    } else if (roll < 40) {
        type = POWERUP_CLEAR_PIPES;
    } else {
        type = DEBUFF_GRAVITY;
    }

    // Найти самую дальнюю трубу для спавна паверапа между трубами
    float spawn_x = WINDOW_WIDTH + 100.0f;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (state->pipes[i].active && state->pipes[i].x > spawn_x) {
            spawn_x = state->pipes[i].x;
        }
    }
    
    spawn_x -= state->current_pipe_distance / 2.0f;
    
    // Спавн на безопасной высоте (центр экрана с разбросом)
    float center_y = (WINDOW_HEIGHT - 80) / 2.0f;
    float spawn_y = center_y - 50.0f + (float)(rand() % 100);
    
    if (spawn_y < 100.0f) spawn_y = 100.0f;
    if (spawn_y > WINDOW_HEIGHT - 180.0f) spawn_y = WINDOW_HEIGHT - 180.0f;

    state->powerups[slot].x = spawn_x;
    state->powerups[slot].y = spawn_y;
    state->powerups[slot].type = type;
    state->powerups[slot].active = 1;
    state->powerups[slot].bob_time = 0.0f;
}

// Применить эффект баффа или дебаффа
static void apply_effect(GameState* state, PowerUpType type) {
    // Сброс предыдущего эффекта
    state->current_gravity = GRAVITY;
    state->bird.size_multiplier = 1.0f;

    switch (type) {
        case POWERUP_SMALL_BIRD:
            // Уменьшение птицы на 30%
            state->bird.size_multiplier = 0.7f;
            state->active_effect.type = POWERUP_SMALL_BIRD;
            state->active_effect.time_left = POWERUP_DURATION;
            state->pipes_cleared = 0;
            break;

        case POWERUP_CLEAR_PIPES:
            // Убрать все трубы на 5 секунд
            for (int i = 0; i < MAX_PIPES; i++) {
                state->pipes[i].active = 0;
            }
            state->pipes_cleared = 1;
            state->active_effect.type = POWERUP_CLEAR_PIPES;
            state->active_effect.time_left = POWERUP_DURATION;
            break;

        case DEBUFF_GRAVITY:
            // Усилить гравитацию в 1.5 раза
            state->current_gravity = GRAVITY * 1.5f;
            state->active_effect.type = DEBUFF_GRAVITY;
            state->active_effect.time_left = POWERUP_DURATION;
            state->pipes_cleared = 0;
            break;

        default:
            break;
    }
}

// Сброс эффекта при истечении времени действия
static void reset_effect(GameState* state) {
    // Если был Clear - восстановить трубы на экран
    if (state->active_effect.type == POWERUP_CLEAR_PIPES && state->pipes_cleared) {
        respawn_pipes(state);
        state->pipes_cleared = 0;
    }

    state->current_gravity = GRAVITY;
    state->bird.size_multiplier = 1.0f;
    state->active_effect.type = POWERUP_NONE;
    state->active_effect.time_left = 0.0f;
}

// Проверка столкновения птицы с паверапом (круговая коллизия)
static void check_powerup_collision(GameState* state) {
    float bird_size = BIRD_SIZE * state->bird.size_multiplier;
    float bird_cx = state->bird.x + bird_size * 0.5f;
    float bird_cy = state->bird.y + bird_size * 0.5f;
    float bird_r = bird_size * 0.5f;

    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!state->powerups[i].active) continue;

        float pu_cx = state->powerups[i].x + POWERUP_SIZE * 0.5f;
        float pu_cy = state->powerups[i].y + POWERUP_SIZE * 0.5f;
        float pu_r = POWERUP_SIZE * 0.5f;

        // Расчет расстояния между центрами
        float dx = bird_cx - pu_cx;
        float dy = bird_cy - pu_cy;
        float dist = sqrtf(dx * dx + dy * dy);

        // Столкновение если сумма радиусов больше расстояния
        if (dist < bird_r + pu_r) {
            apply_effect(state, state->powerups[i].type);
            state->powerups[i].active = 0;
        }
    }
}

// Инициализация игрового состояния при старте
void game_init(GameState* state) {
    memset(state, 0, sizeof(GameState));

    s_best_score = load_best_score();

    // Инициализация птицы
    state->bird.x = 150.0f;
    state->bird.y = WINDOW_HEIGHT / 2.0f - BIRD_SIZE / 2.0f;
    state->bird.velocity = 0.0f;
    state->bird.alive = 1;
    state->bird.rotation = 0.0f;
    state->bird.wing_state = 0;
    state->bird.size_multiplier = 1.0f;

    // Параметры игры
    state->pipes_passed = 0;
    state->current_pipe_speed = PIPE_SPEED_BASE;
    state->current_pipe_gap = PIPE_GAP_BASE;
    state->current_pipe_distance = 250.0f;
    state->current_gravity = GRAVITY;

    // Инициализация труб
    for (int i = 0; i < MAX_PIPES; i++) {
        state->pipes[i].x = WINDOW_WIDTH + i * state->current_pipe_distance + 400.0f;
        state->pipes[i].top_height = (float)(MIN_PIPE_HEIGHT +
            (rand() % (MAX_PIPE_HEIGHT - MIN_PIPE_HEIGHT)));
        state->pipes[i].scored = 0;
        state->pipes[i].active = 1;
    }

    // Инициализация паверапов
    for (int i = 0; i < MAX_POWERUPS; i++) {
        state->powerups[i].active = 0;
    }

    // Инициализация состояния
    state->active_effect.type = POWERUP_NONE;
    state->active_effect.time_left = 0.0f;

    state->score = 0;
    state->best_score = s_best_score;
    state->state = GAME_STATE_MENU;
    state->game_time = 0.0f;
    state->countdown_timer = 0.0f;
    state->countdown_value = 3;
    state->scroll_offset = 0.0f;
    state->time_of_day = TIME_DAY;
    state->time_transition = 0.0f;
    state->powerup_spawn_timer = 8.0f;
    state->pipes_cleared = 0;

    srand((unsigned int)time(NULL));
}

// Главное обновление игровой логики каждый кадр
void game_update(GameState* state, float delta_time) {
    state->game_time += delta_time;

    // Анимация крыльев птицы (чередование 2 состояний)
    state->bird.wing_state = ((int)(state->game_time * 10.0f) % 2);

    // Обновление смещения для параллакса земли и облаков
    if (state->state == GAME_STATE_PLAYING) {
        state->scroll_offset += state->current_pipe_speed;
        if (state->scroll_offset >= 1000.0f) {
            state->scroll_offset -= 1000.0f;
        }
    }

    // Смена времени суток каждые 10 очков (плавный переход)
    if (state->state == GAME_STATE_PLAYING) {
        int time_index = (state->score / 10) % 3;
        TimeOfDay target_time = (TimeOfDay)time_index;

        if (target_time != state->time_of_day) {
            state->time_transition += delta_time * 0.5f;
            if (state->time_transition >= 1.0f) {
                state->time_of_day = target_time;
                state->time_transition = 0.0f;
            }
        } else {
            state->time_transition = 0.0f;
        }
    }

    // Меню: птица покачивается
    if (state->state == GAME_STATE_MENU) {
        state->bird.y = WINDOW_HEIGHT / 2.0f - BIRD_SIZE / 2.0f +
            sinf(state->game_time * 3.0f) * 15.0f;
        return;
    }

    // Пауза: ничего не обновляется
    if (state->state == GAME_STATE_PAUSED) {
        return;
    }

    // Обратный отсчет перед возобновлением игры (3-2-1)
    if (state->state == GAME_STATE_COUNTDOWN) {
        state->countdown_timer -= delta_time;
        if (state->countdown_timer <= 0.0f) {
            if (state->countdown_value > 1) {
                state->countdown_value--;
                state->countdown_timer = 1.0f;
            } else {
                state->state = GAME_STATE_PLAYING;
            }
        }
        return;
    }

    // Game Over: ничего не обновляется
    if (state->state == GAME_STATE_GAME_OVER) {
        return;
    }

    // Обновление активного эффекта (уменьшение времени действия)
    if (state->active_effect.type != POWERUP_NONE) {
        state->active_effect.time_left -= delta_time;
        
        if (state->active_effect.time_left <= 0.0f) {
            reset_effect(state);
        }
    }

    // Спавн паверапов на карту (не во время Clear)
    if (!state->pipes_cleared) {
        state->powerup_spawn_timer -= delta_time;
        if (state->powerup_spawn_timer <= 0.0f) {
            spawn_powerup(state);
            state->powerup_spawn_timer = 6.0f + (float)(rand() % 40) / 10.0f;
        }
    }

    // Обновление физики птицы (гравитация и движение)
    state->bird.velocity += state->current_gravity;
    state->bird.y += state->bird.velocity;

    // Обновление угла поворота птицы в зависимости от скорости падения
    if (state->bird.velocity < -4.0f) {
        state->bird.rotation = -25.0f;
    } else if (state->bird.velocity > 4.0f) {
        state->bird.rotation = 45.0f;
    } else {
        state->bird.rotation = state->bird.velocity * 5.0f;
    }

    // Проверка верхней границы экрана
    if (state->bird.y < 0) {
        state->bird.y = 0;
        state->bird.velocity = 0;
    }

    // Проверка нижней границы (земля)
    float bird_actual_size = BIRD_SIZE * state->bird.size_multiplier;
    if (state->bird.y > WINDOW_HEIGHT - bird_actual_size - 80) {
        state->bird.y = WINDOW_HEIGHT - bird_actual_size - 80;
        state->bird.velocity = 0;
        state->state = GAME_STATE_GAME_OVER;
        state->bird.alive = 0;

        if (state->score > s_best_score) {
            s_best_score = state->score;
            state->best_score = s_best_score;
            save_best_score(s_best_score);
        }
    }

    // Обновление труб (только если не Clear)
    if (!state->pipes_cleared) {
        for (int i = 0; i < MAX_PIPES; i++) {
            if (!state->pipes[i].active) continue;

            // Движение трубы влево со скоростью игры
            state->pipes[i].x -= state->current_pipe_speed;

            // Переработка трубы когда она уходит за левый край
            if (state->pipes[i].x < -PIPE_WIDTH - 50.0f) {
                // Найти самую дальнюю трубу для определения позиции спавна
                float max_x = state->pipes[i].x;
                for (int j = 0; j < MAX_PIPES; j++) {
                    if (state->pipes[j].x > max_x) {
                        max_x = state->pipes[j].x;
                    }
                }

                state->pipes[i].x = max_x + state->current_pipe_distance;
                state->pipes[i].top_height = (float)(MIN_PIPE_HEIGHT +
                    (rand() % (MAX_PIPE_HEIGHT - MIN_PIPE_HEIGHT)));
                state->pipes[i].scored = 0;
            }

            // Подсчет очков когда птица проходит трубу
            if (!state->pipes[i].scored &&
                state->pipes[i].x + PIPE_WIDTH < state->bird.x) {
                state->score++;
                state->pipes[i].scored = 1;
                state->pipes_passed++;

                // Обновление сложности каждые 10 труб
                if (state->pipes_passed % 10 == 0) {
                    update_difficulty(state);
                }
            }
        }
    }

    // Обновление паверапов (движение и удаление с экрана)
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!state->powerups[i].active) continue;

        // Паверапы движутся со скоростью игры (если не Clear)
        if (!state->pipes_cleared) {
            state->powerups[i].x -= state->current_pipe_speed;
        }
        
        // Обновление для анимации покачивания
        state->powerups[i].bob_time += delta_time;

        // Удаление паверапа когда он уходит за левый край
        if (state->powerups[i].x < -POWERUP_SIZE - 10.0f) {
            state->powerups[i].active = 0;
        }
    }

    // Проверка подбора паверапов птицей
    check_powerup_collision(state);

    // Проверка столкновений с трубами (не во время Clear)
    if (!state->pipes_cleared && check_collision(state)) {
        state->state = GAME_STATE_GAME_OVER;
        state->bird.alive = 0;

        if (state->score > s_best_score) {
            s_best_score = state->score;
            state->best_score = s_best_score;
            save_best_score(s_best_score);
        }
    }
}

// Обработка прыжка птицы
void game_jump(GameState* state) {
    // Начало игры с меню
    if (state->state == GAME_STATE_MENU) {
        state->state = GAME_STATE_PLAYING;
        state->bird.velocity = JUMP_FORCE;
        return;
    }

    // Прыжок во время игры
    if (state->state == GAME_STATE_PLAYING && state->bird.alive) {
        state->bird.velocity = JUMP_FORCE;
        
        // Очки за прыжок во время Clear (Jump Points баффа)
        if (state->pipes_cleared && state->active_effect.type == POWERUP_CLEAR_PIPES) {
            state->score += POINTS_PER_JUMP;
        }
    }
}

// Переключение паузы (вкл/выкл)
void game_toggle_pause(GameState* state) {
    if (state->state == GAME_STATE_PLAYING) {
        // Включить паузу
        state->state = GAME_STATE_PAUSED;
    } else if (state->state == GAME_STATE_PAUSED) {
        // Выключить паузу и начать обратный отсчет
        state->state = GAME_STATE_COUNTDOWN;
        state->countdown_timer = 1.0f;
        state->countdown_value = 3;
    }
}

// Перезапуск игры (сохранение рекорда)
void game_restart(GameState* state) {
    int old_best = s_best_score;
    game_init(state);
    s_best_score = old_best;
    state->best_score = old_best;
}

// Проверка столкновения птицы с трубами (AABB коллизия)
int check_collision(const GameState* state) {
    float actual_size = BIRD_SIZE * state->bird.size_multiplier;
    float bird_left = state->bird.x + 4.0f;
    float bird_right = state->bird.x + actual_size - 4.0f;
    float bird_top = state->bird.y + 4.0f;
    float bird_bottom = state->bird.y + actual_size - 4.0f;

    for (int i = 0; i < MAX_PIPES; i++) {
        if (!state->pipes[i].active) continue;

        float pipe_left = state->pipes[i].x;
        float pipe_right = state->pipes[i].x + PIPE_WIDTH;

        // Проверка пересечения по оси X
        if (bird_right > pipe_left && bird_left < pipe_right) {
            // Столкновение с верхней трубой
            if (bird_top < state->pipes[i].top_height) {
                return 1;
            }

            // Столкновение с нижней трубой
            float bottom_pipe_top = state->pipes[i].top_height + state->current_pipe_gap;
            if (bird_bottom > bottom_pipe_top) {
                return 1;
            }
        }
    }

    return 0;
}