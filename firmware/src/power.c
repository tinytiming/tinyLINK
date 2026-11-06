// Target power (Q1/Q2 via TPWR_EN), VTGT sense and the U2 level-shifter
// enable.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "board.h"
#include "console.h"
#include "led.h"
#include "power.h"
#include "settings.h"

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
bool power_leak_allowed(void) { return false; }
const char *power_allow_leak(bool allow) { return "no target power switch on this board"; }

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
#define POWER_SETTLE_MS     30     // report VTGT this long after power-good, once settled
#define POWER_IDLE_MAX_MV   500    // above this the target powers itself
#define POWER_LEAK_MAX_MV   2500   // with allow_leak: IO leakage into an unpowered target

typedef enum { PWR_OFF, PWR_STARTING, PWR_ON } pwr_state_t;

static pwr_state_t state;
static absolute_time_t deadline, next_sample;
static int vtgt_mv, sag_count;
static bool shifter;
static bool allow_leak;
static bool report_pending;
static absolute_time_t report_at;

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
  report_pending = false;
  led_blink(RGB_RED, 2);
  console_notify("target power fault: %s (VTGT %d.%02d V)\r\n", why, vtgt_mv / 1000, (vtgt_mv % 1000) / 10);
}

void power_init(void) {
  allow_leak = settings_load_flags() & SETTING_POWER_ALLOW_LEAK;
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
      report_pending = true;
      report_at = make_timeout_time_ms(POWER_SETTLE_MS);
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
    if (report_pending && absolute_time_diff_us(report_at, get_absolute_time()) >= 0) {
      report_pending = false;
      console_notify("target power on (VTGT %d.%02d V)\r\n", vtgt_mv / 1000, (vtgt_mv % 1000) / 10);
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
      console_notify("target power off\r\n");
    state = PWR_OFF;
    report_pending = false;
    return NULL;
  }
  if (state != PWR_OFF)
    return NULL;
  vtgt_mv = read_vtgt_mv();
  if (vtgt_mv > (allow_leak ? POWER_LEAK_MAX_MV : POWER_IDLE_MAX_MV))
    return allow_leak ? "target is already powered"
                      : "target is already powered ('power leak on' allows < 2.5 V leakage)";
  if (vtgt_mv > POWER_IDLE_MAX_MV)
    console_notify("VTGT %d.%02d V before power-on, treated as leakage\r\n",
                   vtgt_mv / 1000, (vtgt_mv % 1000) / 10);
  gpio_put(PIN_TPWR_EN, 1);
  state = PWR_STARTING;
  deadline = make_timeout_time_ms(POWER_ON_TIMEOUT_MS);
  return NULL;
}

bool power_is_on(void) { return state != PWR_OFF; }
bool power_shifter_on(void) { return shifter; }
bool power_leak_allowed(void) { return allow_leak; }

const char *power_allow_leak(bool allow) {
  allow_leak = allow;
  uint32_t flags = settings_load_flags();
  settings_save_flags(allow ? flags | SETTING_POWER_ALLOW_LEAK : flags & ~SETTING_POWER_ALLOW_LEAK);
  return NULL;
}
int power_vtgt_mv(void) { return vtgt_mv; }

bool power_wait_on(uint32_t timeout_ms) {
  absolute_time_t end = make_timeout_time_ms(timeout_ms);
  while ((state == PWR_STARTING || report_pending) &&
         absolute_time_diff_us(end, get_absolute_time()) < 0)
    power_task();
  return state == PWR_ON;
}

#endif
