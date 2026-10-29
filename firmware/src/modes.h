// Mode table. One mode runs per boot; changing mode saves it and reboots so
// USB re-enumerates with that mode's interfaces.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tusb.h"

typedef enum {
  MODE_SWD = 0,
  MODE_WCH,
  MODE_UPDI,
  MODE_UART,
  MODE_LA,
  MODE_SPI,
  MODE_JTAG,
  MODE_ISP,
  MODE_COUNT
} mode_id_t;

typedef struct {
  const char *name;     // short name for the console and USB strings
  const char *title;    // USB product / interface string
  uint32_t rgb;         // LED colour, 0xRRGGBB
  bool dap;             // CMSIS-DAP v2 vendor interface
  bool proto_cdc;       // protocol port (minichlink, pymcuprog, flashrom, ...)
  bool uart_cdc;        // target UART on IO2/IO3

  void (*init)(void);
  void (*task)(void);   // called from the main loop, may block via usbio_*
  // CDC hooks for the protocol port (may be NULL).
  void (*line_coding)(cdc_line_coding_t const *coding);
  void (*line_state)(bool dtr, bool rts);
  void (*send_break)(uint16_t ms);
} probe_mode_t;

extern const probe_mode_t modes[MODE_COUNT];
extern mode_id_t mode_current;

static inline const probe_mode_t *mode(void) { return &modes[mode_current]; }

// CDC interface numbers for this boot (-1 when absent).
extern int cdc_proto;
extern int cdc_uart;
extern int cdc_console;

void mode_select(mode_id_t id);   // save + reboot

// Mode implementations
void dap_mode_init_swd(void);
void dap_mode_init_jtag(void);
void dap_mode_task(void);

void uart_mode_init(void);
void uart_mode_line_state(bool dtr, bool rts);

void updi_mode_init(void);
void updi_mode_task(void);
void updi_mode_line_coding(cdc_line_coding_t const *coding);
void updi_mode_break(uint16_t ms);

void wch_mode_init(void);
void wch_mode_task(void);

void la_mode_init(void);
void la_mode_task(void);

void serprog_mode_init(void);
void serprog_mode_task(void);

void avrisp_mode_init(void);
void avrisp_mode_task(void);
