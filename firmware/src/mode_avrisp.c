// AVR ISP mode: STK500v2 (Atmel AVR068) over the protocol port.
//   avrdude -c stk500v2 -P /dev/ttyACM0 -p m328p ...
// IO0 SCK, IO1 MOSI, IO2 MISO (SPI0), IO4 RESET. Pins stay high-Z outside
// programming mode so the target runs normally.
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "board.h"
#include "modes.h"
#include "power.h"
#include "usbio.h"

#define MESSAGE_START 0x1B
#define TOKEN         0x0E

enum {
  CMD_SIGN_ON              = 0x01,
  CMD_SET_PARAMETER        = 0x02,
  CMD_GET_PARAMETER        = 0x03,
  CMD_OSCCAL               = 0x05,
  CMD_LOAD_ADDRESS         = 0x06,
  CMD_ENTER_PROGMODE_ISP   = 0x10,
  CMD_LEAVE_PROGMODE_ISP   = 0x11,
  CMD_CHIP_ERASE_ISP       = 0x12,
  CMD_PROGRAM_FLASH_ISP    = 0x13,
  CMD_READ_FLASH_ISP       = 0x14,
  CMD_PROGRAM_EEPROM_ISP   = 0x15,
  CMD_READ_EEPROM_ISP      = 0x16,
  CMD_PROGRAM_FUSE_ISP     = 0x17,
  CMD_READ_FUSE_ISP        = 0x18,
  CMD_PROGRAM_LOCK_ISP     = 0x19,
  CMD_READ_LOCK_ISP        = 0x1A,
  CMD_READ_SIGNATURE_ISP   = 0x1B,
  CMD_READ_OSCCAL_ISP      = 0x1C,
  CMD_SPI_MULTI            = 0x1D,
};

enum {
  STATUS_CMD_OK      = 0x00,
  STATUS_CMD_TOUT    = 0x80,
  STATUS_RDY_BSY_TOUT = 0x81,
  STATUS_CMD_FAILED  = 0xC0,
  STATUS_CMD_UNKNOWN = 0xC9,
};

enum {
  PARAM_HW_VER         = 0x90,
  PARAM_SW_MAJOR       = 0x91,
  PARAM_SW_MINOR       = 0x92,
  PARAM_VTARGET        = 0x94,
  PARAM_SCK_DURATION   = 0x98,
  PARAM_RESET_POLARITY = 0x9E,
};

#define BUF_SIZE   (8 + 300)
#define TIMEOUT_MS 1000

static uint8_t param[256];
static uint8_t msg[BUF_SIZE];
static uint8_t ans[BUF_SIZE];
static uint32_t address;
static bool prog_mode;

// STK500 SCK_DURATION: 0 = 921.6 kHz, 1 = 230.4 kHz, 2 = 57.6 kHz,
// 3 = 28.8 kHz, slower above that (avrdude -B).
static uint32_t sck_hz(uint8_t d) {
  static const uint32_t table[] = { 921600, 230400, 57600, 28800 };
  return d < 4 ? table[d] : 86400 / d;
}

static void reset_active(bool active) {
  bool active_low = param[PARAM_RESET_POLARITY] != 0;
  if (active_low) {
    gpio_put(PIN_ISP_RESET, 0);
    gpio_set_dir(PIN_ISP_RESET, active ? GPIO_OUT : GPIO_IN);   // released = pulled up
  } else {
    gpio_put(PIN_ISP_RESET, active);
    gpio_set_dir(PIN_ISP_RESET, GPIO_OUT);
  }
}

static void spi_pins(bool on) {
  if (on) {
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
  } else {
    gpio_init(PIN_SPI_SCK);
    gpio_init(PIN_SPI_MOSI);
    gpio_init(PIN_SPI_MISO);
  }
}

static void sck_pulse(void) {
  gpio_init(PIN_SPI_SCK);
  gpio_set_dir(PIN_SPI_SCK, GPIO_OUT);
  gpio_put(PIN_SPI_SCK, 1);
  sleep_us(50);
  gpio_put(PIN_SPI_SCK, 0);
  sleep_us(50);
  gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
}

static void isp_cmd(const uint8_t cmd[4], uint8_t res[4]) {
  spi_write_read_blocking(spi0, cmd, res, 4);
}

static uint8_t isp(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint8_t cmd[4] = { a, b, c, d }, res[4];
  isp_cmd(cmd, res);
  return res[3];
}

static bool wait_ready(uint32_t timeout_ms) {
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  while (isp(0xF0, 0, 0, 0) & 1) {
    if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
      return false;
  }
  return true;
}

static void wait_write(uint8_t mode, uint8_t delay_ms, bool page) {
  uint8_t rdy = page ? 0x40 : 0x08;
  if (mode & rdy) {
    wait_ready(50);
    return;
  }
  // timed delay, and the fallback for value polling
  sleep_ms(delay_ms ? delay_ms : 10);
}

static void load_ext_address(void) {
  if (address & 0x80000000u)
    isp(0x4D, 0x00, (uint8_t)(address >> 16), 0x00);
}

// Reply helpers ------------------------------------------------------------

static void send_answer(uint8_t seq, size_t len) {
  uint8_t hdr[5] = { MESSAGE_START, seq, (uint8_t)(len >> 8), (uint8_t)len, TOKEN };
  uint8_t sum = 0;
  for (int i = 0; i < 5; i++)
    sum ^= hdr[i];
  for (size_t i = 0; i < len; i++)
    sum ^= ans[i];
  usbio_write(cdc_proto, hdr, sizeof(hdr));
  usbio_write(cdc_proto, ans, len);
  usbio_putc(cdc_proto, sum);
  usbio_flush(cdc_proto);
}

// Commands ----------------------------------------------------------------

static size_t cmd_enter_progmode(const uint8_t *b) {
  uint8_t stab_delay = b[2], cmdexe_delay = b[3], synch_loops = b[4], byte_delay = b[5];
  uint8_t poll_value = b[6], poll_index = b[7];
  const uint8_t *cmd = &b[8];

  spi_set_baudrate(spi0, sck_hz(param[PARAM_SCK_DURATION]));
  gpio_put(PIN_SPI_SCK, 0);
  spi_pins(true);
  // SCK low, then a positive RESET pulse in case SCK wasn't low at power-up
  reset_active(false);
  sleep_us(100);
  reset_active(true);
  sleep_ms(stab_delay > 20 ? stab_delay : 20);
  sleep_ms(cmdexe_delay);

  ans[1] = STATUS_CMD_FAILED;
  for (uint i = 0; i < (synch_loops ? synch_loops : 32); i++) {
    uint8_t res[4];
    for (int k = 0; k < 4; k++) {
      spi_write_read_blocking(spi0, &cmd[k], &res[k], 1);
      if (byte_delay)
        sleep_ms(byte_delay);
    }
    if (poll_index == 0 || (poll_index <= 4 && res[poll_index - 1] == poll_value)) {
      ans[1] = STATUS_CMD_OK;
      break;
    }
    sck_pulse();
    sleep_ms(1);
  }
  prog_mode = ans[1] == STATUS_CMD_OK;
  if (!prog_mode) {
    reset_active(false);
    spi_pins(false);
  }
  return 2;
}

static size_t cmd_leave_progmode(const uint8_t *b) {
  sleep_ms(b[1]);
  spi_pins(false);
  reset_active(false);
  sleep_ms(b[2]);
  prog_mode = false;
  ans[1] = STATUS_CMD_OK;
  return 2;
}

static size_t cmd_chip_erase(const uint8_t *b) {
  uint8_t erase_delay = b[1], poll_method = b[2];
  isp(b[3], b[4], b[5], b[6]);
  if (poll_method)
    ans[1] = wait_ready(erase_delay ? erase_delay * 2u : 100) ? STATUS_CMD_OK : STATUS_RDY_BSY_TOUT;
  else {
    sleep_ms(erase_delay);
    ans[1] = STATUS_CMD_OK;
  }
  return 2;
}

static size_t cmd_program(const uint8_t *b, bool flash) {
  uint16_t n = (uint16_t)(b[1] << 8 | b[2]);
  uint8_t mode = b[3], delay_ms = b[4], cmd1 = b[5], cmd2 = b[6];
  const uint8_t *data = &b[10];
  bool page = mode & 0x01;

  load_ext_address();
  uint32_t start = address & 0xFFFF;
  for (uint16_t i = 0; i < n; i++) {
    uint32_t a = flash ? start + i / 2 : start + i;
    uint8_t c = flash ? (uint8_t)(cmd1 | ((i & 1) ? 0x08 : 0x00)) : cmd1;
    isp(c, (uint8_t)(a >> 8), (uint8_t)a, data[i]);
    if (!page)
      wait_write(mode, delay_ms, false);
  }
  if (page && (mode & 0x80)) {
    isp(cmd2, (uint8_t)(start >> 8), (uint8_t)start, 0);
    wait_write(mode, delay_ms, true);
  }
  address += flash ? n / 2 : n;
  ans[1] = STATUS_CMD_OK;
  return 2;
}

static size_t cmd_read(const uint8_t *b, bool flash) {
  uint16_t n = (uint16_t)(b[1] << 8 | b[2]);
  uint8_t cmd1 = b[3];
  if (n > BUF_SIZE - 3)
    n = BUF_SIZE - 3;

  load_ext_address();
  uint32_t start = address & 0xFFFF;
  ans[1] = STATUS_CMD_OK;
  for (uint16_t i = 0; i < n; i++) {
    uint32_t a = flash ? start + i / 2 : start + i;
    uint8_t c = flash ? (uint8_t)(cmd1 | ((i & 1) ? 0x08 : 0x00)) : cmd1;
    ans[2 + i] = isp(c, (uint8_t)(a >> 8), (uint8_t)a, 0);
  }
  ans[2 + n] = STATUS_CMD_OK;
  address += flash ? n / 2 : n;
  return 3u + n;
}

static size_t cmd_read_byte(const uint8_t *b) {
  uint8_t ret_addr = b[1], res[4];
  isp_cmd(&b[2], res);
  ans[1] = STATUS_CMD_OK;
  ans[2] = res[(ret_addr >= 1 && ret_addr <= 4 ? ret_addr : 4) - 1];
  ans[3] = STATUS_CMD_OK;
  return 4;
}

static size_t cmd_spi_multi(const uint8_t *b) {
  uint8_t num_tx = b[1], num_rx = b[2], rx_start = b[3];
  const uint8_t *tx = &b[4];
  uint total = num_tx > rx_start + num_rx ? num_tx : rx_start + num_rx;
  ans[1] = STATUS_CMD_OK;
  for (uint i = 0; i < total; i++) {
    uint8_t out = i < num_tx ? tx[i] : 0, in;
    spi_write_read_blocking(spi0, &out, &in, 1);
    if (i >= rx_start && i < (uint)rx_start + num_rx)
      ans[2 + i - rx_start] = in;
  }
  ans[2 + num_rx] = STATUS_CMD_OK;
  return 3u + num_rx;
}

static size_t handle(const uint8_t *b, size_t len) {
  (void)len;
  ans[0] = b[0];
  switch (b[0]) {
  case CMD_SIGN_ON:
    ans[1] = STATUS_CMD_OK;
    ans[2] = 8;
    memcpy(&ans[3], "STK500_2", 8);
    return 11;
  case CMD_SET_PARAMETER:
    param[b[1]] = b[2];
    if (b[1] == PARAM_SCK_DURATION)
      spi_set_baudrate(spi0, sck_hz(b[2]));
    ans[1] = STATUS_CMD_OK;
    return 2;
  case CMD_GET_PARAMETER:
    if (b[1] == PARAM_VTARGET)
      param[PARAM_VTARGET] = (uint8_t)(power_vtgt_mv() / 100);
    ans[1] = STATUS_CMD_OK;
    ans[2] = param[b[1]];
    return 3;
  case CMD_OSCCAL:
    ans[1] = STATUS_CMD_OK;
    return 2;
  case CMD_LOAD_ADDRESS:
    address = (uint32_t)b[1] << 24 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 8 | b[4];
    ans[1] = STATUS_CMD_OK;
    return 2;
  case CMD_ENTER_PROGMODE_ISP:
    return cmd_enter_progmode(b);
  case CMD_LEAVE_PROGMODE_ISP:
    return cmd_leave_progmode(b);
  case CMD_CHIP_ERASE_ISP:
    return cmd_chip_erase(b);
  case CMD_PROGRAM_FLASH_ISP:
    return cmd_program(b, true);
  case CMD_PROGRAM_EEPROM_ISP:
    return cmd_program(b, false);
  case CMD_READ_FLASH_ISP:
    return cmd_read(b, true);
  case CMD_READ_EEPROM_ISP:
    return cmd_read(b, false);
  case CMD_PROGRAM_FUSE_ISP:
  case CMD_PROGRAM_LOCK_ISP:
    isp(b[1], b[2], b[3], b[4]);
    ans[1] = STATUS_CMD_OK;
    ans[2] = STATUS_CMD_OK;
    return 3;
  case CMD_READ_FUSE_ISP:
  case CMD_READ_LOCK_ISP:
  case CMD_READ_SIGNATURE_ISP:
  case CMD_READ_OSCCAL_ISP:
    return cmd_read_byte(b);
  case CMD_SPI_MULTI:
    return cmd_spi_multi(b);
  default:
    ans[1] = STATUS_CMD_UNKNOWN;
    return 2;
  }
}

void avrisp_mode_init(void) {
  param[PARAM_HW_VER] = 2;
  param[PARAM_SW_MAJOR] = 2;
  param[PARAM_SW_MINOR] = 10;
  param[PARAM_SCK_DURATION] = 1;   // 230.4 kHz: fine for targets >= 1 MHz
  param[PARAM_RESET_POLARITY] = 1;

  spi_init(spi0, sck_hz(param[PARAM_SCK_DURATION]));
  spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_init(PIN_ISP_RESET);
  reset_active(false);
  spi_pins(false);
}

void avrisp_mode_task(void) {
  int c = usbio_getc(cdc_proto, 0);
  if (c != MESSAGE_START)
    return;

  uint8_t hdr[4];
  if (!usbio_read(cdc_proto, hdr, 4, TIMEOUT_MS) || hdr[3] != TOKEN)
    return;
  size_t len = (size_t)hdr[1] << 8 | hdr[2];
  if (len == 0 || len > sizeof(msg))
    return;
  uint8_t sum_in;
  if (!usbio_read(cdc_proto, msg, len, TIMEOUT_MS) || !usbio_read(cdc_proto, &sum_in, 1, TIMEOUT_MS))
    return;

  uint8_t sum = MESSAGE_START ^ hdr[0] ^ hdr[1] ^ hdr[2] ^ hdr[3];
  for (size_t i = 0; i < len; i++)
    sum ^= msg[i];
  if (sum != sum_in)
    return;   // host retries

  size_t n = handle(msg, len);
  send_answer(hdr[0], n);
}
