#pragma once
#include <stdbool.h>

void console_task(void);
void console_line_state(bool dtr, bool rts);
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
