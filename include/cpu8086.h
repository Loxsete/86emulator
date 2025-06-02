#ifndef CPU8086_H
#define CPU8086_H

#include <stdint.h>
#include <raylib.h>

#define MEMORY_SIZE (1024 * 1024)
#define STACK_SIZE 0x1000
#define STACK_BASE 0x7000
#define VIDEO_MEMORY 0xB8000
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define KEYBOARD_PORT 0x60
#define KEYBOARD_STATUS 0x64
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define IRQ_KEYBOARD 1
#define IVT_BASE 0x0000

typedef struct {
    uint8_t carry : 1;
    uint8_t zero : 1;
    uint8_t sign : 1;
    uint8_t overflow : 1;
    uint8_t parity : 1;
    uint8_t auxiliary : 1;
    uint8_t interrupt : 1;
} Flags;

typedef struct {
    uint16_t ax, bx, cx, dx;
    uint16_t si, di;
    uint16_t bp, sp;
    uint16_t cs, ds, es, ss;
    uint16_t ip;
    Flags flags;
    uint8_t memory[MEMORY_SIZE];
    int running;
    uint8_t last_instruction;
    uint8_t keyboard_buffer[256];
    uint8_t kb_head, kb_tail;
    uint8_t kb_status;
    uint8_t pic_irr, pic_isr, pic_imr;
} CPU8086;

void init_cpu(CPU8086* cpu);
int load_firmware(CPU8086* cpu, const char* filename);
void update_flags(CPU8086* cpu, uint16_t result);
void push(CPU8086* cpu, uint16_t value);
uint16_t pop(CPU8086* cpu);
void handle_interrupt(CPU8086* cpu, uint8_t int_num);
void handle_keyboard(CPU8086* cpu);
void read_port(CPU8086* cpu, uint16_t port, uint16_t* value);
void write_port(CPU8086* cpu, uint16_t port, uint16_t value);
void execute_instruction(CPU8086* cpu);
void draw_screen(CPU8086* cpu, int screen_x, int screen_y, int char_width, int char_height, Font font);

#endif
