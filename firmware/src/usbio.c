// Background service loop and blocking CDC helpers for the protocol modes.
#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "button.h"
#include "cdc_uart.h"
#include "console.h"
#include "led.h"
#include "modes.h"
#include "power.h"
#include "usbio.h"

void usbio_service(void) {
  tud_task();
  power_task();
  led_task();
  cdc_uart_task();
  console_task();

  switch (button_task()) {
  case BUTTON_SHORT:
    mode_select((mode_id_t)((mode_current + 1) % MODE_COUNT));
    break;
  case BUTTON_LONG: {
    const char *err = power_set(!power_is_on());
    if (err) {
      led_blink(RGB_RED, 2);
      console_printf("target power: %s\r\n", err);
    } else {
      led_blink(RGB_WHITE, 1);
    }
    break;
  }
  default:
    break;
  }
}

int usbio_getc(int itf, uint32_t timeout_ms) {
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  for (;;) {
    if (tud_cdc_n_available((uint8_t)itf)) {
      uint8_t c;
      tud_cdc_n_read((uint8_t)itf, &c, 1);
      return c;
    }
    if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
      return -1;
    usbio_service();
  }
}

bool usbio_read(int itf, void *buf, size_t len, uint32_t timeout_ms) {
  uint8_t *p = buf;
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  while (len) {
    uint32_t n = tud_cdc_n_read((uint8_t)itf, p, (uint32_t)len);
    if (n) {
      p += n;
      len -= n;
      end = make_timeout_time_ms(timeout_ms);
      continue;
    }
    if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
      return false;
    usbio_service();
  }
  return true;
}

void usbio_write(int itf, const void *buf, size_t len) {
  const uint8_t *p = buf;
  absolute_time_t end = make_timeout_time_ms(1000);
  while (len) {
    uint32_t n = tud_cdc_n_write((uint8_t)itf, p, (uint32_t)len);
    p += n;
    len -= n;
    if (len) {
      tud_cdc_n_write_flush((uint8_t)itf);
      if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
        return;   // host stopped reading
      usbio_service();
    }
  }
}

void usbio_putc(int itf, uint8_t c) {
  usbio_write(itf, &c, 1);
}

void usbio_flush(int itf) {
  tud_cdc_n_write_flush((uint8_t)itf);
}
