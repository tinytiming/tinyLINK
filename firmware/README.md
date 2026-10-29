# tinyLINK firmware

Pico SDK firmware for the [tinyLINK hardware](../hardware/README.md). One image holds all eight
modes. The mode is saved in flash; switching modes reboots so USB re-enumerates with that mode's
interfaces.

## Using it

- **BOOT, short press:** next mode (saved, reboots).
- **BOOT, hold 1.5 s:** toggle target power. The LED blinks white once on power on/off, and red twice
  on a fault or refusal.
- **LED:** mode colour (table below).
- **Target power** is off at every boot and needs JP9 bridged. The probe refuses to switch it on if
  VTGT is already above 0.5 V (a self-powered target). After switch-on VTGT must reach 2.8 V within
  20 ms, and while on it must not sag below 2.5 V for 3 ms. Otherwise power goes off, the LED blinks
  red twice and the console prints `target power fault: short/overload or JP9 open` (or
  `short/overload` for a sag).
- **Level shifter:** U2 (SHIFT_OE) is enabled while VTGT >= 3.1 V and disabled below 3.0 V, with
  VTGT = ADC0 × 2. With U2 fitted every mode, including the logic analyser, needs VTGT (from the
  target or probe power).

Every mode has a **console** port (the last CDC port, "tinyLINK console"), any baud rate:

```
status          mode, VTGT, power, level shifter
mode [N|name]   list modes, or switch
power on|off    target power
boot            reboot into the UF2 bootloader
reset           reboot
```

Opening and closing the console at 1200 baud also reboots into the UF2 bootloader.

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

The protocol port is always the first CDC port: the lowest `ttyACMn` on Linux, and
`/dev/serial/by-id/` names ending in `-if00` (`-if01` in the CMSIS-DAP modes). Each mode has its
own USB serial number (`<board id>-<mode>`), so Windows keeps a driver binding per mode. CMSIS-DAP
uses WinUSB via MS OS 2.0 descriptors, so it needs no driver install.

Pins per mode: see the [channel table](../hardware/README.md#channels). PulseView names the channels
D2-D9 after the GPIO numbers, so D2 = IO0. nRESET, the ESP32 EN/BOOT lines and the AVR RESET are
open-drain.

### Mode notes

- **SWD / JTAG:** [free-dap](https://github.com/ataradov/free-dap) bit-banged on SIO. Both modes
  accept SWD and JTAG and differ only in the default port. The clock delays are scaled from free-dap's
  RP2040 port and still need measuring (fast path about 8.9 MHz).
- **WCH:** SWIO for CH32V003/V00x with PicoRVD's PIO program, speaking minichlink's "ardulink" serial
  protocol. That's one USB round trip per debug register access: slower than a WCH-LinkE, fine for
  16 KB parts. minichlink's power on/off maps to target power, so it can power-cycle a locked chip.
- **UPDI:** PIO open-drain half-duplex UART on IO1. The probe echoes its own bytes like the resistor
  SerialUPDI adapter, follows the host's baud rate, parity and stop bits (including pymcuprog's
  300-baud double break) and handles a CDC break.
- **UART:** UART1 on IO2/IO3. RTS/DTR follow esptool's auto-reset logic: RTS alone pulls EN low, DTR
  alone pulls BOOT low.
- **LA:** up to 131072 samples into RAM (PIO + DMA, 5 kS/s to 100 MS/s), then streamed. One capture
  per run, so PulseView's software trigger only fires inside that window. No analog channels.
- **SPI:** SPI0, mode 0, up to 30 MHz (default 8 MHz). Pins stay high-Z until flashrom's first SPI
  operation and after it releases them.
- **ISP:** STK500v2 (AVR068) on SPI0. Default SCK 230.4 kHz, for targets at 1 MHz and up; `-B`
  SCK_DURATION 0/1/2/3 = 921.6/230.4/57.6/28.8 kHz. Pins stay high-Z outside programming mode.

## Building

Pico SDK 2.x (built with 2.2.0) and arm-none-eabi-gcc:

```sh
cd tinyLINK/firmware
mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j8
```

Flash `firmware/build/tinylink.uf2`: hold BOOT while plugging in the RP2040-Zero and copy it to the
RPI-RP2 drive. Later updates: `boot` on the console, or the 1200-baud touch. Without an installed
picotool, cmake downloads and builds one (set `picotool_DIR` to avoid that).

`cmake .. -DLED_ORDER_RGB=ON` for RP2040-Zero LEDs that take R,G,B instead of the usual G,R,B.

## Raspberry Pi Pico / picoprobe PCB

The same firmware runs on a plain Raspberry Pi Pico, for example on the
[picoprobe PCB](https://github.com/Fabien-Chouteau/picoprobe-pcb):

```sh
cmake -B build-pico -DPICO_BOARD=pico -DPICO_SDK_PATH=$PICO_SDK_PATH
make -C build-pico -j8
```

- **LED (GP25):** blinks the mode number (1-8) every 2 s; a fast flicker for power on/off, faults
  and refusals.
- **No level shifter or power switch:** 3.3 V targets only, powered by themselves. `power on` is
  refused.
- **VTREF:** the picoprobe PCB routes VTREF to Pico pin 34 (GP28), not to 3V3. The firmware reads it
  on ADC2 (no divider) as VTGT for `status`, minichlink's power check and avrdude's VTARGET, and
  never drives GP28.
- **SWIO / UPDI:** IO1 only has the RP2040's internal pull-up here, see the SWIO pull-up note in
  [`../hardware/README.md`](../hardware/README.md#open-questions-for-future-revisions).

IO0-IO3 (GP2-GP5) land on the picoprobe header. The rest are only on the Pico's own pins
(GP6 = pin 9, GP7 = pin 10, GP8 = pin 11, GP9 = pin 12), for a flying lead:

| Picoprobe signal (J1 / J2 pin) | GPIO | SWD | WCH | UPDI | UART | LA | SPI | JTAG | ISP |
|---|---|---|---|---|---|---|---|---|---|
| SWDCLK (2 / 4) | GP2 | SWCLK | | | | D2 | SCK | TCK | SCK |
| SWDIO (4 / 2) | GP3 | SWDIO | SWIO | UPDI | | D3 | MOSI | TMS | MOSI |
| ~TRST (5 / 10) | GP4 | TX | TX | TX | TX | D4 | MISO | TX | MISO |
| SWO (6 / 6) | GP5 | RX | RX | RX | RX | D5 | CS | RX | |
| Pico pin 9 | GP6 | nRESET | | | EN | D6 | | nSRST | RESET |
| Pico pin 10 | GP7 | | | | BOOT | D7 | | TDO | |
| Pico pin 11 | GP8 | | | | | D8 | | TDI | |
| Pico pin 12 | GP9 | | | | | D9 | | | |

GND is J1 pin 3, or J2 pins 3/5. On J2, pin 10 is normally the target's nRESET, and in SWD/JTAG/WCH/
UPDI modes it carries the target UART TX: anything sent on the target-UART port then pulses the
reset (the stock picoprobe firmware does the same). Leave that port idle, or use J1.

## Not done yet

- USB VID/PID is the pid.codes test pair 1209:0001; a public release needs its own PID (free from
  pid.codes for open hardware).
- Not implemented: WCH RVSWD (CH32V2xx/V3xx/X0), SWO, dirtyJTAG, UART CTS/RTS flow control, LA RLE
  compression and streaming triggers.

## Licences

MIT, see [`../LICENSE`](../LICENSE). `third_party/free-dap`: BSD-3-Clause (Alex Taradov).
`src/swio.pio`: from PicoRVD, MIT (aappleby). `src/ws2812.pio` and the BOOTSEL read in
`src/button.c` follow pico-examples (BSD-3-Clause, Raspberry Pi). The sigrok and STK500v2 protocols
were implemented from their descriptions.
