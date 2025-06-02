#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

void init_cpu(CPU8086* cpu) {
    memset(cpu, 0, sizeof(CPU8086));
    cpu->sp = STACK_BASE;
    cpu->ip = 0x0100;
    cpu->running = 1;
    cpu->flags.interrupt = 1;
    cpu->kb_status = 0;
    cpu->pic_irr = 0;
    cpu->pic_isr = 0;
    cpu->pic_imr = 0xFD;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT * 2; i += 2) {
        cpu->memory[VIDEO_MEMORY + i] = ' ';
        cpu->memory[VIDEO_MEMORY + i + 1] = 0x07;
    }
    for (int i = 0; i < 256 * 4; i += 4) {
        cpu->memory[IVT_BASE + i] = 0x00;
        cpu->memory[IVT_BASE + i + 1] = 0x01;
        cpu->memory[IVT_BASE + i + 2] = 0x00;
        cpu->memory[IVT_BASE + i + 3] = 0x00;
    }
}

int load_firmware(CPU8086* cpu, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open firmware file: %s\n", filename);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size > MEMORY_SIZE - 0x0100) {
        fprintf(stderr, "Firmware too large: %zu bytes\n", size);
        fclose(file);
        return 0;
    }
    size_t read = fread(cpu->memory + 0x0100, 1, size, file);
    fclose(file);
    if (read != size) {
        fprintf(stderr, "Incomplete read: %zu bytes of %zu\n", read, size);
        return 0;
    }
    return 1;
}

static inline uint32_t get_physical_addr(uint16_t segment, uint16_t offset) {
    return ((uint32_t)segment << 4) + offset;
}

static inline int check_memory_bounds(uint32_t addr, uint32_t size, uint32_t max) {
    return addr <= max - size;
}

void update_flags(CPU8086* cpu, uint16_t result) {
    cpu->flags.zero = (result == 0);
    cpu->flags.sign = (result & 0x8000) != 0;
    cpu->flags.parity = 0;
    uint8_t low_byte = (uint8_t)result;
    int parity_count = 0;
    for (int i = 0; i < 8; i++) {
        parity_count += (low_byte >> i) & 1;
    }
    cpu->flags.parity = (parity_count % 2) == 0;
}

void push(CPU8086* cpu, uint16_t value) {
    cpu->sp -= 2;
    uint32_t addr = get_physical_addr(cpu->ss, cpu->sp);
    if (check_memory_bounds(addr, 2, MEMORY_SIZE)) {
        cpu->memory[addr] = value & 0xFF;
        cpu->memory[addr + 1] = (value >> 8) & 0xFF;
    } else {
        fprintf(stderr, "Stack push out of bounds at address 0x%05X\n", addr);
        cpu->running = 0;
    }
}

uint16_t pop(CPU8086* cpu) {
    uint32_t addr = get_physical_addr(cpu->ss, cpu->sp);
    if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
        fprintf(stderr, "Stack pop out of bounds at address 0x%05X\n", addr);
        cpu->running = 0;
        return 0;
    }
    uint16_t value = cpu->memory[addr] | (cpu->memory[addr + 1] << 8);
    cpu->sp += 2;
    return value;
}

void handle_interrupt(CPU8086* cpu, uint8_t int_num) {
    if (!cpu->flags.interrupt) return;
    push(cpu, cpu->flags.carry | (cpu->flags.zero << 1) | (cpu->flags.sign << 2) |
               (cpu->flags.overflow << 3) | (cpu->flags.parity << 4) |
               (cpu->flags.auxiliary << 5) | (cpu->flags.interrupt << 6));
    push(cpu, cpu->cs);
    push(cpu, cpu->ip);
    uint32_t ivt_addr = IVT_BASE + int_num * 4;
    if (!check_memory_bounds(ivt_addr, 4, MEMORY_SIZE)) {
        fprintf(stderr, "IVT access out of bounds at address 0x%05X\n", ivt_addr);
        cpu->running = 0;
        return;
    }
    cpu->ip = cpu->memory[ivt_addr] | (cpu->memory[ivt_addr + 1] << 8);
    cpu->cs = cpu->memory[ivt_addr + 2] | (cpu->memory[ivt_addr + 3] << 8);
    cpu->flags.interrupt = 0;
    cpu->pic_isr |= (1 << IRQ_KEYBOARD);
}

void handle_keyboard(CPU8086* cpu) {
    if (cpu->kb_head != cpu->kb_tail) {
        cpu->pic_irr |= (1 << IRQ_KEYBOARD);
        cpu->kb_status |= 0x01;
    }
    if (cpu->flags.interrupt && (cpu->pic_irr & ~cpu->pic_imr) & (1 << IRQ_KEYBOARD)) {
        handle_interrupt(cpu, 9);
        cpu->pic_irr &= ~(1 << IRQ_KEYBOARD);
    }
}

void read_port(CPU8086* cpu, uint16_t port, uint16_t* value) {
    *value = 0;
    if (port == KEYBOARD_PORT) {
        if (cpu->kb_head != cpu->kb_tail) {
            *value = cpu->keyboard_buffer[cpu->kb_head];
            cpu->kb_head = (cpu->kb_head + 1) % 256;
            cpu->kb_status &= ~0x01;
        } else {
            cpu->kb_status &= ~0x01;
        }
    } else if (port == KEYBOARD_STATUS) {
        *value = cpu->kb_status;
    } else if (port == PIC1_DATA) {
        *value = cpu->pic_imr;
    } else {
        fprintf(stderr, "Warning: Attempted to read from unsupported port 0x%04X\n", port);
    }
}

void write_port(CPU8086* cpu, uint16_t port, uint16_t value) {
    if (port == PIC1_DATA) {
        cpu->pic_imr = value & 0xFF;
    } else if (port == PIC1_COMMAND) {
        if (value == 0x20) {
            cpu->pic_isr = 0;
        }
    } else {
        fprintf(stderr, "Warning: Attempted to write to unsupported port 0x%04X\n", port);
    }
}

void execute_instruction(CPU8086* cpu) {
    if (!cpu->running) return;

    handle_keyboard(cpu);

    uint32_t addr = get_physical_addr(cpu->cs, cpu->ip);
    if (!check_memory_bounds(addr, 1, MEMORY_SIZE)) {
        fprintf(stderr, "IP out of memory bounds: 0x%05X\n", addr);
        cpu->running = 0;
        return;
    }

    uint8_t opcode = cpu->memory[addr];
    cpu->last_instruction = opcode;
    cpu->ip++;

    switch (opcode) {
        case 0xB8:
            if (!check_memory_bounds(addr, 3, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV AX, imm16\n");
                cpu->running = 0;
                return;
            }
            cpu->ax = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 2;
            update_flags(cpu, cpu->ax);
            break;
        case 0xB9:
            if (!check_memory_bounds(addr, 3, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV CX, imm16\n");
                cpu->running = 0;
                return;
            }
            cpu->cx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 2;
            update_flags(cpu, cpu->cx);
            break;
        case 0xBA:
            if (!check_memory_bounds(addr, 3, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV DX, imm16\n");
                cpu->running = 0;
                return;
            }
            cpu->dx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 2;
            update_flags(cpu, cpu->dx);
            break;
        case 0xBB:
            if (!check_memory_bounds(addr, 3, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV BX, imm16\n");
                cpu->running = 0;
                return;
            }
            cpu->bx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 2;
            update_flags(cpu, cpu->bx);
            break;
        case 0x8E:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV segment_reg, AX\n");
                cpu->running = 0;
                return;
            }
            switch (cpu->memory[addr + 1]) {
                case 0xD8:
                    cpu->ds = cpu->ax;
                    cpu->ip++;
                    break;
                case 0xC0:
                    cpu->es = cpu->ax;
                    cpu->ip++;
                    break;
                case 0xD0:
                    cpu->ss = cpu->ax;
                    cpu->ip++;
                    break;
                default:
                    fprintf(stderr, "Unknown operand for MOV: 0x%02X\n", cpu->memory[addr + 1]);
                    cpu->running = 0;
            }
            break;
        case 0xC7:
            if (!check_memory_bounds(addr, 6, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for MOV [mem], imm16\n");
                cpu->running = 0;
                return;
            }
            if (cpu->memory[addr + 1] == 0x06) {
                uint16_t mem_addr = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                uint16_t val = cpu->memory[addr + 4] | (cpu->memory[addr + 5] << 8);
                uint32_t phys_addr = get_physical_addr(cpu->ds, mem_addr);
                if (check_memory_bounds(phys_addr, 2, MEMORY_SIZE)) {
                    cpu->memory[phys_addr] = val & 0xFF;
                    cpu->memory[phys_addr + 1] = (val >> 8) & 0xFF;
                    cpu->ip += 5;
                } else {
                    fprintf(stderr, "Address out of memory: 0x%05X\n", phys_addr);
                    cpu->running = 0;
                }
            } else {
                fprintf(stderr, "Unknown operand for MOV: 0x%02X\n", cpu->memory[addr + 1]);
                cpu->running = 0;
            }
            break;
        case 0x03:
            if (!check_memory_bounds(addr, 4, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for ADD AX, [mem]\n");
                cpu->running = 0;
                return;
            }
            if (cpu->memory[addr + 1] == 0x06) {
                uint16_t mem_addr = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                uint32_t phys_addr = get_physical_addr(cpu->ds, mem_addr);
                if (check_memory_bounds(phys_addr, 2, MEMORY_SIZE)) {
                    uint16_t val = cpu->memory[phys_addr] | (cpu->memory[phys_addr + 1] << 8);
                    uint32_t result = cpu->ax + val;
                    cpu->flags.carry = (result > 0xFFFF) ? 1 : 0;
                    cpu->ax = result & 0xFFFF;
                    cpu->ip += 3;
                    update_flags(cpu, cpu->ax);
                } else {
                    fprintf(stderr, "Address out of memory: 0x%05X\n", phys_addr);
                    cpu->running = 0;
                }
            } else {
                fprintf(stderr, "Unknown operand for ADD: 0x%02X\n", cpu->memory[addr + 1]);
                cpu->running = 0;
            }
            break;
        case 0x2B:
            if (!check_memory_bounds(addr, 4, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for SUB AX, [mem]\n");
                cpu->running = 0;
                return;
            }
            if (cpu->memory[addr + 1] == 0x06) {
                uint16_t mem_addr = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                uint32_t phys_addr = get_physical_addr(cpu->ds, mem_addr);
                if (check_memory_bounds(phys_addr, 2, MEMORY_SIZE)) {
                    uint16_t val = cpu->memory[phys_addr] | (cpu->memory[phys_addr + 1] << 8);
                    int32_t result = cpu->ax - val;
                    cpu->flags.carry = (result < 0) ? 1 : 0;
                    cpu->ax = result & 0xFFFF;
                    cpu->ip += 3;
                    update_flags(cpu, cpu->ax);
                } else {
                    fprintf(stderr, "Address out of memory: 0x%05X\n", phys_addr);
                    cpu->running = 0;
                }
            } else {
                fprintf(stderr, "Unknown operand for SUB: 0x%02X\n", cpu->memory[addr + 1]);
                cpu->running = 0;
            }
            break;
        case 0xEB:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for JMP short imm8\n");
                cpu->running = 0;
                return;
            }
            int8_t offset = (int8_t)cpu->memory[addr + 1];
            cpu->ip += offset + 1;
            break;
        case 0x74:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for JE imm8\n");
                cpu->running = 0;
                return;
            }
            int8_t je_offset = (int8_t)cpu->memory[addr + 1];
            if (cpu->flags.zero) {
                cpu->ip += je_offset + 1;
            } else {
                cpu->ip++;
            }
            break;
        case 0xF4:
            cpu->running = 0;
            break;
        case 0x83:
            if (!check_memory_bounds(addr, 3, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for CMP reg, imm8\n");
                cpu->running = 0;
                return;
            }
            uint8_t modrm = cpu->memory[addr + 1];
            int8_t imm8 = (int8_t)cpu->memory[addr + 2];
            if ((modrm & 0xF8) == 0xF8) {
                int32_t result = cpu->ax - imm8;
                cpu->flags.zero = (result == 0);
                cpu->flags.sign = (result & 0x8000) != 0;
                cpu->flags.carry = (result < 0) ? 1 : 0;
                cpu->flags.parity = 0;
                uint8_t low_byte = result & 0xFF;
                for (int i = 0; i < 8; i++) {
                    cpu->flags.parity ^= (low_byte >> i) & 1;
                }
                cpu->ip += 2;
            } else {
                fprintf(stderr, "Unsupported ModR/M for 0x83: 0x%02X\n", modrm);
                cpu->running = 0;
            }
            break;
        case 0xE4:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for IN AL, imm8\n");
                cpu->running = 0;
                return;
            }
            uint16_t port = cpu->memory[addr + 1];
            uint16_t value;
            read_port(cpu, port, &value);
            cpu->ax = (cpu->ax & 0xFF00) | (value & 0xFF);
            cpu->ip++;
            update_flags(cpu, cpu->ax);
            break;
        case 0xE5:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for IN AX, imm8\n");
                cpu->running = 0;
                return;
            }
            port = cpu->memory[addr + 1];
            read_port(cpu, port, &value);
            cpu->ax = value;
            cpu->ip++;
            update_flags(cpu, cpu->ax);
            break;
        case 0xE6:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for OUT imm8, AL\n");
                cpu->running = 0;
                return;
            }
            port = cpu->memory[addr + 1];
            write_port(cpu, port, cpu->ax & 0xFF);
            cpu->ip++;
            break;
        case 0xE7:
            if (!check_memory_bounds(addr, 2, MEMORY_SIZE)) {
                fprintf(stderr, "Insufficient memory for OUT imm8, AX\n");
                cpu->running = 0;
                return;
            }
            port = cpu->memory[addr + 1];
            write_port(cpu, port, cpu->ax);
            cpu->ip++;
            break;
        case 0xFA:
            cpu->flags.interrupt = 0;
            break;
        case 0xFB:
            cpu->flags.interrupt = 1;
            break;
        case 0xCF:
            cpu->ip = pop(cpu);
            cpu->cs = pop(cpu);
            uint16_t flags_val = pop(cpu);
            cpu->flags.carry = flags_val & 0x01;
            cpu->flags.zero = (flags_val >> 1) & 0x01;
            cpu->flags.sign = (flags_val >> 2) & 0x01;
            cpu->flags.overflow = (flags_val >> 3) & 0x01;
            cpu->flags.parity = (flags_val >> 4) & 0x01;
            cpu->flags.auxiliary = (flags_val >> 5) & 0x01;
            cpu->flags.interrupt = (flags_val >> 6) & 0x01;
            cpu->pic_isr = 0;
            break;
        default:
            fprintf(stderr, "Unknown instruction: 0x%02X\n", opcode);
            cpu->running = 1;
            break;
    }
}

void draw_screen(CPU8086* cpu, int screen_x, int screen_y, int char_width, int char_height, Font font) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int mem_offset = VIDEO_MEMORY + (y * SCREEN_WIDTH + x) * 2;
            uint8_t ch = cpu->memory[mem_offset];
            uint8_t attr = cpu->memory[mem_offset + 1];
            Color fg_color = (attr == 0x07) ? WHITE : (attr == 0x70) ? BLACK : WHITE;
            Color bg_color = (attr == 0x07) ? BLACK : (attr == 0x70) ? WHITE : BLACK;
            DrawRectangle(screen_x + x * char_width, screen_y + y * char_height, 
                          char_width, char_height, bg_color);
            if (ch != 0) {
                DrawTextEx(font, TextFormat("%c", ch), 
                           (Vector2){screen_x + x * char_width, screen_y + y * char_height}, 
                           char_height, 1, fg_color);
            }
        }
    }
}

int main(void) {
    CPU8086 cpu;
    init_cpu(&cpu);
    
    if (!load_firmware(&cpu, "proshivka.bin")) {
        return 1;
    }

    const int window_width = 800;
    const int window_height = 600;
    InitWindow(window_width, window_height, "8086 Emulator");
    SetTargetFPS(60);

    Font font = LoadFont("terminus.ttf");
    if (font.texture.id == 0) {
        font = GetFontDefault();
    }

    const int char_width = 10;
    const int char_height = 20;
    const int screen_width_pixels = SCREEN_WIDTH * char_width;
    const int screen_height_pixels = SCREEN_HEIGHT * char_height;
    const int screen_x = (window_width - screen_width_pixels) / 2;
    const int screen_y = (window_height - screen_height_pixels) / 2;

    bool auto_run = true;
    float step_timer = 0.0f;
    const float step_interval = 0.1f;

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        step_timer += delta_time;

        if (IsKeyPressed(KEY_A)) {
            auto_run = !auto_run;
        }

        int key = GetKeyPressed();
        if (key != 0 && cpu.running) {
            cpu.keyboard_buffer[cpu.kb_tail] = (uint8_t)key;
            cpu.kb_tail = (cpu.kb_tail + 1) % 256;
            cpu.kb_status |= 0x01;
        }

        if (!auto_run && IsKeyPressed(KEY_SPACE) && cpu.running) {
            execute_instruction(&cpu);
        }

        if (auto_run && step_timer >= step_interval && cpu.running) {
            step_timer = 0.0f;
            execute_instruction(&cpu);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);
        draw_screen(&cpu, screen_x, screen_y, char_width, char_height, font);
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
