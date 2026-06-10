#include <windows.h>
#include "logic/game_logic.h"
#include "interface/ui.h"

// Точка входа приложения Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Структуры для хранения состояния
    GameState game_state;
    UIContext ui_context;

    // Инициализация игровой логики
    game_init(&game_state);

    // Инициализация интерфейса и окна
    if (!ui_init(&ui_context, hInstance)) {
        MessageBoxA(NULL, "Failed to initialize UI", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Главный игровой цикл (блокирует до закрытия окна)
    int result = ui_run(&ui_context, &game_state);

    // Очистка всех ресурсов
    ui_cleanup(&ui_context);

    return result;
    
}