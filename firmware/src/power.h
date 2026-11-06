#pragma once
#include <stdbool.h>
#include <stdint.h>

void power_init(void);
void power_task(void);

// Switch target power. Returns NULL on success (power-good is checked later
// by power_task), or why it refused.
const char *power_set(bool on);
bool power_wait_on(uint32_t timeout_ms);   // spins power_task until on and settled, or fault

bool power_is_on(void);
bool power_shifter_on(void);
int power_vtgt_mv(void);

// Allow power-on while VTGT is only IO leakage (< 2.5 V) instead of < 0.5 V.
// Saved in flash, off by default. Returns NULL, or why it can't.
const char *power_allow_leak(bool allow);
bool power_leak_allowed(void);
