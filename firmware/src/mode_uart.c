// UART bridge mode: the target UART port plus esptool-style auto-reset.
// Host RTS drives IO4 (target EN), host DTR drives IO5 (target IO0/BOOT),
// like the usual two-transistor circuit:
//   RTS DTR   EN    BOOT
//    0   0   high  high
//    1   0   low   high   (reset)
//    0   1   high  low    (boot select)
//    1   1   high  high
// Both pins are open-drain: driven low, or released to the TXS0108E (or
// target) pull-up.
#include "pico/stdlib.h"
#include "board.h"
#include "modes.h"

static void od_put(uint pin, bool high) {
  gpio_set_dir(pin, high ? GPIO_IN : GPIO_OUT);
}

void uart_mode_init(void) {
  const uint pins[] = { PIN_ESP_EN, PIN_ESP_BOOT };
  for (int i = 0; i < 2; i++) {
    gpio_init(pins[i]);
    gpio_put(pins[i], 0);
    gpio_set_dir(pins[i], GPIO_IN);
  }
}

void uart_mode_line_state(bool dtr, bool rts) {
  od_put(PIN_ESP_EN, !(rts && !dtr));
  od_put(PIN_ESP_BOOT, !(dtr && !rts));
}
