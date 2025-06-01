ASM = proshivka.asm
BIN = proshivka.bin
C_SRC = program.c
EMULATOR = emulator

all: $(BIN) $(EMULATOR)

$(BIN): $(ASM)
	nasm -f bin $(ASM) -o $(BIN)

$(EMULATOR): $(C_SRC)
	gcc -o $(EMULATOR) $(C_SRC) -lraylib

clean:
	rm -f $(BIN) $(EMULATOR)
