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
    InitWindow(window_width, window_height, "8086 Emulator");
    SetTargetFPS(60);

    Font font = LoadFont("include/terminus.ttf");
    if (font.texture.id == 0) {
        font = GetFontDefault();
    }

    const int char_width = 10;
    const int char_height = 20;
    const int screen_width_pixels = SCREEN_WIDTH * char_width;
    const int screen_height_pixels = SCREEN_HEIGHT * char_height;
    const int screen_x = (window_width - screen_width_pixels) / 2;
    const int screen_y = (window_height - screen_height_pixels) / 2 + 20;

    bool auto_run = true;
    unsigned long instruction_count = 0;
    float ops_timer = 0.0f;
    float ops = 0.0f;
    const float ops_update_interval = 1.0f;

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
        ClearBackground(DARKGRAY);

        
        const char *title = "8086 Emulator";
        Vector2 title_size = MeasureTextEx(font, title, 24, 2);
        DrawTextEx(font, title, 
                   (Vector2){(window_width - title_size.x) / 2, 10}, 
                   24, 2, RAYWHITE);

       
        DrawRectangle(screen_x - 4, screen_y - 4, screen_width_pixels + 8, screen_height_pixels + 8, BLACK); 
        DrawRectangle(screen_x - 2, screen_y - 2, screen_width_pixels + 4, screen_height_pixels + 4, GRAY);   

        draw_screen(&cpu, screen_x, screen_y, char_width, char_height, font);

        
        DrawRectangle(0, window_height - 30, window_width, 30, Fade(BLACK, 0.6f));
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "[%s]  OPS: %.0f  |  [A]uto-run: %s  |  [SPACE] Step",
                 cpu.running ? "RUNNING" : "HALTED", ops, auto_run ? "ON" : "OFF");
        DrawTextEx(font, status_text, (Vector2){10, window_height - 24}, 18, 1, GREEN);

        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
