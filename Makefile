CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS = -lgdi32 -luser32 -lwinmm -mwindows
TARGET = flappy_bird.exe

SOURCES = src/main.c src/logic/game_logic.c src/interface/ui.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q src\*.o src\logic\*.o src\interface\*.o $(TARGET) 2>nul || true

run: $(TARGET)
	.\$(TARGET)

.PHONY: all clean run