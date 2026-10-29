// USB descriptors, built at boot from the current mode:
//   [CMSIS-DAP v2 vendor] [CDC protocol] [CDC target UART] [CDC console]
#include <string.h>
#include "pico/unique_id.h"
#include "tusb.h"
#include "modes.h"
#include "usb_descriptors.h"

// pid.codes test VID/PID. Replace with an allocated pid.codes PID before a
// public release (see README).
#define USB_VID   0x1209
#define USB_PID   0x0001
#define USB_BCD   0x0100

enum {
  STR_LANG = 0,
  STR_MANUFACTURER,
  STR_PRODUCT,
  STR_SERIAL,
  STR_DAP,
  STR_PROTO,
  STR_UART,
  STR_CONSOLE,
  STR_COUNT
};

int cdc_proto = -1;
int cdc_uart = -1;
int cdc_console = -1;
int itf_dap = -1;

static tusb_desc_device_t desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  // IAD, so composite CDC works on every OS.
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = USB_BCD,
  .iManufacturer      = STR_MANUFACTURER,
  .iProduct           = STR_PRODUCT,
  .iSerialNumber      = STR_SERIAL,
  .bNumConfigurations = 1,
};

#define CONFIG_MAX_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN + 3 * TUD_CDC_DESC_LEN)
static uint8_t desc_config[CONFIG_MAX_LEN];

static char serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 3];
static char product_str[32];

static const char *string_desc[STR_COUNT] = {
  [STR_MANUFACTURER] = "tinyLINK",
  [STR_PRODUCT]      = product_str,
  [STR_SERIAL]       = serial_str,
  [STR_DAP]          = "tinyLINK CMSIS-DAP",
  [STR_PROTO]        = NULL,   // mode title
  [STR_UART]         = "tinyLINK target UART",
  [STR_CONSOLE]      = "tinyLINK console",
};

static void append(uint8_t **p, const uint8_t *src, size_t len) {
  memcpy(*p, src, len);
  *p += len;
}

void usb_descriptors_init(void) {
  const probe_mode_t *m = mode();

  // A different serial per mode makes Windows treat each mode as its own
  // device instance instead of reusing a cached interface layout.
  pico_get_unique_board_id_string(serial_str, sizeof(serial_str) - 2);
  size_t n = strlen(serial_str);
  serial_str[n] = '-';
  serial_str[n + 1] = (char)('1' + mode_current);
  serial_str[n + 2] = 0;

  if (m->dap)
    strcpy(product_str, m->title);   // contains "CMSIS-DAP"
  else {
    strcpy(product_str, "tinyLINK ");
    strncat(product_str, m->name, sizeof(product_str) - strlen(product_str) - 1);
  }
  string_desc[STR_PROTO] = m->title;

  uint8_t itf = 0, ep = 1, cdc = 0;
  uint8_t *p = desc_config + TUD_CONFIG_DESC_LEN;

  if (m->dap) {
    // Interface 0: old Keil versions only look there.
    uint8_t d[] = { TUD_VENDOR_DESCRIPTOR(itf, STR_DAP, ep, 0x80 | ep, 64) };
    append(&p, d, sizeof(d));
    itf_dap = itf;
    itf += 1;
    ep += 1;
    desc_device.bcdUSB = 0x0210;   // BOS + MS OS 2.0 for WinUSB
  }

  struct { bool on; int *idx; uint8_t str; } ports[] = {
    { m->proto_cdc, &cdc_proto,   STR_PROTO },
    { m->uart_cdc,  &cdc_uart,    STR_UART },
    { true,         &cdc_console, STR_CONSOLE },
  };
  for (size_t i = 0; i < TU_ARRAY_SIZE(ports); i++) {
    if (!ports[i].on)
      continue;
    uint8_t d[] = { TUD_CDC_DESCRIPTOR(itf, ports[i].str, 0x80 | ep, 8, ep + 1, 0x80 | (ep + 1), 64) };
    append(&p, d, sizeof(d));
    *ports[i].idx = cdc++;
    itf += 2;
    ep += 2;
  }

  uint16_t total = (uint16_t)(p - desc_config);
  uint8_t hdr[] = { TUD_CONFIG_DESCRIPTOR(1, itf, 0, total, 0, 100) };
  memcpy(desc_config, hdr, sizeof(hdr));
}

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_config;
}

static uint16_t desc_str[48];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  uint8_t len;
  if (index == STR_LANG) {
    desc_str[1] = 0x0409;
    len = 1;
  } else {
    if (index >= STR_COUNT || !string_desc[index])
      return NULL;
    const char *s = string_desc[index];
    len = (uint8_t)strlen(s);
    if (len > TU_ARRAY_SIZE(desc_str) - 1)
      len = TU_ARRAY_SIZE(desc_str) - 1;
    for (uint8_t i = 0; i < len; i++)
      desc_str[1 + i] = (uint8_t)s[i];
  }
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
  return desc_str;
}

// ---------------------------------------------------------------------------
// BOS + Microsoft OS 2.0 descriptors so Windows binds WinUSB to the CMSIS-DAP
// v2 interface without an INF (same layout as raspberrypi/debugprobe).

#define MS_OS_20_VENDOR_CODE   1
#define MS_OS_FEATURE_DESC_LEN 0xA0
#define MS_OS_20_DESC_LEN      (0x0A + 0x08 + MS_OS_FEATURE_DESC_LEN)
#define BOS_TOTAL_LEN          (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static uint8_t const desc_bos[] = {
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, MS_OS_20_VENDOR_CODE),
};

static uint8_t desc_ms_os_20[] = {
  U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),
  // Function subset for the DAP interface (always interface 0)
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), 0, 0, U16_TO_U8S_LE(MS_OS_FEATURE_DESC_LEN),
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(MS_OS_FEATURE_DESC_LEN - 0x08 - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A),
  'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0,
  'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0, 'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
  U16_TO_U8S_LE(0x0050),
  '{', 0, 'C', 0, 'D', 0, 'B', 0, '3', 0, 'B', 0, '5', 0, 'A', 0, 'D', 0, '-', 0,
  '2', 0, '9', 0, '3', 0, 'B', 0, '-', 0, '4', 0, '6', 0, '6', 0, '3', 0, '-', 0,
  'A', 0, 'A', 0, '3', 0, '6', 0, '-', 0, '1', 0, 'A', 0, 'A', 0, 'E', 0, '4', 0,
  '6', 0, '4', 0, '6', 0, '3', 0, '7', 0, '7', 0, '6', 0, '}', 0, 0, 0, 0, 0,
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

uint8_t const *tud_descriptor_bos_cb(void) {
  return desc_bos;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  if (stage != CONTROL_STAGE_SETUP)
    return true;
  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
      request->bRequest == MS_OS_20_VENDOR_CODE && request->wIndex == 7 && itf_dap >= 0) {
    uint16_t total_len;
    memcpy(&total_len, desc_ms_os_20 + 8, 2);
    return tud_control_xfer(rhport, request, desc_ms_os_20, total_len);
  }
  return false;
}
