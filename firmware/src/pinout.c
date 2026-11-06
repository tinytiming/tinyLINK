// `pinout` / `help`: the pins the current mode uses and where they land on
// the connectors. Built from the same PIN_* definitions the modes use, so a
// swap shows up here as it does on the wire.
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "console.h"
#include "modes.h"
#include "pinout.h"

typedef struct {
  const char *signal;
  unsigned gpio;
} pin_use_t;

#if TINYLINK_BOARD_PICO
#define BOARD_NAME   "Raspberry Pi Pico / picoprobe PCB"
#define CONN_HEADER  "Pico   7-pin  Cortex10"
// Pico physical pin, picoprobe PCB 1x7 header (J1) and Cortex 10-pin (J2), per IO.
static const char *const io_conn[IO_COUNT] = {
  "4      2      4",
  "5      4      2",
  "6      5      10",
  "7      6      6",
  "9      flying lead",
  "10     flying lead",
  "11     flying lead",
  "12     flying lead",
};
#define VTGT_ROW     "  VTREF    -    GP28  34     1      1\r\n"
#define GND_ROW      "  GND      -    -     3,8,13 3      3,5\r\n"
#define FOOTER       "Pico GND: pins 3, 8, 13, 18, 23, 28, 33, 38\r\n"
#define EXTRA_GPIO   PIN_UART_TX
#define EXTRA_CONN   "26     flying lead"
#else
#define BOARD_NAME   "tinyLINK rev A, RP2040-Zero"
#define CONN_HEADER  "2x10   JST     Cortex10"
// 2x10 header (J3), SWD/UART JST-SH (J1/J2) and Cortex 10-pin on the back (J4), per IO.
static const char *const io_conn[IO_COUNT] = {
  "5      SWD 1   4",
  "7      SWD 3   2",
  "9      UART 1  -",
  "11     UART 3  -",
  "13     -       10",
  "15     -       6",
  "17     -       8",
  "19     -       -",
};
#define VTGT_ROW     "  VTGT     -    -     1,2    -       1\r\n"
#define GND_ROW      "  GND      -    -     3,even SWD 2   3,5,9\r\n" \
                     "                             UART 2\r\n"
#define FOOTER       "2x10: square pads 1, 2 = VTGT; edge row (even pins) GND except pin 2;\r\n" \
                     "      inner row (odd pins) 3 = GND, 5-19 = IO0-IO7\r\n"
#endif

static const char *conn(unsigned gpio) {
#ifdef EXTRA_CONN
  if (gpio == EXTRA_GPIO)
    return EXTRA_CONN;
#endif
  return io_conn[gpio - PIN_IO_BASE];
}

static unsigned mode_pins(pin_use_t *p) {
  static const char *const la_names[IO_COUNT] = { "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9" };
  unsigned n = 0;
  switch (mode_current) {
  case MODE_SWD:
    p[n++] = (pin_use_t){ "SWCLK", PIN_SWCLK };
    p[n++] = (pin_use_t){ "SWDIO", PIN_SWDIO };
    p[n++] = (pin_use_t){ "nRESET", PIN_NRESET };
    break;
  case MODE_JTAG:
    p[n++] = (pin_use_t){ "TCK", PIN_SWCLK };
    p[n++] = (pin_use_t){ "TMS", PIN_SWDIO };
    p[n++] = (pin_use_t){ "TDO", PIN_TDO };
    p[n++] = (pin_use_t){ "TDI", PIN_TDI };
    p[n++] = (pin_use_t){ "nSRST", PIN_NRESET };
    break;
  case MODE_WCH:
    p[n++] = (pin_use_t){ "SWIO", PIN_SWIO };
    break;
  case MODE_UPDI:
    p[n++] = (pin_use_t){ "UPDI", PIN_UPDI };
    break;
  case MODE_UART:
    p[n++] = (pin_use_t){ "EN", PIN_ESP_EN };
    p[n++] = (pin_use_t){ "BOOT", PIN_ESP_BOOT };
    break;
  case MODE_LA:
    for (unsigned i = 0; i < IO_COUNT; i++)
      p[n++] = (pin_use_t){ la_names[i], PIN_IO(i) };
    break;
  case MODE_SPI:
    p[n++] = (pin_use_t){ "SCK", PIN_SPI_SCK };
    p[n++] = (pin_use_t){ "MOSI", PIN_SPI_MOSI };
    p[n++] = (pin_use_t){ "MISO", PIN_SPI_MISO };
    p[n++] = (pin_use_t){ "CS", PIN_SPI_CS };
    break;
  case MODE_ISP:
    p[n++] = (pin_use_t){ "SCK", PIN_SPI_SCK };
    p[n++] = (pin_use_t){ "MOSI", PIN_SPI_MOSI };
    p[n++] = (pin_use_t){ "MISO", PIN_SPI_MISO };
    p[n++] = (pin_use_t){ "RESET", PIN_ISP_RESET };
    break;
  default:
    break;
  }
  if (mode()->uart_cdc) {
    p[n++] = (pin_use_t){ "TX (out)", PIN_UART_TX };
    p[n++] = (pin_use_t){ "RX (in)", PIN_UART_RX };
  }
  return n;
}

void pinout_print(void) {
  pin_use_t pins[IO_COUNT + 2];
  unsigned n = mode_pins(pins);
  bool swapped = (mode_current == MODE_SWD || mode_current == MODE_JTAG || mode_current == MODE_WCH) &&
                 pin_swclk != PIN_IO(0);
  console_printf("mode %d %s pinout (%s)%s:\r\n", mode_current + 1, mode()->name, BOARD_NAME,
                 swapped ? ", IO0/IO1 swapped" : "");
  console_printf("  signal   IO   GPIO  " CONN_HEADER "\r\n");
  for (unsigned i = 0; i < n; i++) {
    unsigned io = pins[i].gpio - PIN_IO_BASE;
    char io_name[4] = "-";
    if (io < IO_COUNT)
      snprintf(io_name, sizeof(io_name), "IO%u", io);
    console_printf("  %s%-*s %-4s GP%-2u  %s\r\n", pins[i].signal, 8 - (int)strlen(pins[i].signal),
                   swapped && io < 2 ? "*" : "", io_name, pins[i].gpio, conn(pins[i].gpio));
  }
  console_printf(VTGT_ROW);
  console_printf(GND_ROW);
#ifdef FOOTER
  console_printf(FOOTER);
#endif
  if (swapped)
    console_printf("* swapped (swap on)\r\n");
}
