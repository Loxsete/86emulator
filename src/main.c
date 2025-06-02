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
