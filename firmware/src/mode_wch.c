// WCH mode: SWIO (CH32V003/V00x 1-wire debug) on IO1, driven by the
// PicoRVD PIO program, behind minichlink's "ardulink" serial protocol:
//   minichlink -c /dev/ttyACM0 -w firmware.bin flash -b
//   '?'            -> '!'               sync (also resets the debug link)
//   'w' reg d0..d3 -> '+'               write DMI register (little endian)
//   'r' reg        -> d0..d3            read DMI register
//   'p' / 'P'      -> '+' or '-'        target power on / off
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/structs/iobank0.h"
#include "board.h"
#include "modes.h"
#include "power.h"
#include "usbio.h"
#include "swio.pio.h"

#define TIMEOUT_MS 1000

// WCH debug module config registers
#define WCH_DM_CFGR     0x7D
#define WCH_DM_SHDWCFGR 0x7E

static PIO pio = pio0;
static uint sm, offset;

static void swio_put(uint8_t addr, uint32_t data) {
  pio_sm_put_blocking(pio, sm, ((~(uint32_t)addr) << 1) | 0);
  pio_sm_put_blocking(pio, sm, ~data);
}

static bool swio_get(uint8_t addr, uint32_t *data) {
  pio_sm_put_blocking(pio, sm, ((~(uint32_t)addr) << 1) | 1);
  absolute_time_t end = make_timeout_time_ms(10);
  while (pio_sm_is_rx_fifo_empty(pio, sm)) {
    if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
      return false;
  }
  *data = pio_sm_get(pio, sm);
  return true;
}

// 8 us low pulse resets the target's debug link, then enable its output.
static void swio_reset(void) {
  pio_sm_set_enabled(pio, sm, false);
  pio_sm_clear_fifos(pio, sm);
  pio_sm_restart(pio, sm);
  pio_sm_exec(pio, sm, pio_encode_jmp(offset));
  pio_sm_set_enabled(pio, sm, true);

  sio_hw->gpio_clr = 1u << PIN_SWIO;
  sio_hw->gpio_oe_set = 1u << PIN_SWIO;
  io_bank0_hw->io[PIN_SWIO].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait_us(8);
  sio_hw->gpio_oe_clr = 1u << PIN_SWIO;
  io_bank0_hw->io[PIN_SWIO].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  swio_put(WCH_DM_SHDWCFGR, 0x5AA50400);
  swio_put(WCH_DM_CFGR, 0x5AA50400);
}

void wch_mode_init(void) {
  gpio_set_drive_strength(PIN_SWIO, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate(PIN_SWIO, GPIO_SLEW_RATE_SLOW);
  gpio_pull_up(PIN_SWIO);
  pio_gpio_init(pio, PIN_SWIO);

  sm = pio_claim_unused_sm(pio, true);
  offset = pio_add_program(pio, &swio_program);

  pio_sm_config c = swio_program_get_default_config(offset);
  sm_config_set_sideset(&c, 1, false, true);   // side-set drives pindirs
  sm_config_set_out_pins(&c, PIN_SWIO, 1);
  sm_config_set_in_pins(&c, PIN_SWIO);
  sm_config_set_set_pins(&c, PIN_SWIO, 1);
  sm_config_set_sideset_pins(&c, PIN_SWIO);
  sm_config_set_out_shift(&c, false, false, 32);
  sm_config_set_in_shift(&c, false, true, 32);
  sm_config_set_clkdiv(&c, 12);   // ~96 ns per tick at 125 MHz
  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_pins(pio, sm, 0);
  pio_sm_set_enabled(pio, sm, true);
}

void wch_mode_task(void) {
  int cmd = usbio_getc(cdc_proto, 0);
  if (cmd < 0)
    return;

  switch (cmd) {
  case '?':
    swio_reset();
    usbio_putc(cdc_proto, '!');
    break;
  case 'w': {
    uint8_t b[5];
    if (!usbio_read(cdc_proto, b, 5, TIMEOUT_MS))
      break;
    swio_put(b[0], b[1] | (uint32_t)b[2] << 8 | (uint32_t)b[3] << 16 | (uint32_t)b[4] << 24);
    usbio_putc(cdc_proto, '+');
    break;
  }
  case 'r': {
    int reg = usbio_getc(cdc_proto, TIMEOUT_MS);
    if (reg < 0)
      break;
    uint32_t v = 0;
    if (!swio_get((uint8_t)reg, &v))
      swio_reset();   // stuck: resync, answer 0 so minichlink retries
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    usbio_write(cdc_proto, b, 4);
    break;
  }
  case 'p': {
    // A self-powered target is fine; only a dead VTGT is an error.
    power_set(true);
    power_wait_on(25);
    usbio_putc(cdc_proto, power_vtgt_mv() > 1000 ? '+' : '-');
    break;
  }
  case 'P':
    power_set(false);
    usbio_putc(cdc_proto, '+');
    break;
  default:
    break;
  }
  usbio_flush(cdc_proto);
}
