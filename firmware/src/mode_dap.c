// SWD and JTAG modes: CMSIS-DAP v2 (bulk) on free-dap, plus the target UART.
// Both modes speak SWD and JTAG; they differ in the default port and LED.
#include <string.h>
#include "pico/unique_id.h"
#include "tusb.h"
#include "dap.h"
#include "dap_config.h"
#include "modes.h"
#include "usb_descriptors.h"

// free-dap port ids (dap.c)
#define DAP_PORT_SWD  1
#define DAP_PORT_JTAG 2

int dap_default_port = DAP_PORT_SWD;
char dap_serial_number[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

static uint8_t req[DAP_CONFIG_PACKET_SIZE];
static uint8_t resp[DAP_CONFIG_PACKET_SIZE];

static void dap_mode_init(int port) {
  dap_default_port = port;
  pico_get_unique_board_id_string(dap_serial_number, sizeof(dap_serial_number));
  dap_init();
}

void dap_mode_init_swd(void) { dap_mode_init(DAP_PORT_SWD); }
void dap_mode_init_jtag(void) { dap_mode_init(DAP_PORT_JTAG); }

void dap_mode_task(void) {
  if (itf_dap < 0 || !tud_vendor_n_available(0))
    return;
  uint32_t n = tud_vendor_n_read(0, req, sizeof(req));
  if (!n)
    return;
  int len = dap_process_request(req, (int)n, resp, sizeof(resp));
  if (len > 0) {
    tud_vendor_n_write(0, resp, (uint32_t)len);
    tud_vendor_n_write_flush(0);
  }
}
