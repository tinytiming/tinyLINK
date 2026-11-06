#pragma once
#include <stdbool.h>

void console_task(void);
void console_line_state(bool dtr, bool rts);
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// For messages that can arrive while the prompt is shown: redraws it after.
void console_notify(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
