#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Runs USB, power, LED, button, target UART and console. Blocking helpers
// below call it while they wait, so protocol modes can be written as
// straight-line parsers.
void usbio_service(void);

int usbio_getc(int itf, uint32_t timeout_ms);            // -1 on timeout
bool usbio_read(int itf, void *buf, size_t len, uint32_t timeout_ms);
void usbio_write(int itf, const void *buf, size_t len);
void usbio_putc(int itf, uint8_t c);
void usbio_flush(int itf);
