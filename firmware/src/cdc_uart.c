// Target UART on IO2 (TX) / IO3 (RX), hardware UART1, bridged to the
// "target UART" CDC port. RX is interrupt driven into a ring buffer so
// fast bursts survive while a protocol mode is busy.
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "board.h"
#include "cdc_uart.h"
#include "modes.h"

#define RX_BUF_SIZE 4096   // power of two

static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head, rx_tail;
static bool active;

static void on_uart_rx(void) {
  while (uart_is_readable(TARGET_UART)) {
    uint8_t c = (uint8_t)uart_get_hw(TARGET_UART)->dr;
    uint32_t next = (rx_head + 1) & (RX_BUF_SIZE - 1);
    if (next != rx_tail) {
      rx_buf[rx_head] = c;
      rx_head = next;
    }
  }
}

void cdc_uart_init(void) {
  if (cdc_uart < 0)
    return;
  uart_init(TARGET_UART, 115200);
  uart_set_fifo_enabled(TARGET_UART, true);
  gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
  gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);
  gpio_pull_up(PIN_UART_RX);

  int irq = TARGET_UART == uart0 ? UART0_IRQ : UART1_IRQ;
  irq_set_exclusive_handler(irq, on_uart_rx);
  irq_set_enabled(irq, true);
  uart_set_irq_enables(TARGET_UART, true, false);
  active = true;
}

void cdc_uart_task(void) {
  if (!active)
    return;
  uint8_t itf = (uint8_t)cdc_uart;

  // target -> host
  uint32_t head = rx_head, tail = rx_tail;
  // Not gated on DTR: esptool drops DTR while it resets the target and
  // still needs the bootloader's reply.
  if (head != tail) {
    uint32_t len = head > tail ? head - tail : RX_BUF_SIZE - tail;
    uint32_t n = tud_cdc_n_write(itf, &rx_buf[tail], len);
    rx_tail = (tail + n) & (RX_BUF_SIZE - 1);
    tud_cdc_n_write_flush(itf);
  }

  // host -> target
  while (tud_cdc_n_available(itf) && uart_is_writable(TARGET_UART)) {
    uint8_t c;
    tud_cdc_n_read(itf, &c, 1);
    uart_get_hw(TARGET_UART)->dr = c;
  }
}

void cdc_uart_line_coding(cdc_line_coding_t const *coding) {
  if (!active)
    return;
  uart_parity_t parity = coding->parity == 1 ? UART_PARITY_ODD :
                         coding->parity == 2 ? UART_PARITY_EVEN : UART_PARITY_NONE;
  uint data_bits = coding->data_bits >= 5 && coding->data_bits <= 8 ? coding->data_bits : 8;
  uint stop_bits = coding->stop_bits == 2 ? 2 : 1;
  uart_set_baudrate(TARGET_UART, coding->bit_rate ? coding->bit_rate : 115200);
  uart_set_format(TARGET_UART, data_bits, stop_bits, parity);
}

void cdc_uart_break(uint16_t ms) {
  if (!active)
    return;
  // 0xFFFF = until cleared, 0 = clear
  uart_set_break(TARGET_UART, ms != 0);
  if (ms && ms != 0xFFFF) {
    sleep_ms(ms > 250 ? 250 : ms);
    uart_set_break(TARGET_UART, false);
  }
}
