#pragma once

typedef enum {
  BUTTON_NONE,
  BUTTON_SHORT,   // released before LONG_PRESS_MS: next mode
  BUTTON_LONG,    // held LONG_PRESS_MS: toggle target power
} button_event_t;

button_event_t button_task(void);
