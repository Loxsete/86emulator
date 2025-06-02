# 8086 Emulator

A simple Intel 8086 processor emulator with text-mode display and keyboard support.

## Files

- `program.c` - Main emulator code
- `proshivka.asm` - Firmware assembly code
- `Makefile` - Build configuration

## Building

```bash
make
```

## Dependencies

- **Raylib** - Graphics and input handling
- **GCC** - C compiler
- **NASM** - Assembler for firmware

## Features

- Basic 8086 instruction set support
- Text mode video memory (80x25)
- Keyboard input handling
- Interrupt system (keyboard interrupts)
- Memory management with bounds checking
- Step-by-step or automatic execution

## Controls

- **A** - Toggle between automatic and manual execution
- **Space** - Execute single instruction (manual mode)
- **Any key** - Send keyboard input to emulated system

## Memory Layout

- **0x0000-0x03FF** - Interrupt Vector Table
- **0x0100** - Program start address
- **0x7000** - Stack base
- **0xB8000** - Video memory (text mode)

## Supported Instructions

- MOV (register, immediate, memory)
- ADD, SUB (register with memory)
- JMP (short jumps)
- JE (conditional jump)
- CMP (compare with immediate)
- IN, OUT (port I/O)
- CLI, STI (interrupt control)
- HLT (halt)
- IRET (interrupt return)

## Architecture

The emulator implements:
- 16-bit registers (AX, BX, CX, DX, SI, DI, BP, SP)
- Segment registers (CS, DS, ES, SS)
- Flags register with basic arithmetic flags
- 1MB memory space
- Keyboard controller simulation
- Programmable Interrupt Controller (PIC) basics

## Usage

1. Compile the project with `make`
2. Place your firmware binary as `proshivka.bin`
3. Run the emulator
4. The system will load firmware at address 0x0100 and start execution

The emulator window shows the text-mode display output and accepts keyboard input that gets forwarded to the emulated system.
