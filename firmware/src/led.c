// Mode colour on the RP2040-Zero WS2812, plus short blink patterns.
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "board.h"
#include "led.h"
#include "ws2812.pio.h"

// G,R,B, the usual RP2040-Zero order. Some RP2040-Zero batches
// are reported to be R,G,B: build with -DLED_ORDER_RGB=1 if red and green swap.
#ifndef LED_ORDER_RGB
#define LED_ORDER_RGB 0
#endif

#define LED_BRIGHTNESS 48   // of 255, the bare LED is very bright

static PIO led_pio = pio1;
static uint led_sm;
static uint32_t base_rgb;
static uint32_t shown = 0xFFFFFFFF;

static uint32_t blink_rgb;
static int blink_left;           // half-periods remaining
static absolute_time_t blink_next;

static void put(uint32_t rgb) {
  if (rgb == shown)
    return;
  shown = rgb;
  uint32_t r = ((rgb >> 16) & 0xFF) * LED_BRIGHTNESS / 255;
  uint32_t g = ((rgb >> 8) & 0xFF) * LED_BRIGHTNESS / 255;
  uint32_t b = (rgb & 0xFF) * LED_BRIGHTNESS / 255;
#if LED_ORDER_RGB
  uint32_t w = (r << 16) | (g << 8) | b;
#else
  uint32_t w = (g << 16) | (r << 8) | b;
#endif
  pio_sm_put_blocking(led_pio, led_sm, w << 8);
}

void led_init(void) {
  led_sm = pio_claim_unused_sm(led_pio, true);
  uint offset = pio_add_program(led_pio, &ws2812_program);
  ws2812_program_init(led_pio, led_sm, offset, PIN_WS2812, 800000);
  put(0);
}

void led_set(uint32_t rgb) {
  base_rgb = rgb;
  if (!blink_left)
    put(rgb);
}

void led_blink(uint32_t rgb, int count) {
  blink_rgb = rgb;
  blink_left = count * 2;
  blink_next = get_absolute_time();
}

void led_task(void) {
  if (!blink_left)
    return;
  if (absolute_time_diff_us(blink_next, get_absolute_time()) < 0)
    return;
  blink_left--;
  // odd half-periods show the blink colour, even ones are dark
  put(blink_left & 1 ? blink_rgb : 0);
  blink_next = make_timeout_time_ms(150);
  if (!blink_left)
    put(base_rgb);
}
