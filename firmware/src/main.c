// tinyLINK: multi-mode debug probe firmware for the RP2040-Zero carrier
// (../hardware). The BOOT button cycles modes, the WS2812 shows the mode,
// a long press toggles target power.
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include "board.h"
#include "cdc_uart.h"
#include "console.h"
#include "led.h"
#include "modes.h"
#include "power.h"
#include "settings.h"
#include "usb_descriptors.h"
#include "usbio.h"

const probe_mode_t modes[MODE_COUNT] = {
  [MODE_SWD]  = { "swd",  "tinyLINK CMSIS-DAP",          RGB_GREEN,  true,  false, true,
                  dap_mode_init_swd, dap_mode_task, NULL, NULL, NULL },
  [MODE_WCH]  = { "wch",  "tinyLINK WCH SWIO (ardulink)", RGB_PURPLE, false, true,  true,
                  wch_mode_init, wch_mode_task, NULL, NULL, NULL },
  [MODE_UPDI] = { "updi", "tinyLINK UPDI (SerialUPDI)",  RGB_YELLOW, false, true,  true,
                  updi_mode_init, updi_mode_task, updi_mode_line_coding, NULL, updi_mode_break },
  [MODE_UART] = { "uart", "tinyLINK UART",               RGB_WHITE,  false, false, true,
                  uart_mode_init, NULL, NULL, NULL, NULL },
  [MODE_LA]   = { "la",   "tinyLINK logic analyser",     RGB_BLUE,   false, true,  false,
                  la_mode_init, la_mode_task, NULL, NULL, NULL },
  [MODE_SPI]  = { "spi",  "tinyLINK SPI flash (serprog)", RGB_ORANGE, false, true, false,
                  serprog_mode_init, serprog_mode_task, NULL, NULL, NULL },
  [MODE_JTAG] = { "jtag", "tinyLINK CMSIS-DAP JTAG",     RGB_CYAN,   true,  false, true,
                  dap_mode_init_jtag, dap_mode_task, NULL, NULL, NULL },
  [MODE_ISP]  = { "isp",  "tinyLINK AVR ISP (STK500v2)", RGB_RED,    false, true,  false,
                  avrisp_mode_init, avrisp_mode_task, NULL, NULL, NULL },
};

mode_id_t mode_current = MODE_SWD;

void mode_select(mode_id_t id) {
  if (id >= MODE_COUNT)
    return;
  power_set(false);
  settings_save_mode((int)id);
  // Drop off the bus first so the host sees a clean disconnect.
  tud_disconnect();
  sleep_ms(50);
  watchdog_reboot(0, 0, 10);
  for (;;)
    tight_loop_contents();
}

// --- TinyUSB CDC callbacks, routed by port ---------------------------------

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *coding) {
  if ((int)itf == cdc_uart)
    cdc_uart_line_coding(coding);
  else if ((int)itf == cdc_proto && mode()->line_coding)
    mode()->line_coding(coding);
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  if ((int)itf == cdc_console)
    console_line_state(dtr, rts);
  else if ((int)itf == cdc_uart && mode_current == MODE_UART)
    uart_mode_line_state(dtr, rts);
  else if ((int)itf == cdc_proto && mode()->line_state)
    mode()->line_state(dtr, rts);
}

void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms) {
  if ((int)itf == cdc_uart)
    cdc_uart_break(duration_ms);
  else if ((int)itf == cdc_proto && mode()->send_break)
    mode()->send_break(duration_ms);
}

// ---------------------------------------------------------------------------

int main(void) {
  int saved = settings_load_mode();
  if (saved >= 0 && saved < MODE_COUNT)
    mode_current = (mode_id_t)saved;

  // All IOs start as inputs; SHIFT_OE stays low until VTGT is present.
  for (uint i = 0; i < IO_COUNT; i++)
    gpio_init(PIN_IO(i));

  power_init();
  led_init();
  led_set(mode()->rgb);

  usb_descriptors_init();
  tud_init(0);

  cdc_uart_init();
  mode()->init();

  for (;;) {
    usbio_service();
    if (mode()->task)
      mode()->task();
  }
}
