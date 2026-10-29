#pragma once
#include <stdint.h>
#include "tusb.h"

void cdc_uart_init(void);
void cdc_uart_task(void);
void cdc_uart_line_coding(cdc_line_coding_t const *coding);
void cdc_uart_break(uint16_t ms);
