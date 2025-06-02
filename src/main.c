#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include "cpu8086.h"

int main(void) {
    CPU8086 cpu;
    init_cpu(&cpu);

    if (!load_firmware(&cpu, "bin/proshivka.bin")) {
        return 1;
    }

    const int window_width = 800;
    const int window_height = 600;
    const int char_width = 10;
    const int char_height = 20;
    const int screen_width_pixels = SCREEN_WIDTH * char_width;
    const int screen_height_pixels = SCREEN_HEIGHT * char_height;
    const int screen_x = (window_width - screen_width_pixels) / 2;
    const int screen_y = (window_height - screen_height_pixels) / 2 + 40;

    InitWindow(window_width, window_height, "8086 Emulators");
    SetTargetFPS(60);

    
    Font font = LoadFont("include/terminus.ttf");
    if (font.texture.id == 0) {
        font = GetFontDefault();
    }

    
    bool auto_run = true;
    unsigned long instruction_count = 0;
    float ops_timer = 0.0f;
    float ops = 0.0f;
    const float ops_update_interval = 1.0f;

    
    Color bg_color = (Color){40, 40, 40, 255};
    Color border_color = (Color){80, 80, 80, 255};
    Color accent_color = (Color){100, 180, 100, 255};
    Color text_color = (Color){200, 200, 200, 255};
    Color panel_color = (Color){20, 20, 20, 200};
    Color changed_color = (Color){255, 165, 0, 255}; 

    
    uint16_t prev_ax = cpu.ax, prev_bx = cpu.bx, prev_cx = cpu.cx, prev_dx = cpu.dx;
    uint16_t prev_cs = cpu.cs, prev_ds = cpu.ds, prev_es = cpu.es, prev_ss = cpu.ss, prev_ip = cpu.ip;
    uint8_t prev_carry = cpu.flags.carry, prev_zero = cpu.flags.zero, prev_sign = cpu.flags.sign;
    uint8_t prev_overflow = cpu.flags.overflow, prev_parity = cpu.flags.parity;
    uint8_t prev_auxiliary = cpu.flags.auxiliary, prev_interrupt = cpu.flags.interrupt;

	printf("Memory size: %d bytes\n", MEMORY_SIZE);
	printf("Screen size: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
	
    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        ops_timer += delta_time;

       
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
            instruction_count++;
        }

        if (auto_run && cpu.running) {
            for (int i = 0; i < 100000; i++) {
                if (!cpu.running) break;
                execute_instruction(&cpu);
                instruction_count++;
            }
        }

        if (ops_timer >= ops_update_interval) {
            ops = instruction_count / ops_timer;
            instruction_count = 0;
            ops_timer = 0.0f;
        }

        // === GUI Rendering ===
        BeginDrawing();
        ClearBackground(bg_color);

        // Title
        const char *title = "8086 Emulator";
        Vector2 title_size = MeasureTextEx(font, title, 24, 2);
        DrawTextEx(font, title, 
                   (Vector2){(window_width - title_size.x) / 2, 10}, 
                   24, 2, text_color);

        // Screen border and background
        DrawRectangle(screen_x - 6, screen_y - 6, screen_width_pixels + 12, screen_height_pixels + 12, border_color);
        DrawRectangle(screen_x - 4, screen_y - 4, screen_width_pixels + 8, screen_height_pixels + 8, panel_color);
        draw_screen(&cpu, screen_x, screen_y, char_width, char_height, font);

        // Status panel
        DrawRectangle(0, window_height - 40, window_width, 40, panel_color);
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "Status: %s  |  OP/S: %.0f  |  Auto-run: %s  |  Press [SPACE] to Step",
                 cpu.running ? "RUNNING" : "HALTED", ops, auto_run ? "ON" : "OFF");
        Vector2 status_size = MeasureTextEx(font, status_text, 18, 1);
        DrawTextEx(font, status_text, 
                   (Vector2){(window_width - status_size.x) / 2, window_height - 30}, 
                   18, 1, accent_color);

        // Mini debug panel for registers
        char reg_text[128];
        snprintf(reg_text, sizeof(reg_text), "AX: 0x%04X  BX: 0x%04X  CX: 0x%04X  DX: 0x%04X",
                 cpu.ax, cpu.bx, cpu.cx, cpu.dx);
        DrawTextEx(font, reg_text, 
                   (Vector2){10, window_height - 70}, 
                   16, 1, text_color);
        // Highlight changed registers
        if (cpu.ax != prev_ax) DrawRectangle(10, window_height - 70, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.bx != prev_bx) DrawRectangle(90, window_height - 70, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.cx != prev_cx) DrawRectangle(170, window_height - 70, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.dx != prev_dx) DrawRectangle(250, window_height - 70, 60, 16, Fade(changed_color, 0.3f));

        // Debug panel for segment registers
        char seg_text[128];
        snprintf(seg_text, sizeof(seg_text), "CS: 0x%04X  DS: 0x%04X  ES: 0x%04X  SS: 0x%04X  IP: 0x%04X",
                 cpu.cs, cpu.ds, cpu.es, cpu.ss, cpu.ip);
        DrawTextEx(font, seg_text, 
                   (Vector2){10, window_height - 90}, 
                   16, 1, text_color);
        // Highlight changed segment registers
        if (cpu.cs != prev_cs) DrawRectangle(10, window_height - 90, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.ds != prev_ds) DrawRectangle(90, window_height - 90, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.es != prev_es) DrawRectangle(170, window_height - 90, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.ss != prev_ss) DrawRectangle(250, window_height - 90, 60, 16, Fade(changed_color, 0.3f));
        if (cpu.ip != prev_ip) DrawRectangle(330, window_height - 90, 60, 16, Fade(changed_color, 0.3f));

        // Debug panel for flags
        char flags_text[128];
        snprintf(flags_text, sizeof(flags_text), "Flags: C:%d Z:%d S:%d O:%d P:%d A:%d I:%d",
                 cpu.flags.carry, cpu.flags.zero, cpu.flags.sign, cpu.flags.overflow,
                 cpu.flags.parity, cpu.flags.auxiliary, cpu.flags.interrupt);
        DrawTextEx(font, flags_text, 
                   (Vector2){10, window_height - 110}, 
                   16, 1, text_color);
        // Highlight changed flags
        if (cpu.flags.carry != prev_carry) DrawRectangle(60, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.zero != prev_zero) DrawRectangle(90, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.sign != prev_sign) DrawRectangle(120, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.overflow != prev_overflow) DrawRectangle(150, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.parity != prev_parity) DrawRectangle(180, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.auxiliary != prev_auxiliary) DrawRectangle(210, window_height - 110, 20, 16, Fade(changed_color, 0.3f));
        if (cpu.flags.interrupt != prev_interrupt) DrawRectangle(240, window_height - 110, 20, 16, Fade(changed_color, 0.3f));

        // Update previous values for next frame
        prev_ax = cpu.ax; prev_bx = cpu.bx; prev_cx = cpu.cx; prev_dx = cpu.dx;
        prev_cs = cpu.cs; prev_ds = cpu.ds; prev_es = cpu.es; prev_ss = cpu.ss; prev_ip = cpu.ip;
        prev_carry = cpu.flags.carry; prev_zero = cpu.flags.zero; prev_sign = cpu.flags.sign;
        prev_overflow = cpu.flags.overflow; prev_parity = cpu.flags.parity;
        prev_auxiliary = cpu.flags.auxiliary; prev_interrupt = cpu.flags.interrupt;

        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
