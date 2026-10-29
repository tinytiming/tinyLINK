// SPI flash mode: flashrom serprog over the protocol port.
//   flashrom -p serprog:dev=/dev/ttyACM0,spispeed=8M
// SPI0: IO0 SCK, IO1 MOSI, IO2 MISO, IO3 CS. The pins stay high-Z until the
// first SPI operation and after flashrom releases them (S_PIN_STATE 0).
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "board.h"
#include "modes.h"
#include "usbio.h"

#define S_ACK 0x06
#define S_NAK 0x15

enum {
  S_CMD_NOP         = 0x00,
  S_CMD_Q_IFACE     = 0x01,
  S_CMD_Q_CMDMAP    = 0x02,
  S_CMD_Q_PGMNAME   = 0x03,
  S_CMD_Q_SERBUF    = 0x04,
  S_CMD_Q_BUSTYPE   = 0x05,
  S_CMD_SYNCNOP     = 0x10,
  S_CMD_S_BUSTYPE   = 0x12,
  S_CMD_O_SPIOP     = 0x13,
  S_CMD_S_SPI_FREQ  = 0x14,
  S_CMD_S_PIN_STATE = 0x15,
  S_CMD_S_SPI_CS    = 0x16,
};

#define BUS_SPI     (1 << 3)
#define SERBUF_SIZE 4096
#define TIMEOUT_MS  2000

static const uint8_t supported[] = {
  S_CMD_NOP, S_CMD_Q_IFACE, S_CMD_Q_CMDMAP, S_CMD_Q_PGMNAME, S_CMD_Q_SERBUF,
  S_CMD_Q_BUSTYPE, S_CMD_SYNCNOP, S_CMD_S_BUSTYPE, S_CMD_O_SPIOP,
  S_CMD_S_SPI_FREQ, S_CMD_S_PIN_STATE, S_CMD_S_SPI_CS,
};

static bool pins_on;
static uint32_t spi_hz = 8000000;

static void pins_enable(bool on) {
  if (on == pins_on)
    return;
  pins_on = on;
  if (on) {
    gpio_put(PIN_SPI_CS, 1);
    gpio_set_dir(PIN_SPI_CS, GPIO_OUT);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
  } else {
    gpio_set_dir(PIN_SPI_CS, GPIO_IN);
    gpio_init(PIN_SPI_SCK);
    gpio_init(PIN_SPI_MOSI);
    gpio_init(PIN_SPI_MISO);
  }
}

void serprog_mode_init(void) {
  spi_init(spi0, spi_hz);
  spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_init(PIN_SPI_CS);
  gpio_pull_up(PIN_SPI_MISO);
}

static void reply_u16(uint16_t v) {
  uint8_t b[3] = { S_ACK, (uint8_t)v, (uint8_t)(v >> 8) };
  usbio_write(cdc_proto, b, sizeof(b));
}

static bool read_u24(uint32_t *v) {
  uint8_t b[3];
  if (!usbio_read(cdc_proto, b, 3, TIMEOUT_MS))
    return false;
  *v = b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16;
  return true;
}

static void spi_op(void) {
  uint32_t slen, rlen;
  if (!read_u24(&slen) || !read_u24(&rlen))
    return;
  pins_enable(true);

  static uint8_t buf[256];
  gpio_put(PIN_SPI_CS, 0);
  while (slen) {
    uint32_t n = slen < sizeof(buf) ? slen : sizeof(buf);
    if (!usbio_read(cdc_proto, buf, n, TIMEOUT_MS)) {
      gpio_put(PIN_SPI_CS, 1);
      return;
    }
    spi_write_blocking(spi0, buf, n);
    slen -= n;
  }
  usbio_putc(cdc_proto, S_ACK);
  while (rlen) {
    uint32_t n = rlen < sizeof(buf) ? rlen : sizeof(buf);
    spi_read_blocking(spi0, 0xFF, buf, n);
    usbio_write(cdc_proto, buf, n);
    rlen -= n;
  }
  gpio_put(PIN_SPI_CS, 1);
  usbio_flush(cdc_proto);
}

void serprog_mode_task(void) {
  int cmd = usbio_getc(cdc_proto, 0);
  if (cmd < 0)
    return;

  switch (cmd) {
  case S_CMD_NOP:
    usbio_putc(cdc_proto, S_ACK);
    break;
  case S_CMD_Q_IFACE:
    reply_u16(1);
    break;
  case S_CMD_Q_CMDMAP: {
    uint8_t b[33] = { S_ACK };
    for (size_t i = 0; i < sizeof(supported); i++)
      b[1 + supported[i] / 8] |= (uint8_t)(1u << (supported[i] % 8));
    usbio_write(cdc_proto, b, sizeof(b));
    break;
  }
  case S_CMD_Q_PGMNAME: {
    uint8_t b[17] = { S_ACK };
    memcpy(b + 1, "tinyLINK", 8);
    usbio_write(cdc_proto, b, sizeof(b));
    break;
  }
  case S_CMD_Q_SERBUF:
    reply_u16(SERBUF_SIZE);
    break;
  case S_CMD_Q_BUSTYPE: {
    uint8_t b[2] = { S_ACK, BUS_SPI };
    usbio_write(cdc_proto, b, sizeof(b));
    break;
  }
  case S_CMD_SYNCNOP: {
    uint8_t b[2] = { S_NAK, S_ACK };
    usbio_write(cdc_proto, b, sizeof(b));
    break;
  }
  case S_CMD_S_BUSTYPE: {
    int t = usbio_getc(cdc_proto, TIMEOUT_MS);
    usbio_putc(cdc_proto, t >= 0 && (t & BUS_SPI) ? S_ACK : S_NAK);
    break;
  }
  case S_CMD_O_SPIOP:
    spi_op();
    break;
  case S_CMD_S_SPI_FREQ: {
    uint8_t b[4];
    if (!usbio_read(cdc_proto, b, 4, TIMEOUT_MS))
      break;
    uint32_t hz = b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    if (!hz) {
      usbio_putc(cdc_proto, S_NAK);
      break;
    }
    if (hz > 30000000)
      hz = 30000000;   // TXS0108E + 33R + leads
    spi_hz = spi_set_baudrate(spi0, hz);
    uint8_t r[5] = { S_ACK, (uint8_t)spi_hz, (uint8_t)(spi_hz >> 8), (uint8_t)(spi_hz >> 16), (uint8_t)(spi_hz >> 24) };
    usbio_write(cdc_proto, r, sizeof(r));
    break;
  }
  case S_CMD_S_PIN_STATE: {
    int on = usbio_getc(cdc_proto, TIMEOUT_MS);
    if (on < 0)
      break;
    pins_enable(on != 0);
    usbio_putc(cdc_proto, S_ACK);
    break;
  }
  case S_CMD_S_SPI_CS: {
    int cs = usbio_getc(cdc_proto, TIMEOUT_MS);
    usbio_putc(cdc_proto, cs == 0 ? S_ACK : S_NAK);
    break;
  }
  default:
    usbio_putc(cdc_proto, S_NAK);
    break;
  }
  usbio_flush(cdc_proto);
}
