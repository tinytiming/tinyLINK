#pragma once
#include <stdbool.h>
#include <stdint.h>

void power_init(void);
void power_task(void);

// Switch target power. Returns NULL on success (power-good is checked later
// by power_task), or why it refused.
const char *power_set(bool on);
bool power_wait_on(uint32_t timeout_ms);   // spins power_task until on or fault

bool power_is_on(void);
bool power_shifter_on(void);
int power_vtgt_mv(void);
