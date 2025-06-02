# Компиляторы и флаги
CC = gcc
CFLAGS = -Iinclude -Wall
LDFLAGS = -lraylib
ASM = nasm
ASMFLAGS = -f bin

# Папки
SRC_DIR = src
FIRMWARE_DIR = firmware
BIN_DIR = bin
INCLUDE_DIR = include

# Файлы
ASM_SRC = $(FIRMWARE_DIR)/proshivka.asm
BIN = $(BIN_DIR)/proshivka.bin
C_SRC = $(SRC_DIR)/main.c $(SRC_DIR)/cpu8086.c
OBJ = $(C_SRC:.c=.o)
EMULATOR = emulator

# Заголовочные файлы
HEADERS = $(INCLUDE_DIR)/cpu8086.h $(INCLUDE_DIR)/main.h

# Цели
all: $(BIN) $(EMULATOR)

# Сборка бинарного файла прошивки
$(BIN): $(ASM_SRC) | $(BIN_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Сборка эмулятора
$(EMULATOR): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Компиляция исходных C-файлов с зависимостями от заголовков
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Создание папки bin, если не существует
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Очистка
clean:
	rm -rf $(BIN_DIR)/*.o $(BIN) $(EMULATOR)

# Принуждение пересборки (для тестирования)
rebuild: clean all

# Фиктивные цели
.PHONY: all clean rebuild
