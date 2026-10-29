// free-dap configuration for tinyLINK (bit-banged SWD/JTAG on SIO).
// SWCLK/TCK = IO0, SWDIO/TMS = IO1, TDO = IO5, TDI = IO6, nRESET = IO4.
#ifndef _DAP_CONFIG_H_
#define _DAP_CONFIG_H_

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "board.h"
#include "version.h"

#define DAP_CONFIG_ENABLE_JTAG

extern int dap_default_port;
#define DAP_CONFIG_DEFAULT_PORT        dap_default_port
#define DAP_CONFIG_DEFAULT_CLOCK       1000000 // Hz

#define DAP_CONFIG_PACKET_SIZE         64
#define DAP_CONFIG_PACKET_COUNT        1

#define DAP_CONFIG_JTAG_DEV_COUNT      8

// DAP_CONFIG_PRODUCT_STR must contain "CMSIS-DAP" to be compatible with the standard
#define DAP_CONFIG_VENDOR_STR          "tinyLINK"
#define DAP_CONFIG_PRODUCT_STR         "tinyLINK CMSIS-DAP"
#define DAP_CONFIG_SER_NUM_STR         dap_serial_number
#define DAP_CONFIG_CMSIS_DAP_VER_STR   "2.0.0"
#define DAP_CONFIG_FW_VER_STR          TINYLINK_VERSION

#define DAP_CONFIG_PERFORMANCE_ATTR    __attribute__((section(".time_critical.dap")))

// Scaled from free-dap's rp2040 port (120 MHz) to the SDK's 125 MHz; not
// yet measured on tinyLINK.
#define DAP_CONFIG_DELAY_CONSTANT      20833
#define DAP_CONFIG_FAST_CLOCK          8900000 // Hz

extern char dap_serial_number[];

#define DAP_PIN(p) (1u << (p))

static inline void dap_pin_write(uint pin, int value) {
  if (value)
    sio_hw->gpio_set = DAP_PIN(pin);
  else
    sio_hw->gpio_clr = DAP_PIN(pin);
}

static inline int dap_pin_read(uint pin) {
  return (sio_hw->gpio_in >> pin) & 1;
}

static inline void dap_pin_out(uint pin) { sio_hw->gpio_oe_set = DAP_PIN(pin); }
static inline void dap_pin_in(uint pin)  { sio_hw->gpio_oe_clr = DAP_PIN(pin); }

static inline void DAP_CONFIG_SWCLK_TCK_write(int value) { dap_pin_write(PIN_SWCLK, value); }
static inline void DAP_CONFIG_SWDIO_TMS_write(int value) { dap_pin_write(PIN_SWDIO, value); }
static inline void DAP_CONFIG_TDI_write(int value)       { dap_pin_write(PIN_TDI, value); }
static inline void DAP_CONFIG_TDO_write(int value)       { dap_pin_write(PIN_TDO, value); }
static inline void DAP_CONFIG_nTRST_write(int value)     { (void)value; }

// nRESET is open-drain: drive low, or release to the target's pull-up.
static inline void DAP_CONFIG_nRESET_write(int value) {
  if (value)
    dap_pin_in(PIN_NRESET);
  else
    dap_pin_out(PIN_NRESET);
}

static inline int DAP_CONFIG_SWCLK_TCK_read(void) { return dap_pin_read(PIN_SWCLK); }
static inline int DAP_CONFIG_SWDIO_TMS_read(void) { return dap_pin_read(PIN_SWDIO); }
static inline int DAP_CONFIG_TDO_read(void)       { return dap_pin_read(PIN_TDO); }
static inline int DAP_CONFIG_TDI_read(void)       { return dap_pin_read(PIN_TDI); }
static inline int DAP_CONFIG_nTRST_read(void)     { return 0; }
static inline int DAP_CONFIG_nRESET_read(void)    { return dap_pin_read(PIN_NRESET); }

static inline void DAP_CONFIG_SWCLK_TCK_set(void) { sio_hw->gpio_set = DAP_PIN(PIN_SWCLK); }
static inline void DAP_CONFIG_SWCLK_TCK_clr(void) { sio_hw->gpio_clr = DAP_PIN(PIN_SWCLK); }

static inline void DAP_CONFIG_SWDIO_TMS_in(void)  { dap_pin_in(PIN_SWDIO); }
static inline void DAP_CONFIG_SWDIO_TMS_out(void) { dap_pin_out(PIN_SWDIO); }

static inline void DAP_CONFIG_SETUP(void) {
  const uint pins[] = { PIN_SWCLK, PIN_SWDIO, PIN_NRESET, PIN_TDO, PIN_TDI };
  for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    gpio_init(pins[i]);           // SIO, input, output latch low
    gpio_set_input_hysteresis_enabled(pins[i], true);
  }
}

static inline void DAP_CONFIG_DISCONNECT(void) {
  dap_pin_in(PIN_SWCLK);
  dap_pin_in(PIN_SWDIO);
  dap_pin_in(PIN_NRESET);
  dap_pin_in(PIN_TDO);
  dap_pin_in(PIN_TDI);
}

static inline void DAP_CONFIG_CONNECT_SWD(void) {
  dap_pin_write(PIN_SWDIO, 1);
  dap_pin_out(PIN_SWDIO);
  dap_pin_write(PIN_SWCLK, 1);
  dap_pin_out(PIN_SWCLK);
  dap_pin_write(PIN_NRESET, 0);   // latch low; released = input
  dap_pin_in(PIN_NRESET);
  dap_pin_in(PIN_TDO);
  dap_pin_in(PIN_TDI);
}

static inline void DAP_CONFIG_CONNECT_JTAG(void) {
  dap_pin_write(PIN_SWDIO, 1);
  dap_pin_out(PIN_SWDIO);
  dap_pin_write(PIN_SWCLK, 1);
  dap_pin_out(PIN_SWCLK);
  dap_pin_write(PIN_NRESET, 0);
  dap_pin_in(PIN_NRESET);
  dap_pin_in(PIN_TDO);
  dap_pin_write(PIN_TDI, 1);
  dap_pin_out(PIN_TDI);
}

static inline void DAP_CONFIG_LED(int index, int state) {
  (void)index;
  (void)state;
}

__attribute__((always_inline))
static inline void DAP_CONFIG_DELAY(uint32_t cycles) {
  asm volatile (
    "1: sub %[cycles], %[cycles], #1 \n"
    "   bne 1b \n"
    : [cycles] "+l"(cycles)
  );
}

#endif // _DAP_CONFIG_H_
