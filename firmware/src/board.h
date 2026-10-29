// tinyLINK revA pin map (hardware/, RP2040-Zero carrier).
//
// IO0-IO7 are GP2-GP9. Each goes through the optional TXS0108E (U2) and a
// 33R to J1/J2/J3/J4. U2 is enabled by SHIFT_OE only while VTGT >= 3.1 V.
#pragma once

#define PIN_IO_BASE     2u
#define PIN_IO(n)       (PIN_IO_BASE + (n))
#define IO_COUNT        8u

#define PIN_SHIFT_OE    10u   // U2 OE, R14 pull-down
#define PIN_TPWR_EN     14u   // Q3 gate: target power on
#define PIN_WS2812      16u   // on the RP2040-Zero module
#define PIN_VTGT_SENSE  26u   // VTGT/2 (R12/R13 100k/100k)
#define ADC_VTGT        0u

// Per-mode channel names, see hardware/README.md "Channels".
#define PIN_SWCLK       PIN_IO(0)
#define PIN_SWDIO       PIN_IO(1)
#define PIN_UART_TX     PIN_IO(2)   // UART1 TX
#define PIN_UART_RX     PIN_IO(3)   // UART1 RX
#define PIN_NRESET      PIN_IO(4)
#define PIN_TDO         PIN_IO(5)
#define PIN_TDI         PIN_IO(6)

#define PIN_ESP_EN      PIN_IO(4)   // UART mode, host RTS
#define PIN_ESP_BOOT    PIN_IO(5)   // UART mode, host DTR

#define PIN_UPDI        PIN_IO(1)
#define PIN_SWIO        PIN_IO(1)

#define PIN_SPI_SCK     PIN_IO(0)   // SPI0 SCK
#define PIN_SPI_MOSI    PIN_IO(1)   // SPI0 TX
#define PIN_SPI_MISO    PIN_IO(2)   // SPI0 RX
#define PIN_SPI_CS      PIN_IO(3)
#define PIN_ISP_RESET   PIN_IO(4)

#define TARGET_UART     uart1

// Shifter thresholds on the reconstructed VTGT (mV).
#define VTGT_SHIFT_ON_MV   3100
#define VTGT_SHIFT_OFF_MV  3000
