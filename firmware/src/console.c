// "tinyLINK console" CDC port: status, mode and target power commands, and
// the place power faults are reported. Opening it at 1200 baud and closing
// it reboots into the UF2 bootloader (Arduino style).
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include "console.h"
#include "led.h"
#include "modes.h"
#include "power.h"
#include "version.h"

static char line[64];
static size_t line_len;

// Waits for TX FIFO space with tud_task() only: usbio_write() would run
// console_task() and recurse.
static void console_write(const char *p, size_t len) {
  uint8_t itf = (uint8_t)cdc_console;
  absolute_time_t end = make_timeout_time_ms(100);
  while (len) {
    uint32_t n = tud_cdc_n_write(itf, p, (uint32_t)len);
    p += n;
    len -= n;
    tud_cdc_n_write_flush(itf);
    if (len) {
      if (absolute_time_diff_us(end, get_absolute_time()) >= 0)
        return;   // nobody reading
      tud_task();
    }
  }
}

void console_printf(const char *fmt, ...) {
  if (cdc_console < 0 || !tud_cdc_n_connected((uint8_t)cdc_console))
    return;
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0)
    return;
  if ((size_t)n >= sizeof(buf))
    n = sizeof(buf) - 1;
  console_write(buf, (size_t)n);
}

// Lets queued console output reach the host before a reboot.
static void console_drain(void) {
  if (cdc_console < 0 || !tud_cdc_n_connected((uint8_t)cdc_console))
    return;
  uint8_t itf = (uint8_t)cdc_console;
  tud_cdc_n_write_flush(itf);
  absolute_time_t end = make_timeout_time_ms(100);
  while (tud_cdc_n_write_available(itf) < CFG_TUD_CDC_TX_BUFSIZE &&
         absolute_time_diff_us(end, get_absolute_time()) < 0)
    tud_task();
  sleep_ms(10);   // last packet on the wire
}

// Unformatted, for text longer than console_printf's buffer.
static void console_puts(const char *s) {
  if (cdc_console < 0 || !tud_cdc_n_connected((uint8_t)cdc_console))
    return;
  console_write(s, strlen(s));
}

static void print_status(void) {
  int mv = power_vtgt_mv();
  console_printf("tinyLINK %s, mode %d %s\r\n", TINYLINK_VERSION, mode_current + 1, mode()->name);
#if TINYLINK_BOARD_PICO
  console_printf("VTGT %d.%02d V (VTREF on GP28; no target power switch or level shifter)\r\n",
                 mv / 1000, (mv % 1000) / 10);
  return;
#endif
  console_printf("VTGT %d.%02d V, target power %s, level shifter %s\r\n",
                 mv / 1000, (mv % 1000) / 10, power_is_on() ? "on" : "off",
                 power_shifter_on() ? "on" : "off (VTGT < 3.1 V)");
}

static void print_help(void) {
  console_puts("commands:\r\n"
               "  status          mode, VTGT, power\r\n"
               "  mode [N|name]   list modes, or switch (saves and re-enumerates)\r\n"
#if TINYLINK_BOARD_PICO
               "  power on|off    target power (not on this board)\r\n"
#else
               "  power on|off    target power (needs JP9 bridged)\r\n"
#endif
               "  boot            reboot into the UF2 bootloader\r\n"
               "  reset           reboot\r\n");
}

#if !TINYLINK_BOARD_PICO
static const char *colour(uint32_t rgb) {
  switch (rgb) {
  case RGB_GREEN:  return "green";
  case RGB_PURPLE: return "purple";
  case RGB_YELLOW: return "yellow";
  case RGB_WHITE:  return "white";
  case RGB_BLUE:   return "blue";
  case RGB_ORANGE: return "orange";
  case RGB_CYAN:   return "cyan";
  case RGB_RED:    return "red";
  default:         return "?";
  }
}
#endif

static void switch_mode(int i) {
#if TINYLINK_BOARD_PICO
  console_printf("switching to mode %d %s (LED blinks %dx), rebooting...\r\n", i + 1, modes[i].name, i + 1);
#else
  console_printf("switching to mode %d %s (%s), rebooting...\r\n", i + 1, modes[i].name, colour(modes[i].rgb));
#endif
  console_drain();
  mode_select((mode_id_t)i);
}

static void cmd_mode(const char *arg) {
  if (!*arg) {
    for (int i = 0; i < MODE_COUNT; i++)
      console_printf("%c %d %-6s %s\r\n", i == (int)mode_current ? '*' : ' ', i + 1, modes[i].name, modes[i].title);
    return;
  }
  int n = atoi(arg);
  if (n >= 1 && n <= MODE_COUNT) {
    switch_mode(n - 1);
    return;
  }
  for (int i = 0; i < MODE_COUNT; i++) {
    if (!strcasecmp(arg, modes[i].name)) {
      switch_mode(i);
      return;
    }
  }
  console_printf("unknown mode '%s'\r\n", arg);
}

static void run(char *cmd) {
  while (*cmd == ' ')
    cmd++;
  char *arg = strchr(cmd, ' ');
  if (arg) {
    *arg++ = 0;
    while (*arg == ' ')
      arg++;
  } else {
    arg = cmd + strlen(cmd);
  }

  if (!*cmd)
    return;
  if (!strcasecmp(cmd, "status") || !strcmp(cmd, "s"))
    print_status();
  else if (!strcasecmp(cmd, "help") || !strcmp(cmd, "?"))
    print_help();
  else if (!strcasecmp(cmd, "mode") || !strcmp(cmd, "m"))
    cmd_mode(arg);
  else if (!strcasecmp(cmd, "power") || !strcmp(cmd, "p")) {
    const char *err = NULL;
    if (!strcasecmp(arg, "on"))
      err = power_set(true);
    else if (!strcasecmp(arg, "off"))
      err = power_set(false);
    else
      print_status();
    if (err)
      console_printf("target power: %s\r\n", err);
  } else if (!strcasecmp(cmd, "boot")) {
    console_printf("rebooting into the UF2 bootloader...\r\n");
    console_drain();
    reset_usb_boot(0, 0);
  } else if (!strcasecmp(cmd, "reset")) {
    console_printf("rebooting...\r\n");
    console_drain();
    watchdog_reboot(0, 0, 10);
  } else {
    console_printf("unknown command, try 'help'\r\n");
  }
}

void console_task(void) {
  if (cdc_console < 0)
    return;
  uint8_t itf = (uint8_t)cdc_console;
  while (tud_cdc_n_available(itf)) {
    int c = tud_cdc_n_read_char(itf);
    if (c == '\r' || c == '\n') {
      if (!line_len && c == '\n')
        continue;
      console_printf("\r\n");
      line[line_len] = 0;
      line_len = 0;
      run(line);
      console_printf("> ");
    } else if (c == 0x7F || c == 0x08) {
      if (line_len) {
        line_len--;
        console_printf("\b \b");
      }
    } else if (c >= 0x20 && line_len < sizeof(line) - 1) {
      line[line_len++] = (char)c;
      tud_cdc_n_write_char(itf, (char)c);
      tud_cdc_n_write_flush(itf);
    }
  }
}

void console_line_state(bool dtr, bool rts) {
  (void)rts;
  cdc_line_coding_t coding;
  tud_cdc_n_get_line_coding((uint8_t)cdc_console, &coding);
  if (!dtr && coding.bit_rate == 1200)
    reset_usb_boot(0, 0);
  if (dtr) {
    line_len = 0;
    print_status();
    console_printf("> ");
  }
}
