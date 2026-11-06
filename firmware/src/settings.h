#pragma once
#include <stdint.h>

#define SETTING_POWER_ALLOW_LEAK  (1u << 0)   // power on with leakage VTGT (< 2.5 V)

int settings_load_mode(void);        // -1 when nothing valid is saved
void settings_save_mode(int mode);
uint32_t settings_load_flags(void);  // 0 when nothing valid is saved
void settings_save_flags(uint32_t flags);
