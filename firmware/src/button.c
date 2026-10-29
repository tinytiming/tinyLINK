// The RP2040-Zero BOOT button sits on the QSPI CS strap, so it can only be
// read by briefly floating CS (pico-examples picoboard/button, BSD-3-Clause).
// Nothing may run from flash meanwhile: interrupts are off and core 1 is
// unused.
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "button.h"

#define POLL_MS       10
#define DEBOUNCE      3       // polls
#define LONG_PRESS_MS 1500

static bool __no_inline_not_in_flash_func(bootsel_pressed)(void) {
  const uint CS_PIN_INDEX = 1;
  uint32_t flags = save_and_disable_interrupts();

  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
  for (volatile int i = 0; i < 1000; ++i)
    ;
  bool pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

  restore_interrupts(flags);
  return pressed;
}

static absolute_time_t next_poll;
static int stable_count;
static bool state, long_sent;
static absolute_time_t pressed_at;

button_event_t button_task(void) {
  if (absolute_time_diff_us(next_poll, get_absolute_time()) < 0)
    return BUTTON_NONE;
  next_poll = make_timeout_time_ms(POLL_MS);

  bool raw = bootsel_pressed();
  if (raw == state) {
    stable_count = 0;
    if (state && !long_sent &&
        absolute_time_diff_us(pressed_at, get_absolute_time()) >= LONG_PRESS_MS * 1000) {
      long_sent = true;
      return BUTTON_LONG;
    }
    return BUTTON_NONE;
  }
  if (++stable_count < DEBOUNCE)
    return BUTTON_NONE;
  stable_count = 0;
  state = raw;
  if (state) {
    pressed_at = get_absolute_time();
    long_sent = false;
    return BUTTON_NONE;
  }
  return long_sent ? BUTTON_NONE : BUTTON_SHORT;
}
