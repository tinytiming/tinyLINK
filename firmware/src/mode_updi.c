// UPDI mode: a SerialUPDI-compatible port for pymcuprog / avrdude
// (-c serialupdi). The protocol port is a half-duplex UART on IO1 with
// the host's own bytes echoed back. Baud rate, parity and stop bits follow
// the host (8E2 normally, 300 baud 8N1 for the double break).
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "tusb.h"
#include "board.h"
#include "modes.h"
#include "od_uart.pio.h"

static PIO pio = pio0;
static uint sm_tx, sm_rx, off_tx, off_rx;
static uint data_bits = 8, parity = 2, stop_bits = 2;   // 8E2

static void configure(uint32_t baud) {
  pio_sm_set_enabled(pio, sm_tx, false);
  pio_sm_set_enabled(pio, sm_rx, false);

  float div = (float)clock_get_hz(clk_sys) / (8.0f * (float)baud);
  if (div < 1.0f)
    div = 1.0f;
  if (div > 65535.0f)
    div = 65535.0f;

  pio_sm_config c = od_uart_tx_program_get_default_config(off_tx);
  sm_config_set_out_pins(&c, PIN_UPDI, 1);
  sm_config_set_out_shift(&c, true, false, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  sm_config_set_clkdiv(&c, div);
  pio_sm_init(pio, sm_tx, off_tx, &c);
  pio_sm_set_pins_with_mask(pio, sm_tx, 0, 1u << PIN_UPDI);
  pio_sm_set_pindirs_with_mask(pio, sm_tx, 0, 1u << PIN_UPDI);

  c = od_uart_rx_program_get_default_config(off_rx);
  sm_config_set_in_pins(&c, PIN_UPDI);
  sm_config_set_jmp_pin(&c, PIN_UPDI);
  sm_config_set_in_shift(&c, true, false, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv(&c, div);
  pio_sm_init(pio, sm_rx, off_rx, &c);
  uint nbits = data_bits + (parity ? 1 : 0);
  pio_sm_exec(pio, sm_rx, pio_encode_set(pio_y, nbits - 1));

  pio_sm_set_enabled(pio, sm_tx, true);
  pio_sm_set_enabled(pio, sm_rx, true);
}

void updi_mode_init(void) {
  gpio_pull_up(PIN_UPDI);
  pio_gpio_init(pio, PIN_UPDI);
  sm_tx = pio_claim_unused_sm(pio, true);
  sm_rx = pio_claim_unused_sm(pio, true);
  off_tx = pio_add_program(pio, &od_uart_tx_program);
  off_rx = pio_add_program(pio, &od_uart_rx_program);
  configure(115200);
}

void updi_mode_line_coding(cdc_line_coding_t const *coding) {
  data_bits = coding->data_bits >= 5 && coding->data_bits <= 8 ? coding->data_bits : 8;
  parity = coding->parity == 1 || coding->parity == 2 ? coding->parity : 0;
  stop_bits = coding->stop_bits == 0 ? 1 : 2;   // 1.5 rounds up
  configure(coding->bit_rate ? coding->bit_rate : 115200);
}

void updi_mode_break(uint16_t ms) {
  if (!ms)
    return;
  if (ms == 0xFFFF || ms > 100)
    ms = 25;   // UPDI break is >= 24.6 ms at the slowest UPDI clock
  gpio_init(PIN_UPDI);
  gpio_put(PIN_UPDI, 0);
  gpio_set_dir(PIN_UPDI, GPIO_OUT);
  sleep_ms(ms);
  gpio_set_dir(PIN_UPDI, GPIO_IN);
  sleep_us(100);
  pio_gpio_init(pio, PIN_UPDI);
  pio_sm_clear_fifos(pio, sm_rx);
}

static uint32_t frame(uint8_t c, uint *len) {
  uint32_t bits = (uint32_t)(c & ((1u << data_bits) - 1)) << 1;   // start bit 0
  uint n = 1 + data_bits;
  if (parity) {
    uint ones = (uint)__builtin_popcount(c & ((1u << data_bits) - 1));
    uint p = parity == 2 ? (ones & 1) : !(ones & 1);
    bits |= (uint32_t)p << n;
    n++;
  }
  for (uint i = 0; i < stop_bits; i++, n++)
    bits |= 1u << n;
  *len = n;
  return bits;
}

void updi_mode_task(void) {
  uint8_t itf = (uint8_t)cdc_proto;

  while (tud_cdc_n_available(itf) && !pio_sm_is_tx_fifo_full(pio, sm_tx)) {
    uint8_t c;
    tud_cdc_n_read(itf, &c, 1);
    uint len;
    uint32_t bits = frame(c, &len);
    uint32_t inv = ~bits & ((1u << len) - 1);
    pio_sm_put(pio, sm_tx, (inv << 4) | (len - 1));
  }

  uint nbits = data_bits + (parity ? 1 : 0);
  bool wrote = false;
  while (!pio_sm_is_rx_fifo_empty(pio, sm_rx) && tud_cdc_n_write_available(itf)) {
    uint32_t w = pio_sm_get(pio, sm_rx) >> (32 - nbits);
    uint8_t c = (uint8_t)(w & ((1u << data_bits) - 1));
    tud_cdc_n_write(itf, &c, 1);
    wrote = true;
  }
  if (wrote)
    tud_cdc_n_write_flush(itf);
}
