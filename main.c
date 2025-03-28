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
    uint16_t cs, ds, ss, es;
    uint16_t flags;
    uint8_t memory[0x100000];
    uint8_t video_memory_text[80 * 25 * 2];
    uint8_t video_memory_vga[640 * 480];  // Увеличено для 640x480
    int video_mode;  // 0 - text, 1 - 320x200, 2 - 640x480
    uint8_t key_state[SDL_NUM_SCANCODES];
} CPU;

// Initialize CPU
void reset_cpu(CPU *cpu) {
    cpu->ax = cpu->bx = cpu->cx = cpu->dx = 0;
    cpu->si = cpu->di = cpu->bp = cpu->sp = 0;
    cpu->ip = 0x7C00;
    cpu->cs = 0x0000;
    cpu->ds = 0x0000;
    cpu->ss = 0x0000;
    cpu->es = 0x0000;
    cpu->flags = 0x200;
    cpu->video_mode = 0;
    memset(cpu->memory, 0, sizeof(cpu->memory));
    memset(cpu->video_memory_text, 0, sizeof(cpu->video_memory_text));
    memset(cpu->video_memory_vga, 0, sizeof(cpu->video_memory_vga));
    memset(cpu->key_state, 0, sizeof(cpu->key_state));
}

// Load image file (без изменений)
int load_image(CPU *cpu, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 512) {
        fclose(file);
        return 0;
    }
    uint8_t buffer[512];
    fread(buffer, 1, 512, file);
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        fclose(file);
        return 0;
    }
    rewind(file);
    fread(cpu->memory + 0x7C00, 1, 512, file);
    if (size > 512) {
        fseek(file, 512, SEEK_SET);
        long remaining = size - 512;
        if (remaining > 0x100000 - 0x7E00) remaining = 0x100000 - 0x7E00;
        fread(cpu->memory + 0x7E00, 1, remaining, file);
    }
    fclose(file);
    return 1;
}

uint32_t get_physical_address(CPU *cpu, uint16_t segment, uint16_t offset) {
    return (segment << 4) + offset;
}

// Расширенная обработка прерываний для новых видеорежимов
void handle_interrupt(CPU *cpu) {
    uint32_t addr = get_physical_address(cpu, cpu->cs, cpu->ip);
    uint8_t int_num = cpu->memory[addr + 1];
    switch (int_num) {
        case 0x10: { // Video
            uint8_t ah = cpu->ax >> 8;
            if (ah == 0x00) {
                uint8_t al = cpu->ax & 0xFF;
                if (al == 0x03) {
                    cpu->video_mode = 0;
                    memset(cpu->video_memory_text, 0, sizeof(cpu->video_memory_text));
                } else if (al == 0x13) {
                    cpu->video_mode = 1;
                    memset(cpu->video_memory_vga, 0, sizeof(cpu->video_memory_vga));
                } else if (al == 0x12) {  // Добавлен режим 640x480
                    cpu->video_mode = 2;
                    memset(cpu->video_memory_vga, 0, sizeof(cpu->video_memory_vga));
                }
            } else if (ah == 0x0E && cpu->video_mode == 0) {
                uint8_t al = cpu->ax & 0xFF;
                uint8_t bl = cpu->bx & 0xFF;
                uint16_t cursor_pos = cpu->cx;
                if (cursor_pos < 80 * 25) {
                    cpu->video_memory_text[cursor_pos * 2] = al;
                    cpu->video_memory_text[cursor_pos * 2 + 1] = bl;
                }
            }
            break;
        }
        case 0x16: { // Keyboard (без изменений)
            uint8_t ah = cpu->ax >> 8;
            if (ah == 0x00) {
                for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
                    if (cpu->key_state[i]) {
                        cpu->ax = (cpu->ax & 0xFF00) | i;
                        cpu->key_state[i] = 0;
                        break;
                    }
                }
            }
            break;
        }
    }
    cpu->ip += 2;
}

// Emulate one instruction (expanded opcodes)
void step_cpu(CPU *cpu) {
    uint32_t addr = get_physical_address(cpu, cpu->cs, cpu->ip);
    uint8_t opcode = cpu->memory[addr];
    uint32_t stack_addr;

    switch (opcode) {
        // Register moves
        case 0xB8: cpu->ax = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xB9: cpu->cx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBA: cpu->dx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBB: cpu->bx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBC: cpu->sp = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBD: cpu->bp = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBE: cpu->si = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBF: cpu->di = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;

        // 8-bit register moves
        case 0xB0: cpu->ax = (cpu->ax & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break; // AL
        case 0xB1: cpu->cx = (cpu->cx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break; // CL
        case 0xB2: cpu->dx = (cpu->dx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break; // DL
        case 0xB3: cpu->bx = (cpu->bx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break; // BL
        case 0xB4: cpu->ax = (cpu->ax & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break; // AH
        case 0xB5: cpu->cx = (cpu->cx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break; // CH
        case 0xB6: cpu->dx = (cpu->dx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break; // DH
        case 0xB7: cpu->bx = (cpu->bx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break; // BH

        // Arithmetic
        case 0x01: cpu->ax += cpu->bx; cpu->flags = (cpu->ax == 0) ? 0x40 : 0; cpu->ip += 2; break; // ADD AX,BX
        case 0x29: cpu->ax -= cpu->bx; cpu->flags = (cpu->ax == 0) ? 0x40 : 0; cpu->ip += 2; break; // SUB AX,BX
        case 0x3B: { // CMP AX,BX
            uint16_t res = cpu->ax - cpu->bx;
            cpu->flags = (res == 0) ? 0x40 : 0;
            cpu->flags |= (res & 0x8000) ? 0x80 : 0; // SF
            cpu->flags |= (cpu->ax < cpu->bx) ? 0x01 : 0; // CF
            cpu->ip += 2;
            break;
        }
        case 0xF6: { // MUL/DIV
            uint8_t modrm = cpu->memory[addr + 1];
            if ((modrm & 0x38) == 0x20) { // MUL
                cpu->ax = (cpu->ax & 0xFF) * cpu->memory[get_physical_address(cpu, cpu->ds, cpu->bx)];
                cpu->ip += 2;
            }
            break;
        }

        // Jumps
        case 0x74: cpu->ip += (cpu->flags & 0x40) ? (int8_t)cpu->memory[addr + 1] + 2 : 2; break; // JZ
        case 0x75: cpu->ip += !(cpu->flags & 0x40) ? (int8_t)cpu->memory[addr + 1] + 2 : 2; break; // JNZ
        case 0xE9: cpu->ip += (int16_t)(cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8)) + 3; break; // JMP near
        case 0xEB: cpu->ip += (int8_t)cpu->memory[addr + 1] + 2; break; // JMP short

        // Stack
        case 0x50: cpu->sp -= 2; stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp); 
                   cpu->memory[stack_addr] = cpu->ax & 0xFF; 
                   cpu->memory[stack_addr + 1] = cpu->ax >> 8; cpu->ip++; break; // PUSH AX
        case 0x58: stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp); 
                   cpu->ax = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8); 
                   cpu->sp += 2; cpu->ip++; break; // POP AX

        // Call/Return
        case 0xE8: { // CALL
            int16_t offset = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8);
            cpu->sp -= 2;
            stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
            uint16_t ret = cpu->ip + 3;
            cpu->memory[stack_addr] = ret & 0xFF;
            cpu->memory[stack_addr + 1] = ret >> 8;
            cpu->ip += offset + 3;
            break;
        }
        case 0xC3: stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp); 
                   cpu->ip = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8); 
                   cpu->sp += 2; break; // RET

        // Memory
        case 0xC6: if (cpu->memory[addr + 1] == 0x06) { // MOV byte [mem], imm8
            uint32_t mem_addr = get_physical_address(cpu, cpu->ds, cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8));
            if (cpu->video_mode == 1 && mem_addr >= 0xA0000 && mem_addr < 0xA0000 + 320 * 200)
                cpu->video_memory_vga[mem_addr - 0xA0000] = cpu->memory[addr + 4];
            else
                cpu->memory[mem_addr] = cpu->memory[addr + 4];
            cpu->ip += 5;
        } break;

        // Interrupts
        case 0xCD: handle_interrupt(cpu); break;
        case 0xF4: cpu->ip++; break; // HLT
        case 0xFA: cpu->flags &= ~0x200; cpu->ip++; break; // CLI
        case 0xFB: cpu->flags |= 0x200; cpu->ip++; break; // STI

        default: cpu->ip++; break;
    }
}

int is_point_in_rect(int x, int y, SDL_Rect *rect) {
    return (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h);
}

void get_vga_color(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0xE0);
    *g = (color & 0x1C) << 3;
    *b = (color & 0x03) << 6;
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *window = SDL_CreateWindow("8086 Emulator", SDL_WINDOWPOS_CENTERED, 
                                          SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font *font = TTF_OpenFont("arial.ttf", 16);
    if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!font) return 1;

    CPU cpu;
    reset_cpu(&cpu);
    int running = 1, emulation_running = 0, image_loaded = 0, button_pressed = 0;
    char filename[256] = "No file";

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
                int x = event.button.x, y = event.button.y;
                button_pressed = 1;
                if (is_point_in_rect(x, y, &select_button)) {
                    const char *file = tinyfd_openFileDialog("Select Image (.img)", "", 1, (const char *[]){"*.img"}, "Disk Images", 0);
                    if (file) {
                        strncpy(filename, file, sizeof(filename) - 1);
                        image_loaded = load_image(&cpu, filename);
                    }
                }
                if (is_point_in_rect(x, y, &start_button) && image_loaded)
                    emulation_running = !emulation_running;
            }
            if (event.type == SDL_MOUSEBUTTONUP) button_pressed = 0;
            if (event.type == SDL_KEYDOWN) cpu.key_state[event.key.keysym.scancode] = 1;
            if (event.type == SDL_KEYUP) cpu.key_state[event.key.keysym.scancode] = 0;
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        if (emulation_running) step_cpu(&cpu);

        SDL_Color select_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &select_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, select_color.r, select_color.g, select_color.b, 255);
        SDL_RenderFillRect(renderer, &select_button);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &select_button);

        SDL_Color start_color = button_pressed && is_point_in_rect(event.button.x, event.button.y, &start_button) ? gray : dark_gray;
        SDL_SetRenderDrawColor(renderer, start_color.r, start_color.g, start_color.b, 255);
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
        snprintf(status_text, sizeof(status_text), "File: %s | Status: %s", filename, emulation_running ? "Running" : "Stopped");
        SDL_Surface *status_surface = TTF_RenderUTF8_Solid(font, status_text, white);
        SDL_Texture *status_texture = SDL_CreateTextureFromSurface(renderer, status_surface);
        SDL_Rect status_text_rect = {status_bar.x + 5, status_bar.y + 5, status_surface->w, status_surface->h};
        SDL_RenderCopy(renderer, status_texture, NULL, &status_text_rect);

        SDL_Rect screen = {50, 120, 640, 400};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &screen);
        if (cpu.video_mode == 0) {
            for (int y = 0; y < 25; y++) {
                for (int x = 0; x < 80; x++) {
                    char ch = cpu.video_memory_text[(y * 80 + x) * 2];
                    if (ch) {
                        char str[2] = {ch, 0};
                        SDL_Surface *s = TTF_RenderUTF8_Solid(font, str, white);
                        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                        SDL_Rect r = {screen.x + x * 8, screen.y + y * 16, s->w, s->h};
                        SDL_RenderCopy(renderer, t, NULL, &r);
                        SDL_FreeSurface(s);
                        SDL_DestroyTexture(t);
                    }
                }
            }
        } else if (cpu.video_mode == 1) {
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 320; x++) {
                    uint8_t color = cpu.video_memory_vga[y * 320 + x];
                    if (color) {
                        uint8_t r, g, b;
                        get_vga_color(color, &r, &g, &b);
                        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                        SDL_Rect pixel = {screen.x + x * 2, screen.y + y * 2, 2, 2};
                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }

        char reg_text[128];
        snprintf(reg_text, sizeof(reg_text), "AX=%04X BX=%04X CX=%04X DX=%04X CS:IP=%04X:%04X", 
                 cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.cs, cpu.ip);
        SDL_Surface *reg_surface = TTF_RenderUTF8_Solid(font, reg_text, white);
        SDL_Texture *reg_texture = SDL_CreateTextureFromSurface(renderer, reg_surface);
        SDL_Rect reg_text_rect = {50, 530, reg_surface->w, reg_surface->h};
        SDL_RenderCopy(renderer, reg_texture, NULL, &reg_text_rect);

        SDL_FreeSurface(select_surface); SDL_DestroyTexture(select_texture);
        SDL_FreeSurface(start_surface); SDL_DestroyTexture(start_texture);
        SDL_FreeSurface(status_surface); SDL_DestroyTexture(status_texture);
        SDL_FreeSurface(reg_surface); SDL_DestroyTexture(reg_texture);

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
