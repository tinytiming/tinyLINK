// Target power (Q1/Q2 via TPWR_EN), VTGT sense and the U2 level-shifter
// enable.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "board.h"
#include "console.h"
#include "led.h"
#include "power.h"

#if TINYLINK_BOARD_PICO

// A plain Pico has no power switch or level shifter; VTGT is read straight
// from VTREF on GP28.
void power_init(void) {
  adc_init();
  adc_gpio_init(PIN_VTREF_SENSE);
  adc_select_input(ADC_VTREF);
}

void power_task(void) {}
const char *power_set(bool on) { return on ? "no target power switch on this board" : NULL; }
bool power_wait_on(uint32_t timeout_ms) { return false; }
bool power_is_on(void) { return false; }
bool power_shifter_on(void) { return false; }

int power_vtgt_mv(void) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++)
    sum += adc_read();
  return (int)(sum * 3300u / (4u * 4096u));
}

#else

#define POWER_ON_TIMEOUT_MS 20     // R9/R10/C3 soft start, then F1 + C4
#define POWER_GOOD_MV       2800   // either JP9 rail (3V3 or 5V) clears this
#define POWER_SAG_MV        2500
#define POWER_IDLE_MAX_MV   500    // above this the target powers itself

typedef enum { PWR_OFF, PWR_STARTING, PWR_ON } pwr_state_t;

static pwr_state_t state;
static absolute_time_t deadline, next_sample;
static int vtgt_mv, sag_count;
static bool shifter;

static int read_vtgt_mv(void) {
  // 4-sample average; the ADC reference is the module's 3V3 rail.
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++)
    sum += adc_read();
  return (int)(sum * 3300u * 2u / (4u * 4096u));
}

static void set_shifter(bool on) {
  if (on == shifter)
    return;
  shifter = on;
  gpio_put(PIN_SHIFT_OE, on);
}

static void fault(const char *why) {
  gpio_put(PIN_TPWR_EN, 0);
  state = PWR_OFF;
  led_blink(RGB_RED, 2);
  console_printf("target power fault: %s (VTGT %d.%02d V)\r\n", why, vtgt_mv / 1000, (vtgt_mv % 1000) / 10);
}

void power_init(void) {
  gpio_init(PIN_TPWR_EN);
  gpio_put(PIN_TPWR_EN, 0);
  gpio_set_dir(PIN_TPWR_EN, GPIO_OUT);

  gpio_init(PIN_SHIFT_OE);
  gpio_put(PIN_SHIFT_OE, 0);
  gpio_set_dir(PIN_SHIFT_OE, GPIO_OUT);

  adc_init();
  adc_gpio_init(PIN_VTGT_SENSE);
  adc_select_input(ADC_VTGT);
  vtgt_mv = read_vtgt_mv();
}

void power_task(void) {
  if (absolute_time_diff_us(next_sample, get_absolute_time()) < 0)
    return;
  next_sample = make_timeout_time_ms(1);
  vtgt_mv = read_vtgt_mv();

  if (vtgt_mv >= VTGT_SHIFT_ON_MV)
    set_shifter(true);
  else if (vtgt_mv < VTGT_SHIFT_OFF_MV)
    set_shifter(false);

  switch (state) {
  case PWR_STARTING:
    if (vtgt_mv >= POWER_GOOD_MV) {
      state = PWR_ON;
      sag_count = 0;
      console_printf("target power on (VTGT %d.%02d V)\r\n", vtgt_mv / 1000, (vtgt_mv % 1000) / 10);
    } else if (absolute_time_diff_us(deadline, get_absolute_time()) >= 0) {
      fault("short/overload or JP9 open");
    }
    break;
  case PWR_ON:
    if (vtgt_mv < POWER_SAG_MV) {
      if (++sag_count >= 3)
        fault("short/overload");
    } else {
      sag_count = 0;
    }
    break;
  default:
    break;
  }
}

const char *power_set(bool on) {
  if (!on) {
    gpio_put(PIN_TPWR_EN, 0);
    if (state != PWR_OFF)
      console_printf("target power off\r\n");
    state = PWR_OFF;
    return NULL;
  }
  if (state != PWR_OFF)
    return NULL;
  vtgt_mv = read_vtgt_mv();
  if (vtgt_mv > POWER_IDLE_MAX_MV)
    return "target is already powered";   // Q1/Q2 would connect the two rails
  gpio_put(PIN_TPWR_EN, 1);
  state = PWR_STARTING;
  deadline = make_timeout_time_ms(POWER_ON_TIMEOUT_MS);
  return NULL;
}

bool power_is_on(void) { return state != PWR_OFF; }
bool power_shifter_on(void) { return shifter; }
int power_vtgt_mv(void) { return vtgt_mv; }

bool power_wait_on(uint32_t timeout_ms) {
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  while (state == PWR_STARTING && absolute_time_diff_us(end, get_absolute_time()) < 0)
    power_task();
  return state == PWR_ON;
}

#endif
