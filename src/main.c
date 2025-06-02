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

        BeginDrawing();
        ClearBackground(DARKGRAY);

        char ops_text[32];
        snprintf(ops_text, sizeof(ops_text), "OPS: %.0f", ops);
        Vector2 text_size = MeasureTextEx(font, ops_text, char_height, 1);
        DrawTextEx(font, ops_text, 
                   (Vector2){(window_width - text_size.x) / 2, screen_y - char_height - 5}, 
                   char_height, 1, WHITE);

        draw_screen(&cpu, screen_x, screen_y, char_width, char_height, font);
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
