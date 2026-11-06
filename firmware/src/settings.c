// Saved mode and flags, in the last 4 KB flash sector. The flags record sits
// after the original mode record, so older firmware still finds its mode and
// an old sector reads as flags = 0.
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "settings.h"

#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SETTINGS_MAGIC  0x4B4E4C54u   // "TLNK"
#define FLAGS_MAGIC     0x47414C46u   // "FLAG"

typedef struct {
  uint32_t magic;
  uint32_t value;
  uint32_t check;
} record_t;

static uint32_t check_of(const record_t *r) {
  return ~(r->magic ^ (r->value * 2654435761u));
}

static int load(int index, uint32_t magic) {
  const record_t *r = (const record_t *)(XIP_BASE + SETTINGS_OFFSET) + index;
  if (r->magic != magic || r->check != check_of(r))
    return -1;
  return (int)r->value;
}

int settings_load_mode(void) {
  return load(0, SETTINGS_MAGIC);
}

uint32_t settings_load_flags(void) {
  int flags = load(1, FLAGS_MAGIC);
  return flags < 0 ? 0 : (uint32_t)flags;
}

static void save(int mode, uint32_t flags) {
  static uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0xFF, sizeof(page));
  record_t r[2] = {
    { .magic = SETTINGS_MAGIC, .value = (uint32_t)mode },
    { .magic = FLAGS_MAGIC, .value = flags },
  };
  r[0].check = check_of(&r[0]);
  r[1].check = check_of(&r[1]);
  memcpy(page, r, sizeof(r));

  uint32_t irq = save_and_disable_interrupts();
  flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(SETTINGS_OFFSET, page, FLASH_PAGE_SIZE);
  restore_interrupts(irq);
}

void settings_save_mode(int mode) {
  if (settings_load_mode() == mode)
    return;
  save(mode, settings_load_flags());
}

void settings_save_flags(uint32_t flags) {
  int mode = settings_load_mode();
  if (mode >= 0 && settings_load_flags() == flags)
    return;
  save(mode < 0 ? 0 : mode, flags);
}
