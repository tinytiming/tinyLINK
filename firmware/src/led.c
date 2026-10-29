// Mode colour on the RP2040-Zero WS2812, plus short blink patterns. On a
// plain Pico the single LED blinks the mode number instead.
#include "pico/stdlib.h"
#include "board.h"
#include "led.h"

#if TINYLINK_BOARD_PICO

#include "modes.h"

#define MODE_ON_MS      150
#define MODE_OFF_MS     250
#define MODE_PERIOD_MS  2000
#define FLICKER_MS      60

static int step;                 // mode pattern: 2 steps per blink, then the pause
static absolute_time_t step_next;
static int blink_left;           // flicker half-periods remaining
static absolute_time_t blink_next;

void led_init(void) {
  gpio_init(PIN_LED);
  gpio_set_dir(PIN_LED, GPIO_OUT);
  gpio_put(PIN_LED, 0);
}

void led_set(uint32_t rgb) {
  step = 0;
  step_next = get_absolute_time();
}

void led_blink(uint32_t rgb, int count) {
  blink_left = count * 2;
  blink_next = get_absolute_time();
}

void led_task(void) {
  absolute_time_t now = get_absolute_time();
  if (blink_left) {
    if (absolute_time_diff_us(blink_next, now) < 0)
      return;
    blink_left--;
    gpio_put(PIN_LED, blink_left & 1);
    blink_next = make_timeout_time_ms(FLICKER_MS);
    if (!blink_left)
      led_set(0);
    return;
  }
  if (absolute_time_diff_us(step_next, now) < 0)
    return;
  int n = mode_current + 1;
  if (step < 2 * n) {
    bool on = !(step & 1);
    gpio_put(PIN_LED, on);
    step_next = make_timeout_time_ms(on ? MODE_ON_MS : MODE_OFF_MS);
    step++;
  } else {
    gpio_put(PIN_LED, 0);
    step_next = make_timeout_time_ms(MODE_PERIOD_MS - n * (MODE_ON_MS + MODE_OFF_MS));
    step = 0;
  }
}

#else

#include "hardware/pio.h"
#include "ws2812.pio.h"

// G,R,B, the usual RP2040-Zero order. Some RP2040-Zero batches
// are reported to be R,G,B: configure with -DLED_ORDER_RGB=ON if red and green swap.
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

#endif
