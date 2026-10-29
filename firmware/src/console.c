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
#include "modes.h"
#include "power.h"
#include "version.h"

static char line[64];
static size_t line_len;

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
  tud_cdc_n_write((uint8_t)cdc_console, buf, (uint32_t)n);
  tud_cdc_n_write_flush((uint8_t)cdc_console);
}

static void print_status(void) {
  int mv = power_vtgt_mv();
  console_printf("tinyLINK %s, mode %d %s\r\n", TINYLINK_VERSION, mode_current + 1, mode()->name);
  console_printf("VTGT %d.%02d V, target power %s, level shifter %s\r\n",
                 mv / 1000, (mv % 1000) / 10, power_is_on() ? "on" : "off",
                 power_shifter_on() ? "on" : "off (VTGT < 3.1 V)");
}

static void print_help(void) {
  console_printf("commands:\r\n"
                 "  status          mode, VTGT, power\r\n"
                 "  mode [N|name]   list modes, or switch (saves and re-enumerates)\r\n"
                 "  power on|off    target power (needs JP9 bridged)\r\n"
                 "  boot            reboot into the UF2 bootloader\r\n"
                 "  reset           reboot\r\n");
}

static void cmd_mode(const char *arg) {
  if (!*arg) {
    for (int i = 0; i < MODE_COUNT; i++)
      console_printf("%c %d %-6s %s\r\n", i == (int)mode_current ? '*' : ' ', i + 1, modes[i].name, modes[i].title);
    return;
  }
  int n = atoi(arg);
  if (n >= 1 && n <= MODE_COUNT) {
    mode_select((mode_id_t)(n - 1));
    return;
  }
  for (int i = 0; i < MODE_COUNT; i++) {
    if (!strcasecmp(arg, modes[i].name)) {
      mode_select((mode_id_t)i);
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
    reset_usb_boot(0, 0);
  } else if (!strcasecmp(cmd, "reset")) {
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
