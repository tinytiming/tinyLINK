// Saved mode, in the last 4 KB flash sector.
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "settings.h"

#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SETTINGS_MAGIC  0x4B4E4C54u   // "TLNK"

typedef struct {
  uint32_t magic;
  uint32_t mode;
  uint32_t check;
} settings_t;

static uint32_t check_of(const settings_t *s) {
  return ~(s->magic ^ (s->mode * 2654435761u));
}

int settings_load_mode(void) {
  const settings_t *s = (const settings_t *)(XIP_BASE + SETTINGS_OFFSET);
  if (s->magic != SETTINGS_MAGIC || s->check != check_of(s))
    return -1;
  return (int)s->mode;
}

void settings_save_mode(int mode) {
  if (settings_load_mode() == mode)
    return;
  static uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0xFF, sizeof(page));
  settings_t s = { .magic = SETTINGS_MAGIC, .mode = (uint32_t)mode };
  s.check = check_of(&s);
  memcpy(page, &s, sizeof(s));

  uint32_t irq = save_and_disable_interrupts();
  flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(SETTINGS_OFFSET, page, FLASH_PAGE_SIZE);
  restore_interrupts(irq);
}
