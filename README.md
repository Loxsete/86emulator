# 8086 Emulator README
Overview

This is a simple 8086 emulator with a graphical interface that allows you to:

    Load boot sector images (.img files)

    Run emulation step-by-step or continuously

    View CPU registers and memory contents

    Display both text mode (80x25) and graphics mode (320x200 or 640x480) output

Features

    Basic 8086 instruction set emulation

    Memory emulation (1MB address space)

    Video emulation:

        Text mode (80x25 characters with attributes)

        Graphics mode (320x200 with 256 colors)

    Simple debug interface showing register states

    File selection dialog for loading disk images

    Start/Stop emulation control

Requirements

    SDL2

    SDL2_ttf

    tinyfiledialogs

    A TrueType font (either arial.ttf or DejaVuSans.ttf)

Building

Compile with:
Copy

gcc emulator.c -o emulator -lSDL2 -lSDL2_ttf -ltinyfiledialogs

Usage

    Run the emulator

    Click "Select Image" to choose a boot sector image file

    Click "Start" to begin emulation

    Click "Stop" to pause emulation

The emulator window shows:

    Current loaded file

    Emulation status

    Video output (text or graphics mode)

    Current register states

Supported Instructions

The emulator supports a subset of 8086 instructions including:

    MOV (register and memory operations)

    ADD, SUB, CMP

    JMP, JZ, JNZ, JB, JA

    PUSH, POP

    INT (with basic INT 10h video services)

    HLT

    CLI, STI

    XOR

    MUL

    INC, DEC

    LODSB, STOSB

Limitations

    Not all 8086 instructions are implemented

    Limited interrupt handling (only basic INT 10h)

    Simple memory model without segmentation faults

    Basic video modes only (text 80x25 and mode 13h 320x200)

Known Issues

    Some edge cases in instruction handling may not be perfectly emulated

    Performance may be slow for complex programs

    Limited error handling for invalid instructions

License

This code is provided as-is without warranty. You may use and modify it freely.
