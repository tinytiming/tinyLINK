# tinyLINK firmware

Firmware for the tinyLINK multi-mode debug probe ([`../hardware/`](../hardware), RP2040-Zero carrier).
One image holds all eight modes. The mode is saved in flash, and switching modes reboots
the probe so USB re-enumerates with that mode's interfaces.

Status: **written and built, not yet tested on hardware.** See "Not done yet" below.

## Using it

- **BOOT, short press:** go to the next mode (saved, reboots).
- **BOOT, hold 1.5 s:** toggle target power. The LED blinks white once when power turns on or off. It blinks red twice
  on a fault or refusal.
- **LED:** shows the mode colour (table below).
- **Target power** is off at every boot. It only works with JP9 bridged (1-2 = 3V3,
  2-3 = 5V). The probe refuses to switch it on if VTGT already has a voltage on it (a self-powered
  target), because Q1/Q2 would connect the two supplies. If VTGT doesn't come up within 20 ms,
  or sags below 2.5 V while on, the probe turns power off. It then blinks red twice and prints
  `target power fault` on the console.
- **Level shifter:** U2 is enabled (SHIFT_OE) only while VTGT ≥ 3.1 V, and disabled below
  3.0 V. VTGT = ADC0 × 2. Every mode, including the logic analyser, needs VTGT connected
  (or probe power) when U2 is fitted.

Every mode has a **console** port (the last CDC port, "tinyLINK console"). Open it at any
baud rate:

```
status          mode, VTGT, power, level shifter
mode [N|name]   list modes, or switch
power on|off    target power
boot            reboot into the UF2 bootloader
reset           reboot
```

Opening the console at 1200 baud and closing it also reboots into the UF2 bootloader.

| # | Mode | LED | USB ports (in order) | Host tools |
|---|------|-----|-----------------|------------|
| 1 | `swd`  | green  | CMSIS-DAP v2, target UART, console | OpenOCD (`interface/cmsis-dap.cfg`), probe-rs, pyOCD |
| 2 | `wch`  | purple | SWIO (ardulink), target UART, console | `minichlink -c /dev/ttyACM0 -w fw.bin flash -b` |
| 3 | `updi` | yellow | SerialUPDI, target UART, console | `pymcuprog -t uart -u /dev/ttyACM0 -d attiny1616 ...`, `avrdude -c serialupdi -P /dev/ttyACM0 ...` |
| 4 | `uart` | white  | target UART, console | any terminal, `esptool.py` (auto-reset) |
| 5 | `la`   | blue   | sigrok, console | PulseView / sigrok-cli, driver `raspberrypi-pico` |
| 6 | `spi`  | orange | serprog, console | `flashrom -p serprog:dev=/dev/ttyACM0,spispeed=8M` |
| 7 | `jtag` | cyan   | CMSIS-DAP v2 (JTAG default), target UART, console | OpenOCD, openFPGALoader (`-c cmsisdap`) |
| 8 | `isp`  | red    | STK500v2, console | `avrdude -c stk500v2 -P /dev/ttyACM0 -p m328p ...` |

The protocol port is always the first CDC port. On Linux that's the lowest `ttyACMn`;
`/dev/serial/by-id/` names end in `-if00` (or `-if01` in the CMSIS-DAP modes). Each mode reports
its own USB serial number (`<board id>-<mode>`), so Windows keeps a separate driver
binding per mode. CMSIS-DAP uses WinUSB through MS OS 2.0 descriptors, so it needs no driver install.

### Pins per mode

Same as the channel table in [`../hardware/README.md`](../hardware/README.md):

| Ch | GPIO | SWD | WCH | UPDI | UART | LA | SPI | JTAG | ISP |
|----|------|-----|-----|------|------|----|-----|------|-----|
| IO0 | GP2 | SWCLK | | | | D2 | SCK | TCK | SCK |
| IO1 | GP3 | SWDIO | SWIO | UPDI | | D3 | MOSI | TMS | MOSI |
| IO2 | GP4 | TX | TX | TX | TX | D4 | MISO | TX | MISO |
| IO3 | GP5 | RX | RX | RX | RX | D5 | CS | RX | |
| IO4 | GP6 | nRESET | | | EN (host RTS) | D6 | | nSRST | RESET |
| IO5 | GP7 | | | | IO0/BOOT (host DTR) | D7 | | TDO | |
| IO6 | GP8 | | | | | D8 | | TDI | |
| IO7 | GP9 | | | | | D9 | | | |

PulseView names the channels D2-D9 after the GPIO numbers, so D2 = IO0. nRESET, the ESP32 EN/BOOT
lines and the AVR RESET are open-drain: driven low, or released to the pull-up.

### Mode notes

- **SWD / JTAG:** [free-dap](https://github.com/ataradov/free-dap) (BSD-3) bit-banged on SIO.
  Both modes accept SWD and JTAG; they differ only in the default port. Clock delay
  constants are scaled from free-dap's RP2040 port and still need measuring (fast path is about 8.9 MHz).
  No SWO yet.
- **WCH:** SWIO for CH32V003/V00x, using PicoRVD's PIO program (MIT). It speaks minichlink's "ardulink"
  serial protocol, so it is one USB round trip per debug register access: slower than a WCH-LinkE but
  fine for 16 KB parts. minichlink's power on/off maps to target power, so it can power-cycle a
  locked chip.
- **UPDI:** PIO open-drain half-duplex UART on IO1. The probe receives its own
  bytes back, like the resistor SerialUPDI adapter, so the host tools work unchanged. It follows the host's
  baud rate, parity and stop bits, including pymcuprog's 300-baud double break, and it also
  handles a CDC break.
- **UART:** hardware UART1 on IO2/IO3. RTS/DTR follow the esptool auto-reset logic
  (RTS asserted alone pulls EN low; DTR asserted alone pulls BOOT low). CTS/RTS flow
  control is not implemented.
- **LA:** captures up to 131072 samples into RAM (PIO + DMA, 5 kS/s to 100 MS/s), then
  streams them. That's one capture per run, so PulseView's software trigger only fires inside
  that window. No analog channels.
- **SPI:** hardware SPI0, mode 0, up to 30 MHz (default 8 MHz). The pins stay high-Z until
  flashrom's first SPI operation and after it releases them.
- **ISP:** STK500v2 (AVR068) over CDC, hardware SPI0. The default SCK is 230.4 kHz, fine for
  targets at 1 MHz and up. Use `-B` for slower ones: SCK_DURATION 0/1/2/3 =
  921.6/230.4/57.6/28.8 kHz. The pins stay high-Z outside programming mode.

## Building

Pico SDK 2.x (built with 2.2.0) and arm-none-eabi-gcc:

```sh
cd tinylink
mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j8
```

Output is `build/tinylink.uf2`. Hold BOOT while plugging in the RP2040-Zero and copy the UF2
to the RPI-RP2 drive. Later updates: run `boot` on the console, or use the 1200-baud touch.
If cmake can't find picotool it downloads and builds it (needs network). To avoid that, point
`picotool_DIR` at an installed one.

Build options: `-DLED_ORDER_RGB=1` for RP2040-Zero LEDs that take R,G,B order (default is G,R,B, the usual
RP2040-Zero order).

## Not done yet

- Untested on hardware. Every mode compiles, but none has run on a board yet.
- USB VID/PID is the pid.codes test pair 1209:0001. A public release needs a PID allocated from
  pid.codes (free for open hardware).
- WCH RVSWD (2-wire, for CH32V2xx/V3xx/X0) isn't implemented. The WCH mode does SWIO only.
- Not implemented: SWO in SWD mode, dirtyJTAG, LA RLE compression and streaming triggers, UART
  flow control.

## Licences

tinyLINK firmware: MIT, see [`../LICENSE`](../LICENSE). `third_party/free-dap`: BSD-3-Clause (Alex Taradov).
`src/swio.pio`: from PicoRVD, MIT (aappleby). `src/ws2812.pio` and the BOOTSEL read in
`src/button.c` follow pico-examples (BSD-3-Clause, Raspberry Pi). The sigrok and STK500v2
protocols were implemented from their protocol descriptions.
