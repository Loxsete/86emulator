#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tinyfiledialogs.h"

// CPU structure
typedef struct {
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t ip;
    uint16_t cs, ds, ss, es; // Segment registers
    uint16_t flags; // Flags (ZF=0x40, CF=0x01, SF=0x80, OF=0x800, IF=0x200)
    uint8_t memory[0x100000]; // 1 MB of memory
    uint8_t video_memory_text[80 * 25 * 2]; // Text mode 80x25 (char + attribute)
    uint8_t video_memory_vga[320 * 200]; // VGA mode 320x200 (256 colors)
    int video_mode; // 0 - text, 1 - graphics (mode 13h)
} CPU;

// Initialize CPU
void reset_cpu(CPU *cpu) {
    cpu->ax = cpu->bx = cpu->cx = cpu->dx = 0;
    cpu->si = cpu->di = cpu->bp = cpu->sp = 0;
    cpu->ip = 0x7C00; // Boot sector start
    cpu->cs = 0x0000; // Code segment
    cpu->ds = 0x0000; // Data segment
    cpu->ss = 0x0000; // Stack segment
    cpu->es = 0x0000; // Extra segment
    cpu->flags = 0;
    cpu->video_mode = 0; // Default to text mode
    memset(cpu->memory, 0, sizeof(cpu->memory));
    memset(cpu->video_memory_text, 0, sizeof(cpu->video_memory_text));
    memset(cpu->video_memory_vga, 0, sizeof(cpu->video_memory_vga));
}

// Load image file (boot sector)
int load_image(CPU *cpu, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        tinyfd_messageBox("Error", "Failed to open file!", "error", "ok", 1);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 512) {
        tinyfd_messageBox("Error", "File too small for a boot sector!", "error", "ok", 1);
        fclose(file);
        return 0;
    }
    // Check boot sector signature
    uint8_t buffer[512];
    fread(buffer, 1, 512, file);
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        tinyfd_messageBox("Error", "Invalid boot sector signature!", "error", "ok", 1);
        fclose(file);
        return 0;
    }
    // Load first sector to 0x7C00
    rewind(file);
    fread(cpu->memory + 0x7C00, 1, 512, file);
    // Load remaining data if any
    if (size > 512) {
        fseek(file, 512, SEEK_SET);
        long remaining_size = size - 512;
        if (remaining_size > 0x100000 - 0x7E00) remaining_size = 0x100000 - 0x7E00;
        fread(cpu->memory + 0x7E00, 1, remaining_size, file);
    }
    fclose(file);
    char message[512];
    snprintf(message, sizeof(message), "Loaded image: %s, size: %ld bytes", filename, size);
    tinyfd_messageBox("Success", message, "info", "ok", 1);
    return 1;
}

// Get physical address from segment and offset
uint32_t get_physical_address(CPU *cpu, uint16_t segment, uint16_t offset) {
    return (segment << 4) + offset;
}

// Handle interrupts
void handle_interrupt(CPU *cpu) {
    uint32_t addr = get_physical_address(cpu, cpu->cs, cpu->ip);
    uint8_t int_num = cpu->memory[addr + 1];
    if (int_num == 0x10) { // INT 10h - Video
        uint8_t ah = (cpu->ax >> 8) & 0xFF;
        if (ah == 0x00) { // Set video mode
            uint8_t al = cpu->ax & 0xFF;
            if (al == 0x03) { // Text mode 80x25
                cpu->video_mode = 0;
                memset(cpu->video_memory_text, 0, sizeof(cpu->video_memory_text));
            } else if (al == 0x13) { // Graphics mode 320x200, 256 colors
                cpu->video_mode = 1;
                memset(cpu->video_memory_vga, 0, sizeof(cpu->video_memory_vga));
            }
        } else if (ah == 0x0E && cpu->video_mode == 0) { // Print character in text mode
            uint8_t al = cpu->ax & 0xFF; // Character
            uint8_t bl = cpu->bx & 0xFF; // Attribute (color)
            uint16_t cursor_pos = cpu->cx; // Cursor position
            if (cursor_pos < 80 * 25) {
                cpu->video_memory_text[cursor_pos * 2] = al;     // Character
                cpu->video_memory_text[cursor_pos * 2 + 1] = bl; // Attribute
            }
        }
    }
    cpu->ip += 2; // Skip INT
}

// Emulate one instruction
void step_cpu(CPU *cpu) {
    uint32_t addr = get_physical_address(cpu, cpu->cs, cpu->ip);
    uint8_t opcode = cpu->memory[addr];
    uint32_t stack_addr; // Moved outside switch to avoid redefinition

    switch (opcode) {
        case 0xB8: // MOV AX, imm16
            cpu->ax = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0xBB: // MOV BX, imm16
            cpu->bx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0xB9: // MOV CX, imm16
            cpu->cx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0xBA: // MOV DX, imm16
            cpu->dx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0x01: // ADD AX, BX
            cpu->ax += cpu->bx;
            cpu->flags = (cpu->ax == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
            cpu->ip += 2;
            break;
        case 0x29: // SUB AX, BX
            cpu->ax -= cpu->bx;
            cpu->flags = (cpu->ax == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
            cpu->ip += 2;
            break;
        case 0x3B: // CMP AX, BX
            uint16_t result = cpu->ax - cpu->bx;
            cpu->flags = (result == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
            cpu->ip += 2;
            break;
        case 0x74: // JZ (Jump if Zero)
            if (cpu->flags & 0x40) { // ZF set
                int8_t offset = cpu->memory[addr + 1];
                cpu->ip += offset + 2;
            } else {
                cpu->ip += 2;
            }
            break;
        case 0x75: // JNZ (Jump if Not Zero)
            if (!(cpu->flags & 0x40)) { // ZF not set
                int8_t offset = cpu->memory[addr + 1];
                cpu->ip += offset + 2;
            } else {
                cpu->ip += 2;
            }
            break;
        case 0xE9: // JMP near
            int16_t offset = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += offset + 3;
            break;
        case 0xCD: // INT
            handle_interrupt(cpu);
            break;
        case 0xF4: // HLT
            cpu->ip++;
            break;
        case 0xC6: // MOV byte [mem], imm8
            if (cpu->memory[addr + 1] == 0x06) { // Direct address
                uint16_t mem_offset = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                uint32_t mem_addr = get_physical_address(cpu, cpu->ds, mem_offset);
                if (mem_addr >= 0xA0000 && mem_addr < 0xA0000 + 320 * 200 && cpu->video_mode == 1) {
                    cpu->video_memory_vga[mem_addr - 0xA0000] = cpu->memory[addr + 4];
                } else {
                    cpu->memory[mem_addr] = cpu->memory[addr + 4];
                }
                cpu->ip += 5;
            } else {
                cpu->ip++;
            }
            break;
        case 0x50: // PUSH AX
            cpu->sp -= 2;
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->memory[stack_addr] = cpu->ax & 0xFF;
            cpu->memory[stack_addr + 1] = (cpu->ax >> 8) & 0xFF;
            cpu->ip++;
            break;
        case 0x58: // POP AX
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->ax = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8);
            cpu->sp += 2;
            cpu->ip++;
            break;
        case 0xE8: // CALL near
            int16_t offset_call = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->sp -= 2;
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            uint16_t return_ip = cpu->ip + 3;
            cpu->memory[stack_addr] = return_ip & 0xFF;
            cpu->memory[stack_addr + 1] = (return_ip >> 8) & 0xFF;
            cpu->ip += offset_call + 3;
            break;
        case 0xC3: // RET
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->ip = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8);
            cpu->sp += 2;
            break;

        // Previously added opcodes
        case 0xFA: // CLI (Clear Interrupt Flag)
            cpu->flags &= ~0x200; // Clear IF
            cpu->ip++;
            break;
        case 0x31: // XOR reg, reg (e.g., XOR AX, AX)
            if (cpu->memory[addr + 1] == 0xC0) { // XOR AX, AX
                cpu->ax = cpu->ax ^ cpu->ax; // AX = 0
                cpu->flags = (cpu->ax == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
                cpu->ip += 2;
            } else {
                cpu->ip++;
            }
            break;
        case 0x8E: // MOV segment_reg, reg
            {
                uint8_t modrm = cpu->memory[addr + 1];
                if (modrm == 0xD8) { // MOV DS, AX
                    cpu->ds = cpu->ax;
                    cpu->ip += 2;
                } else if (modrm == 0xC0) { // MOV ES, AX
                    cpu->es = cpu->ax;
                    cpu->ip += 2;
                } else {
                    cpu->ip++;
                }
            }
            break;
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: // MOV reg8, imm8
        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            {
                uint8_t reg = opcode & 0x07;
                uint8_t value = cpu->memory[addr + 1];
                switch (reg) {
                    case 0: cpu->ax = (cpu->ax & 0xFF00) | value; break; // AL
                    case 1: cpu->cx = (cpu->cx & 0xFF00) | value; break; // CL
                    case 2: cpu->dx = (cpu->dx & 0xFF00) | value; break; // DL
                    case 3: cpu->bx = (cpu->bx & 0xFF00) | value; break; // BL
                    case 4: cpu->ax = (cpu->ax & 0x00FF) | (value << 8); break; // AH
                    case 5: cpu->cx = (cpu->cx & 0x00FF) | (value << 8); break; // CH
                    case 6: cpu->dx = (cpu->dx & 0x00FF) | (value << 8); break; // DH
                    case 7: cpu->bx = (cpu->bx & 0x00FF) | (value << 8); break; // BH
                }
                cpu->ip += 2;
            }
            break;
        case 0xBF: // MOV DI, imm16
            cpu->di = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0xBE: // MOV SI, imm16
            cpu->si = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->ip += 3;
            break;
        case 0x07: // POP ES
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->es = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8);
            cpu->sp += 2;
            cpu->ip++;
            break;
        case 0xAC: // LODSB
            {
                uint32_t src_addr = get_physical_address(cpu, cpu->ds, cpu->si);
                cpu->ax = (cpu->ax & 0xFF00) | cpu->memory[src_addr];
                cpu->si++;
                cpu->ip++;
            }
            break;
        case 0x3C: // CMP AL, imm8
            {
                uint8_t value = cpu->memory[addr + 1];
                uint8_t al = cpu->ax & 0xFF;
                uint8_t result = al - value;
                cpu->flags = (result == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
                cpu->flags = (result & 0x80) ? (cpu->flags | 0x80) : (cpu->flags & ~0x80); // SF
                cpu->flags = (al < value) ? (cpu->flags | 0x01) : (cpu->flags & ~0x01); // CF
                cpu->ip += 2;
            }
            break;
        case 0x0E: // PUSH CS
            cpu->sp -= 2;
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->memory[stack_addr] = cpu->cs & 0xFF;
            cpu->memory[stack_addr + 1] = (cpu->cs >> 8) & 0xFF;
            cpu->ip++;
            break;
        case 0x1F: // POP DS
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            cpu->ds = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8);
            cpu->sp += 2;
            cpu->ip++;
            break;

        // New opcodes (ЕЩЁ БОЛЬШЕ!)
        case 0xFB: // STI (Set Interrupt Flag)
            cpu->flags |= 0x200; // Set IF
            cpu->ip++;
            break;
        case 0x33: // XOR reg, reg (e.g., XOR BX, BX)
            if (cpu->memory[addr + 1] == 0xDB) { // XOR BX, BX
                cpu->bx = cpu->bx ^ cpu->bx;
                cpu->flags = (cpu->bx == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
                cpu->ip += 2;
            } else {
                cpu->ip++;
            }
            break;
        case 0x89: // MOV reg/mem, reg (e.g., MOV [BX], AX)
            {
                uint8_t modrm = cpu->memory[addr + 1];
                if (modrm == 0x07) { // MOV [BX], AX
                    uint32_t mem_addr = get_physical_address(cpu, cpu->ds, cpu->bx);
                    cpu->memory[mem_addr] = cpu->ax & 0xFF;
                    cpu->memory[mem_addr + 1] = (cpu->ax >> 8) & 0xFF;
                    cpu->ip += 2;
                } else {
                    cpu->ip++;
                }
            }
            break;
        case 0x8B: // MOV reg, reg/mem (e.g., MOV AX, [BX])
            {
                uint8_t modrm = cpu->memory[addr + 1];
                if (modrm == 0x07) { // MOV AX, [BX]
                    uint32_t mem_addr = get_physical_address(cpu, cpu->ds, cpu->bx);
                    cpu->ax = cpu->memory[mem_addr] | (cpu->memory[mem_addr + 1] << 8);
                    cpu->ip += 2;
                } else {
                    cpu->ip++;
                }
            }
            break;
        case 0xA0: // MOV AL, [imm16]
            {
                uint16_t mem_offset = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
                uint32_t mem_addr = get_physical_address(cpu, cpu->ds, mem_offset);
                cpu->ax = (cpu->ax & 0xFF00) | cpu->memory[mem_addr];
                cpu->ip += 3;
            }
            break;
        case 0xA1: // MOV AX, [imm16]
            {
                uint16_t mem_offset = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
                uint32_t mem_addr = get_physical_address(cpu, cpu->ds, mem_offset);
                cpu->ax = cpu->memory[mem_addr] | (cpu->memory[mem_addr + 1] << 8);
                cpu->ip += 3;
            }
            break;
        case 0xAA: // STOSB (Store String Byte)
            {
                uint32_t dest_addr = get_physical_address(cpu, cpu->es, cpu->di);
                cpu->memory[dest_addr] = cpu->ax & 0xFF;
                cpu->di++;
                cpu->ip++;
            }
            break;
        case 0xEB: // JMP short
            {
                int8_t offset = cpu->memory[addr + 1];
                cpu->ip += offset + 2;
            }
            break;
        case 0x72: // JB/JNAE (Jump if Below)
            if (cpu->flags & 0x01) { // CF set
                int8_t offset = cpu->memory[addr + 1];
                cpu->ip += offset + 2;
            } else {
                cpu->ip += 2;
            }
            break;
        case 0x77: // JA/JNBE (Jump if Above)
            if (!(cpu->flags & 0x01) && !(cpu->flags & 0x40)) { // CF=0 and ZF=0
                int8_t offset = cpu->memory[addr + 1];
                cpu->ip += offset + 2;
            } else {
                cpu->ip += 2;
            }
            break;
        case 0xF6: // Group 1 (e.g., MUL, DIV)
            {
                uint8_t modrm = cpu->memory[addr + 1];
                uint8_t op = (modrm >> 3) & 0x7;
                if (op == 4 && modrm == 0xE0) { // MUL AL
                    uint16_t result = (cpu->ax & 0xFF) * (cpu->ax & 0xFF);
                    cpu->ax = result;
                    cpu->flags = (result > 0xFF) ? (cpu->flags | 0x01) : (cpu->flags & ~0x01); // CF
                    cpu->ip += 2;
                } else {
                    cpu->ip++;
                }
            }
            break;
        case 0xFE: // INC/DEC byte
            {
                uint8_t modrm = cpu->memory[addr + 1];
                if (modrm == 0x06) { // INC byte [imm16]
                    uint16_t mem_offset = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                    uint32_t mem_addr = get_physical_address(cpu, cpu->ds, mem_offset);
                    cpu->memory[mem_addr]++;
                    cpu->flags = (cpu->memory[mem_addr] == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
                    cpu->ip += 4;
                } else if (modrm == 0x0E) { // DEC byte [imm16]
                    uint16_t mem_offset = cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8);
                    uint32_t mem_addr = get_physical_address(cpu, cpu->ds, mem_offset);
                    cpu->memory[mem_addr]--;
                    cpu->flags = (cpu->memory[mem_addr] == 0) ? (cpu->flags | 0x40) : (cpu->flags & ~0x40); // ZF
                    cpu->ip += 4;
                } else {
                    cpu->ip++;
                }
            }
            break;

        default:
            printf("Unknown opcode: %02X at CS:IP=%04X:%04X\n", opcode, cpu->cs, cpu->ip);
            cpu->ip++;
            break;
    }
}

// Check if a point is inside a rectangle (for buttons)
int is_point_in_rect(int x, int y, SDL_Rect *rect) {
    return (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h);
}

// Simple VGA palette (256 colors)
void get_vga_color(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0xE0);        // Red
    *g = (color & 0x1C) << 3;   // Green
    *b = (color & 0x03) << 6;   // Blue
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *window = SDL_CreateWindow("8086 Emulator", SDL_WINDOWPOS_CENTERED, 
                                          SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont("arial.ttf", 16);
    if (!font) {
        tinyfd_messageBox("Error", "Failed to load font arial.ttf! Trying DejaVuSans.ttf...", "error", "ok", 1);
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
        if (!font) {
            tinyfd_messageBox("Error", "Failed to load DejaVuSans.ttf! Exiting...", "error", "ok", 1);
            return 1;
        }
    }

    CPU cpu;
    reset_cpu(&cpu);
    int running = 1;
    int emulation_running = 0;
    char filename[256] = "No file";
    int image_loaded = 0;
    int button_pressed = 0;

    SDL_Rect select_button = {50, 20, 200, 50};
    SDL_Rect start_button = {300, 20, 200, 50};
    SDL_Rect status_bar = {50, 80, 700, 30};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color green = {0, 255, 0, 255};
    SDL_Color gray = {150, 150, 150, 255};
    SDL_Color dark_gray = {100, 100, 100, 255};

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;
                button_pressed = 1;

                if (is_point_in_rect(mouse_x, mouse_y, &select_button)) {
                    const char *file = tinyfd_openFileDialog(
                        "Select Image (.img)", "", 1, (const char *[]){"*.img"}, "Disk Images", 0);
                    if (file) {
                        strncpy(filename, file, sizeof(filename) - 1);
                        filename[sizeof(filename) - 1] = '\0';
                        if (load_image(&cpu, filename)) {
                            image_loaded = 1;
                        } else {
                            image_loaded = 0;
                            strncpy(filename, "Load Error", sizeof(filename));
                        }
                    }
                }

                if (is_point_in_rect(mouse_x, mouse_y, &start_button) && image_loaded) {
                    emulation_running = !emulation_running;
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                button_pressed = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        if (emulation_running) {
            step_cpu(&cpu);
        }

        SDL_Color select_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &select_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, select_color.r, select_color.g, select_color.b, select_color.a);
        SDL_RenderFillRect(renderer, &select_button);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &select_button);

        SDL_Color start_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &start_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, start_color.r, start_color.g, start_color.b, start_color.a);
        SDL_RenderFillRect(renderer, &start_button);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &start_button);

        SDL_Surface *select_surface = TTF_RenderUTF8_Solid(font, "Select Image", white);
        SDL_Texture *select_texture = SDL_CreateTextureFromSurface(renderer, select_surface);
        SDL_Rect select_text_rect = {select_button.x + 10, select_button.y + 15, select_surface->w, select_surface->h};
        SDL_RenderCopy(renderer, select_texture, NULL, &select_text_rect);

        SDL_Surface *start_surface = TTF_RenderUTF8_Solid(font, emulation_running ? "Stop" : "Start", image_loaded ? green : white);
        SDL_Texture *start_texture = SDL_CreateTextureFromSurface(renderer, start_surface);
        SDL_Rect start_text_rect = {start_button.x + 10, start_button.y + 15, start_surface->w, start_surface->h};
        SDL_RenderCopy(renderer, start_texture, NULL, &start_text_rect);

        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &status_bar);
        char status_text[512];
        snprintf(status_text, sizeof(status_text), "File: %s | Status: %s", filename, emulation_running ? "Emulation Running" : "Emulation Stopped");
        SDL_Surface *status_surface = TTF_RenderUTF8_Solid(font, status_text, white);
        SDL_Texture *status_texture = SDL_CreateTextureFromSurface(renderer, status_surface);
        SDL_Rect status_text_rect = {status_bar.x + 5, status_bar.y + 5, status_surface->w, status_surface->h};
        SDL_RenderCopy(renderer, status_texture, NULL, &status_text_rect);

        SDL_Rect emulator_screen = {50, 120, 640, 400};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &emulator_screen);

        if (cpu.video_mode == 0) {
            for (int y = 0; y < 25; y++) {
                for (int x = 0; x < 80; x++) {
                    int idx = (y * 80 + x) * 2;
                    char ch = cpu.video_memory_text[idx];
                    if (ch == 0) continue;
                    char str[2] = {ch, '\0'};
                    SDL_Surface *char_surface = TTF_RenderUTF8_Solid(font, str, white);
                    SDL_Texture *char_texture = SDL_CreateTextureFromSurface(renderer, char_surface);
                    SDL_Rect char_rect = {emulator_screen.x + x * 8, emulator_screen.y + y * 16, char_surface->w, char_surface->h};
                    SDL_RenderCopy(renderer, char_texture, NULL, &char_rect);
                    SDL_FreeSurface(char_surface);
                    SDL_DestroyTexture(char_texture);
                }
            }
        } else if (cpu.video_mode == 1) {
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 320; x++) {
                    uint8_t color = cpu.video_memory_vga[y * 320 + x];
                    if (color == 0) continue;
                    uint8_t r, g, b;
                    get_vga_color(color, &r, &g, &b);
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_Rect pixel = {emulator_screen.x + x * 2, emulator_screen.y + y * 2, 2, 2};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        char reg_text[128];
        snprintf(reg_text, sizeof(reg_text), "AX=%04X BX=%04X CX=%04X DX=%04X CS:IP=%04X:%04X", 
                 cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.cs, cpu.ip);
        SDL_Surface *reg_surface = TTF_RenderUTF8_Solid(font, reg_text, white);
        SDL_Texture *reg_texture = SDL_CreateTextureFromSurface(renderer, reg_surface);
        SDL_Rect reg_text_rect = {50, 530, reg_surface->w, reg_surface->h};
        SDL_RenderCopy(renderer, reg_texture, NULL, &reg_text_rect); // Fixed typo: reg_text_rect instead of ®_text_rect

        SDL_FreeSurface(select_surface);
        SDL_DestroyTexture(select_texture);
        SDL_FreeSurface(start_surface);
        SDL_DestroyTexture(start_texture);
        SDL_FreeSurface(status_surface);
        SDL_DestroyTexture(status_texture);
        SDL_FreeSurface(reg_surface);
        SDL_DestroyTexture(reg_texture);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
// Check if a point is inside a rectangle (for buttons)
int is_point_in_rect(int x, int y, SDL_Rect *rect) {
    return (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h);
}

// Simple VGA palette (256 colors)
void get_vga_color(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0xE0);        // Red
    *g = (color & 0x1C) << 3;   // Green
    *b = (color & 0x03) << 6;   // Blue
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Create window
    SDL_Window *window = SDL_CreateWindow("8086 Emulator", SDL_WINDOWPOS_CENTERED, 
                                          SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Load font
    TTF_Font *font = TTF_OpenFont("arial.ttf", 16);
    if (!font) {
        tinyfd_messageBox("Error", "Failed to load font arial.ttf! Trying DejaVuSans.ttf...", "error", "ok", 1);
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
        if (!font) {
            tinyfd_messageBox("Error", "Failed to load DejaVuSans.ttf! Exiting...", "error", "ok", 1);
            return 1;
        }
    }

    CPU cpu;
    reset_cpu(&cpu);
    int running = 1;
    int emulation_running = 0;
    char filename[256] = "No file";
    int image_loaded = 0;
    int button_pressed = 0; // For button press effect

    // Define buttons
    SDL_Rect select_button = {50, 20, 200, 50};  // "Select Image" button
    SDL_Rect start_button = {300, 20, 200, 50};  // "Start/Stop" button
    SDL_Rect status_bar = {50, 80, 700, 30};     // Status bar
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color green = {0, 255, 0, 255};
    SDL_Color gray = {150, 150, 150, 255};
    SDL_Color dark_gray = {100, 100, 100, 255};

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;
                button_pressed = 1;

                // "Select Image" button
                if (is_point_in_rect(mouse_x, mouse_y, &select_button)) {
                    const char *file = tinyfd_openFileDialog(
                        "Select Image (.img)", "", 1, (const char *[]){"*.img"}, "Disk Images", 0);
                    if (file) {
                        strncpy(filename, file, sizeof(filename) - 1);
                        filename[sizeof(filename) - 1] = '\0';
                        if (load_image(&cpu, filename)) {
                            image_loaded = 1;
                        } else {
                            image_loaded = 0;
                            strncpy(filename, "Load Error", sizeof(filename));
                        }
                    }
                }

                // "Start/Stop" button
                if (is_point_in_rect(mouse_x, mouse_y, &start_button) && image_loaded) {
                    emulation_running = !emulation_running; // Toggle state
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                button_pressed = 0;
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // Dark background
        SDL_RenderClear(renderer);

        // Emulation
        if (emulation_running) {
            step_cpu(&cpu);
        }

        // Draw buttons
        SDL_Color select_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &select_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, select_color.r, select_color.g, select_color.b, select_color.a);
        SDL_RenderFillRect(renderer, &select_button);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &select_button);

        SDL_Color start_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &start_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, start_color.r, start_color.g, start_color.b, start_color.a);
        SDL_RenderFillRect(renderer, &start_button);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &start_button);

        // Button text
        SDL_Surface *select_surface = TTF_RenderUTF8_Solid(font, "Select Image", white);
        SDL_Texture *select_texture = SDL_CreateTextureFromSurface(renderer, select_surface);
        SDL_Rect select_text_rect = {select_button.x + 10, select_button.y + 15, select_surface->w, select_surface->h};
        SDL_RenderCopy(renderer, select_texture, NULL, &select_text_rect);

        SDL_Surface *start_surface = TTF_RenderUTF8_Solid(font, emulation_running ? "Stop" : "Start", image_loaded ? green : white);
        SDL_Texture *start_texture = SDL_CreateTextureFromSurface(renderer, start_surface);
        SDL_Rect start_text_rect = {start_button.x + 10, start_button.y + 15, start_surface->w, start_surface->h};
        SDL_RenderCopy(renderer, start_texture, NULL, &start_text_rect);

        // Status bar
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &status_bar);
        char status_text[512];
        snprintf(status_text, sizeof(status_text), "File: %s | Status: %s", filename, emulation_running ? "Emulation Running" : "Emulation Stopped");
        SDL_Surface *status_surface = TTF_RenderUTF8_Solid(font, status_text, white);
        SDL_Texture *status_texture = SDL_CreateTextureFromSurface(renderer, status_surface);
        SDL_Rect status_text_rect = {status_bar.x + 5, status_bar.y + 5, status_surface->w, status_surface->h};
        SDL_RenderCopy(renderer, status_texture, NULL, &status_text_rect);

        // Draw emulator screen
        SDL_Rect emulator_screen = {50, 120, 640, 400}; // Scaled for 320x200
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &emulator_screen);

        // Render screen content
        if (cpu.video_mode == 0) { // Text mode
            for (int y = 0; y < 25; y++) {
                for (int x = 0; x < 80; x++) {
                    int idx = (y * 80 + x) * 2;
                    char ch = cpu.video_memory_text[idx];
                    if (ch == 0) continue;
                    char str[2] = {ch, '\0'};
                    SDL_Surface *char_surface = TTF_RenderUTF8_Solid(font, str, white);
                    SDL_Texture *char_texture = SDL_CreateTextureFromSurface(renderer, char_surface);
                    SDL_Rect char_rect = {emulator_screen.x + x * 8, emulator_screen.y + y * 16, char_surface->w, char_surface->h};
                    SDL_RenderCopy(renderer, char_texture, NULL, &char_rect);
                    SDL_FreeSurface(char_surface);
                    SDL_DestroyTexture(char_texture);
                }
            }
        } else if (cpu.video_mode == 1) { // Graphics mode 320x200
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 320; x++) {
                    uint8_t color = cpu.video_memory_vga[y * 320 + x];
                    if (color == 0) continue; // Skip black
                    uint8_t r, g, b;
                    get_vga_color(color, &r, &g, &b);
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_Rect pixel = {emulator_screen.x + x * 2, emulator_screen.y + y * 2, 2, 2};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        // Display registers
        char reg_text[128];
        snprintf(reg_text, sizeof(reg_text), "AX=%04X BX=%04X CX=%04X DX=%04X CS:IP=%04X:%04X", 
                 cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.cs, cpu.ip);
        SDL_Surface *reg_surface = TTF_RenderUTF8_Solid(font, reg_text, white);
        SDL_Texture *reg_texture = SDL_CreateTextureFromSurface(renderer, reg_surface);
        SDL_Rect reg_text_rect = {50, 530, reg_surface->w, reg_surface->h};
        SDL_RenderCopy(renderer, reg_texture, NULL, &reg_text_rect);

        // Cleanup
        SDL_FreeSurface(select_surface);
        SDL_DestroyTexture(select_texture);
        SDL_FreeSurface(start_surface);
        SDL_DestroyTexture(start_texture);
        SDL_FreeSurface(status_surface);
        SDL_DestroyTexture(status_texture);
        SDL_FreeSurface(reg_surface);
        SDL_DestroyTexture(reg_texture);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}