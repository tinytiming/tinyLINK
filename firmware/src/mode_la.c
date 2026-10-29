// Logic analyser mode: 8 channels (IO0-IO7 = GP2-GP9) speaking the
// sigrok "raspberrypi-pico" serial protocol, so PulseView/sigrok-cli work
// with the stock driver. PulseView names the channels D2-D9 (the GPIO
// numbers): D2 = IO0 ... D9 = IO7.
//
// Each run captures into RAM with PIO + DMA, then streams the buffer:
// at most LA_MAX_SAMPLES per run, up to ~100 MS/s. Continuous mode ('C',
// used for software triggers) is served the same way, so a trigger must
// fall inside one capture.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "tusb.h"
#include "board.h"
#include "modes.h"
#include "usbio.h"
#include "la.pio.h"

#define LA_MAX_SAMPLES (128u * 1024u)
#define LA_MAX_RATE    100000000u

static uint8_t buf[LA_MAX_SAMPLES] __attribute__((aligned(4)));

static PIO pio = pio0;
static uint sm, offset;
static int dma_chan;

static uint32_t rate = 1000000, limit = 1000;
static uint32_t dmask = 0xFF;
static char line[24];
static size_t line_len;

void la_mode_init(void) {
  for (uint i = 0; i < IO_COUNT; i++) {
    gpio_init(PIN_IO(i));
    pio_gpio_init(pio, PIN_IO(i));
    gpio_set_dir(PIN_IO(i), GPIO_IN);
  }
  sm = pio_claim_unused_sm(pio, true);
  offset = pio_add_program(pio, &la_program);
  pio_sm_set_consecutive_pindirs(pio, sm, PIN_IO_BASE, IO_COUNT, false);
  dma_chan = dma_claim_unused_channel(true);
}

static void reply(const char *s) {
  usbio_write(cdc_proto, s, strlen(s));
  usbio_flush(cdc_proto);
}

// True if the host asked to stop ('+' abort or '*' reset).
static bool host_abort(void) {
  uint8_t c;
  while (tud_cdc_n_peek((uint8_t)cdc_proto, &c)) {
    if (c == '+' || c == '*') {
      tud_cdc_n_read((uint8_t)cdc_proto, &c, 1);
      return true;
    }
    tud_cdc_n_read((uint8_t)cdc_proto, &c, 1);   // stray byte
  }
  return false;
}

static uint32_t capture(uint32_t samples) {
  uint32_t words = (samples + 3) / 4;

  pio_sm_config c = la_program_get_default_config(offset);
  sm_config_set_in_pins(&c, PIN_IO_BASE);
  sm_config_set_in_shift(&c, true, true, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (float)rate);
  pio_sm_init(pio, sm, offset, &c);

  dma_channel_config d = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&d, DMA_SIZE_32);
  channel_config_set_read_increment(&d, false);
  channel_config_set_write_increment(&d, true);
  channel_config_set_dreq(&d, pio_get_dreq(pio, sm, false));
  dma_channel_configure(dma_chan, &d, buf, &pio->rxf[sm], words, true);

  pio_sm_set_enabled(pio, sm, true);
  while (dma_channel_is_busy(dma_chan)) {
    usbio_service();
    if (host_abort()) {
      dma_channel_abort(dma_chan);
      pio_sm_set_enabled(pio, sm, false);
      return 0;
    }
  }
  pio_sm_set_enabled(pio, sm, false);
  return samples;
}

static void stream(uint32_t samples) {
  static uint8_t out[256];
  size_t n = 0;
  uint32_t bytes = 0;
  bool d4 = (dmask & ~0x0Fu) == 0;

  for (uint32_t i = 0; i < samples; i++) {
    uint8_t s = buf[i];
    if (d4) {
      out[n++] = 0x80 | (s & 0x0F);   // no RLE, one sample per byte
    } else {
      if (dmask & 0x7F)
        out[n++] = 0x80 | (s & 0x7F);
      if (dmask & 0x80)
        out[n++] = 0x80 | (s >> 7);
    }
    if (n >= sizeof(out) - 2) {
      usbio_write(cdc_proto, out, n);
      bytes += n;
      n = 0;
      if (host_abort())
        break;
    }
  }
  if (n) {
    usbio_write(cdc_proto, out, n);
    bytes += n;
  }
  char end[16];
  snprintf(end, sizeof(end), "$%lu+", (unsigned long)bytes);
  reply(end);
}

static void run_capture(void) {
  uint32_t samples = limit < LA_MAX_SAMPLES ? limit : LA_MAX_SAMPLES;
  if (!samples)
    samples = LA_MAX_SAMPLES;
  samples = capture(samples);
  if (samples)
    stream(samples);
}

static void handle_line(void) {
  line[line_len] = 0;
  char cmd = line[0];
  const char *arg = &line[1];
  switch (cmd) {
  case 'i':
    reply("SRPICO,A001D08,02");
    break;
  case 'R': {
    uint32_t r = (uint32_t)strtoul(arg, NULL, 10);
    if (r < 5000 || r > LA_MAX_RATE) {
      reply("rate out of range");
      break;
    }
    rate = r;
    reply("*");
    break;
  }
  case 'L':
    limit = (uint32_t)strtoul(arg, NULL, 10);
    reply("*");
    break;
  case 'D': {
    // D<en><idx>
    if (line_len >= 3) {
      int en = arg[0] - '0';
      int idx = atoi(arg + 1);
      if (idx >= 0 && idx < 8) {
        if (en)
          dmask |= 1u << idx;
        else
          dmask &= ~(1u << idx);
        reply("*");
      }
    }
    break;
  }
  case 'A':   // no analog channels; ack so the host carries on
  case 't':
  case 'p':
    reply("*");
    break;
  case 'F':
  case 'C':
    run_capture();
    break;
  default:
    break;
  }
}

void la_mode_task(void) {
  int c = usbio_getc(cdc_proto, 0);
  if (c < 0)
    return;
  if (c == '*' || c == '+') {   // reset / abort while idle
    line_len = 0;
    return;
  }
  if (c == '\n' || c == '\r') {
    if (line_len)
      handle_line();
    line_len = 0;
    return;
  }
  if (line_len < sizeof(line) - 1)
    line[line_len++] = (char)c;
}
