cmake_minimum_required(VERSION 3.10)
project(flappy_bird C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Исходные файлы
set(SOURCES
    src/main.c
    src/logic/game_logic.c
    src/interface/ui.c
)

# Создание исполняемого файла
add_executable(flappy_bird WIN32 ${SOURCES})

# Линковка библиотек Win32
target_link_libraries(flappy_bird 
    gdi32
    user32
    winmm
)

# Флаги компиляции
if(MSVC)
    target_compile_options(flappy_bird PRIVATE /W4 /WX)
else()
    target_compile_options(flappy_bird PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()