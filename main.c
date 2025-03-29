#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

// CPU structure
typedef struct {
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t ip;
    uint16_t cs, ds, ss, es;
    uint16_t flags;
    uint8_t memory[0x100000];
    uint8_t video_memory_text[80 * 25 * 2];
    uint8_t video_memory_vga[640 * 480];
    int video_mode;  // 0 - text, 1 - 320x200, 2 - 640x480
    uint8_t key_state[ALLEGRO_KEY_MAX];
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

// Load image file
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

// Handle interrupts
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
                } else if (al == 0x12) {
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
                    cpu->cx++;
                }
            }
            break;
        }
        case 0x16: { // Keyboard
            uint8_t ah = cpu->ax >> 8;
            if (ah == 0x00) {
                for (int i = 0; i < ALLEGRO_KEY_MAX; i++) {
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

// Emulate one instruction
void step_cpu(CPU *cpu) {
    uint32_t addr = get_physical_address(cpu, cpu->cs, cpu->ip);
    uint8_t opcode = cpu->memory[addr];
    uint32_t stack_addr;

    switch (opcode) {
        case 0xB8: cpu->ax = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xB9: cpu->cx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBA: cpu->dx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBB: cpu->bx = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBC: cpu->sp = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBD: cpu->bp = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBE: cpu->si = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;
        case 0xBF: cpu->di = cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8); cpu->ip += 3; break;

        case 0xB0: cpu->ax = (cpu->ax & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break;
        case 0xB1: cpu->cx = (cpu->cx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break;
        case 0xB2: cpu->dx = (cpu->dx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break;
        case 0xB3: cpu->bx = (cpu->bx & 0xFF00) | cpu->memory[addr + 1]; cpu->ip += 2; break;
        case 0xB4: cpu->ax = (cpu->ax & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break;
        case 0xB5: cpu->cx = (cpu->cx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break;
        case 0xB6: cpu->dx = (cpu->dx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break;
        case 0xB7: cpu->bx = (cpu->bx & 0x00FF) | (cpu->memory[addr + 1] << 8); cpu->ip += 2; break;

        case 0x01: cpu->ax += cpu->bx; cpu->flags = (cpu->ax == 0) ? 0x40 : 0; cpu->ip += 2; break;
        case 0x29: cpu->ax -= cpu->bx; cpu->flags = (cpu->ax == 0) ? 0x40 : 0; cpu->ip += 2; break;
        case 0x3B: {
            uint16_t res = cpu->ax - cpu->bx;
            cpu->flags = (res == 0) ? 0x40 : 0;
            cpu->flags |= (res & 0x8000) ? 0x80 : 0;
            cpu->flags |= (cpu->ax < cpu->bx) ? 0x01 : 0;
            cpu->ip += 2;
            break;
        }
        case 0xF6: {
            uint8_t modrm = cpu->memory[addr + 1];
            if ((modrm & 0x38) == 0x20) {
                cpu->ax = (cpu->ax & 0xFF) * cpu->memory[get_physical_address(cpu, cpu->ds, cpu->bx)];
                cpu->ip += 2;
            }
            break;
        }
        case 0x74: cpu->ip += (cpu->flags & 0x40) ? (int8_t)cpu->memory[addr + 1] + 2 : 2; break;
        case 0x75: cpu->ip += !(cpu->flags & 0x40) ? (int8_t)cpu->memory[addr + 1] + 2 : 2; break;
        case 0xE9: cpu->ip += (int16_t)(cpu->memory[addr + 1] | (cpu->memory[addr + 2] << 8)) + 3; break;
        case 0xEB: cpu->ip += (int8_t)cpu->memory[addr + 1] + 2; break;

        case 0x50: cpu->sp -= 2; stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
                   cpu->memory[stack_addr] = cpu->ax & 0xFF;
                   cpu->memory[stack_addr + 1] = cpu->ax >> 8; cpu->ip++; break;
        case 0x58: stack_addr = get_physical_address(cpu, cpu->ss, cpu->sp);
                   cpu->ax = cpu->memory[stack_addr] | (cpu->memory[stack_addr + 1] << 8);
                   cpu->sp += 2; cpu->ip++; break;

        case 0xE8: {
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
                   cpu->sp += 2; break;

        case 0xC6: if (cpu->memory[addr + 1] == 0x06) {
            uint32_t mem_addr = get_physical_address(cpu, cpu->ds, cpu->memory[addr + 2] | (cpu->memory[addr + 3] << 8));
            if (cpu->video_mode == 1 && mem_addr >= 0xA0000 && mem_addr < 0xA0000 + 320 * 200)
                cpu->video_memory_vga[mem_addr - 0xA0000] = cpu->memory[addr + 4];
            else
                cpu->memory[mem_addr] = cpu->memory[addr + 4];
            cpu->ip += 5;
        } break;

        case 0xCD: handle_interrupt(cpu); break;
        case 0xF4: cpu->ip++; break;
        case 0xFA: cpu->flags &= ~0x200; cpu->ip++; break;
        case 0xFB: cpu->flags |= 0x200; cpu->ip++; break;

        default: cpu->ip++; break;
    }
}

void get_vga_color(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0xE0);
    *g = (color & 0x1C) << 3;
    *b = (color & 0x03) << 6;
}


int main(int argc, char *argv[]) {
    if (!al_init()) {
        fprintf(stderr, "Failed to initialize Allegro\n");
        return 1;
    }
    if (!al_init_font_addon() || !al_init_ttf_addon()) {
        fprintf(stderr, "Failed to initialize font addon\n");
        return 1;
    }
    if (!al_init_primitives_addon()) {
        fprintf(stderr, "Failed to initialize primitives addon\n");
        return 1;
    }
    if (!al_install_mouse()) {
        fprintf(stderr, "Failed to initialize mouse\n");
        return 1;
    }
    if (!al_install_keyboard()) {
        fprintf(stderr, "Failed to initialize keyboard\n");
        return 1;
    }

    ALLEGRO_DISPLAY *display = al_create_display(800, 600);
    if (!display) {
        fprintf(stderr, "Failed to create display\n");
        return 1;
    }
    al_set_window_title(display, "8086 Emulator");

    ALLEGRO_EVENT_QUEUE *queue = al_create_event_queue();
    if (!queue) {
        fprintf(stderr, "Failed to create event queue\n");
        al_destroy_display(display);
        return 1;
    }

    // Попытка загрузить шрифт с обработкой ошибки
    ALLEGRO_FONT *font = al_load_ttf_font("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 16, 0);
    if (!font) {
        fprintf(stderr, "Failed to load font from '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'. "
                       "Please ensure the font is installed or use a different font path.\n");
        // Попробуем альтернативный шрифт
        font = al_load_ttf_font("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", 16, 0);
        if (!font) {
            fprintf(stderr, "Failed to load fallback font. Exiting.\n");
            al_destroy_event_queue(queue);
            al_destroy_display(display);
            return 1;
        } else {
            fprintf(stderr, "Using fallback font: LiberationMono-Regular.ttf\n");
        }
    }

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_keyboard_event_source());

    CPU cpu;
    reset_cpu(&cpu);
    int running = 1, emulation_running = 0, image_loaded = 0, button_pressed = 0;
    char filename[256] = "No file";

    if (argc > 1) {
        strncpy(filename, argv[1], sizeof(filename) - 1);
        image_loaded = load_image(&cpu, filename);
    }

    while (running) {
        ALLEGRO_EVENT event;
        while (al_get_next_event(queue, &event)) {
            if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) running = 0;
            if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && event.mouse.button == 1) {
                button_pressed = 1;
                int x = event.mouse.x, y = event.mouse.y;
                if (x >= 50 && x <= 250 && y >= 20 && y <= 70) { // Select button
                    printf("Enter .img file path: ");
                    scanf("%255s", filename);
                    image_loaded = load_image(&cpu, filename);
                }
                if (x >= 300 && x <= 500 && y >= 20 && y <= 70 && image_loaded) // Start button
                    emulation_running = !emulation_running;
            }
            if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) button_pressed = 0;
            if (event.type == ALLEGRO_EVENT_KEY_DOWN)
                cpu.key_state[event.keyboard.keycode] = 1;
            if (event.type == ALLEGRO_EVENT_KEY_UP)
                cpu.key_state[event.keyboard.keycode] = 0;
        }

        if (emulation_running) step_cpu(&cpu);

        al_clear_to_color(al_map_rgb(30, 30, 30));

        ALLEGRO_MOUSE_STATE mouse_state;
        al_get_mouse_state(&mouse_state);

        al_draw_filled_rectangle(50, 20, 250, 70, 
            button_pressed && al_mouse_button_down(&mouse_state, 1) && 
            al_get_mouse_state_axis(&mouse_state, 0) >= 50 && 
            al_get_mouse_state_axis(&mouse_state, 0) <= 250 && 
            al_get_mouse_state_axis(&mouse_state, 1) >= 20 && 
            al_get_mouse_state_axis(&mouse_state, 1) <= 70 
            ? al_map_rgb(150, 150, 150) : al_map_rgb(100, 100, 100));
        
        al_draw_filled_rectangle(300, 20, 500, 70, 
            button_pressed && al_mouse_button_down(&mouse_state, 1) && 
            al_get_mouse_state_axis(&mouse_state, 0) >= 300 && 
            al_get_mouse_state_axis(&mouse_state, 0) <= 500 && 
            al_get_mouse_state_axis(&mouse_state, 1) >= 20 && 
            al_get_mouse_state_axis(&mouse_state, 1) <= 70 
            ? al_map_rgb(150, 150, 150) : al_map_rgb(100, 100, 100));

        al_draw_text(font, al_map_rgb(255, 255, 255), 60, 35, 0, "Select Image");
        al_draw_text(font, al_map_rgb(image_loaded ? 0 : 255, image_loaded ? 255 : 255, 255), 310, 35, 0, emulation_running ? "Stop" : "Start");

        al_draw_filled_rectangle(50, 80, 750, 110, al_map_rgb(50, 50, 50));
        char status_text[512];
        snprintf(status_text, sizeof(status_text), "File: %s | Status: %s", filename, emulation_running ? "Running" : "Stopped");
        al_draw_text(font, al_map_rgb(255, 255, 255), 55, 85, 0, status_text);

        al_draw_filled_rectangle(50, 120, 690, 520, al_map_rgb(0, 0, 0));
        if (cpu.video_mode == 0) {
            for (int y = 0; y < 25; y++) {
                for (int x = 0; x < 80; x++) {
                    char ch = cpu.video_memory_text[(y * 80 + x) * 2];
                    if (ch) {
                        char str[2] = {ch, 0};
                        al_draw_text(font, al_map_rgb(255, 255, 255), 50 + x * 8, 120 + y * 16, 0, str);
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
                        al_draw_filled_rectangle(50 + x * 2, 120 + y * 2, 52 + x * 2, 122 + y * 2, al_map_rgb(r, g, b));
                    }
                }
            }
        }

        char reg_text[128];
        snprintf(reg_text, sizeof(reg_text), "AX=%04X BX=%04X CX=%04X DX=%04X CS:IP=%04X:%04X", 
                 cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.cs, cpu.ip);
        al_draw_text(font, al_map_rgb(255, 255, 255), 50, 530, 0, reg_text);

        al_flip_display();
        al_rest(0.016); // ~60 FPS
    }

    al_destroy_font(font);
    al_destroy_event_queue(queue);
    al_destroy_display(display);
    return 0;
}
