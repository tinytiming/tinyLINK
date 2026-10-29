#pragma once
#include <stdint.h>

#define RGB_GREEN   0x00FF00
#define RGB_PURPLE  0x8000FF
#define RGB_YELLOW  0xFFC000
#define RGB_WHITE   0xFFFFFF
#define RGB_BLUE    0x0000FF
#define RGB_ORANGE  0xFF4000
#define RGB_CYAN    0x00FFFF
#define RGB_RED     0xFF0000

void led_init(void);
void led_set(uint32_t rgb);              // steady mode colour
void led_blink(uint32_t rgb, int count); // blink, then back to the mode colour
void led_task(void);
