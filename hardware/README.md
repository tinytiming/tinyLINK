# tinyLINK

Multi-mode debug probe (RP2040-Zero carrier), rev A draft.

Open-hardware carrier for a Waveshare RP2040-Zero. One combined firmware; the module's
BOOT button cycles modes and its WS2812 shows the mode colour.

| # | Mode | LED | Tools |
|---|------|-----|-------|
| 1 | SWD + UART (debugprobe, CMSIS-DAP) | green | OpenOCD, probe-rs, pyOCD |
| 2 | WCH: SWIO (CH32V003, 1-wire) and RVSWD (CH32V2xx/V3xx/X0, 2-wire), PicoRVD + minichlink-compatible | purple | minichlink, gdb |
| 3 | UPDI (SerialUPDI) | yellow | pymcuprog, avrdude serialupdi |
| 4 | UART bridge (CTS/RTS; ESP32 auto-reset) | white | any terminal, esptool |
| 5 | Logic analyser, 8 ch (sigrok-pico) | blue | PulseView |
| 6 | SPI flash (serprog) | orange | flashrom |
| 7 | JTAG (CMSIS-DAP JTAG or dirtyJTAG) | cyan | openFPGALoader, OpenOCD |
| 8 | AVR ISP (STK500v2 / avrisp) | red | avrdude |

## Renders

KiCad 3D renders of rev A. "Assembled" includes the RP2040-Zero module and leaves out the
hand-soldered J3 2x10 header; "bare" is the PCB only.

| | Front | Back |
|---|---|---|
| Assembled | <img src="renders/tinylink-front-filled.png" width="260"> | <img src="renders/tinylink-back-filled.png" width="260"> |
| Bare | <img src="renders/tinylink-front-bare.png" width="260"> | <img src="renders/tinylink-back-bare.png" width="260"> |

![Isometric render](renders/tinylink-iso.png)

## Channels

Eight channels IO0-IO7. Each goes RP2040 GPIO -> optional TXS0108E (U2, auto-direction) -> 33R
(R1-R8) -> connectors. JP1-JP8 bypass U2 per channel. On J3 each IO has its own GND pin beside it (2x10 header,
even row GND except pin 2 = VTGT), handy for logic-analyser and fast SWD/JTAG leads.

| Ch | RP2040 | SWD | WCH | UPDI | UART | LA | SPI | JTAG | ISP |
|----|--------|-----|-----|------|------|----|-----|------|-----|
| IO0 | GP2 | SWCLK | RVSWD CLK | | | CH0 | SCK | TCK | SCK |
| IO1 | GP3 | SWDIO | SWIO / RVSWD DIO | UPDI | | CH1 | MOSI | TMS | MOSI |
| IO2 | GP4 | TX | TX | TX | TX | CH2 | MISO | | MISO |
| IO3 | GP5 | RX | RX | RX | RX | CH3 | CS | | |
| IO4 | GP6 | nRESET | | | CTS / EN (host RTS) | CH4 | | nSRST | RESET |
| IO5 | GP7 | SWO | | | RTS / IO0 (host DTR) | CH5 | | TDO | |
| IO6 | GP8 | | | | | CH6 | | TDI | |
| IO7 | GP9 | | | | | CH7 | | | |

WCH RVSWD uses the SWD pins (IO1 data, IO0 clock); TPWR can power-cycle the target to unbrick it.
ESP32 auto-reset in UART mode follows esptool: host RTS -> IO4 (target EN), host DTR -> IO5
(target IO0/BOOT). The firmware maps the CDC control lines, so the pins are unchanged.

GP2-5 are SPI0 SCK/TX/RX/CSn and GP4-7 are UART1 TX/RX/CTS/RTS, so SPI and UART can use the
hardware peripherals; GP2-9 are contiguous for PIO and the logic analyser.

U2 needs VCCA <= VCCB (VTGT). VCCA_SHIFT = 3V3 - D1 (1N5819WS) Vf, about 3.0-3.1 V, with R15 (10k)
bleeding ~0.3 mA to keep D1 forward-biased, so VCCA stays below VTGT even when the probe supplies 3V3
through F1 and Q1/Q2. The target range with U2 fitted is about 3.1-5.5 V.

GPIO-high current through U2's A-side pull-ups can lift VCCA a little above that: with all 8 lines
high and the 10k bleed, VCCA_SHIFT settles at about 3.15 V, still below the ~3.25 V that VTGT reaches
on 3V3 target power at light load (VTGT drops further with target current: F1 is 0.1-0.8 ohm plus
~2 x 60 mOhm for Q1/Q2). Bench check on the first boards: measure VCCA_SHIFT vs VTGT with all IOs
high and 3V3 target power; if VCCA >= VTGT, drop R15 to 4.7k.

U2's internal pull-ups (TXS0108E datasheet) are about 4 kOhm while an output is high and about
40 kOhm while it drives low; they replace the old external ones. Avoid external pull-ups on the
shifted lines, or keep them >= 50k (the TXS one-shots fight stronger ones).

GP10 SHIFT_OE drives U2 OE (R14 pull-down keeps the outputs high-Z while the RP2040 boots or with no
target). VTGT_SENSE is VTGT/2 (R12/R13 100k/100k), so firmware reconstructs VTGT = ADC0 volts x 2 and
raises SHIFT_OE only when VTGT >= 3.1 V, i.e. ADC0 >= 1.55 V.

Other pins: GP14 TPWR_EN (target power), GP26/ADC0 VTGT_SENSE (VTGT/2), GP16 WS2812 (on module).
Free: GP0, GP1, GP11, GP12, GP13, GP15, GP27-29.

**3.3 V-only build:** leave U2, C1 and C2 unfitted and close JP1-JP8. Solder JP9 1-2 (3V3).
Never close JP1-JP8 with U2 fitted. In this build the IOs are driven straight from the RP2040 at
3.3 V, so the target must run at 3.3 V or have 3.3 V-tolerant inputs: never use it on 5 V targets,
nor on targets below ~3 V unless their inputs tolerate 3.3 V.

### Target power jumper (JP9)

The 3V3 / 5V selector is JP9, a 3-pad solder jumper on the front. The silk doesn't say "JP9": look for
the "3v3" and "5v" labels with arrows pointing down at it. It sits at the right-hand edge, just below
the bottom-right corner of the RP2040-Zero, between U2 (the TSSOP-20) and the SWD connector J1. A small
triangle under the left pad marks pin 1.

- **Ships open:** the probe gives the target no power until you bridge it.
- **3V3:** solder the centre pad to the left pad, under "3v3" (pins 1-2, the module's LDO).
- **5V:** solder the centre pad to the right pad, under "5v" (pins 2-3, USB VBUS).
- **Never bridge all three:** that shorts the module's 3V3 to USB 5V.
- Target power switching (BOOT long-press, or `power on` on the console) only works with JP9 bridged.
  With it open, VTGT has to come from a self-powered target.

### Solder jumpers

The eight bypass jumpers JP1-JP8 sit on the back in one row under the "lvl shifter bypass" text,
between the U-slot and J4. They have no per-channel labels, so here is the map. Seen from the back,
left to right (the last one, JP8, sits slightly higher, next to the "UART" label):

| Jumper | Bridges | Close for |
|--------|---------|-----------|
| JP1 | IO0_A - IO0_B (GP2) | 3.3 V-only build |
| JP2 | IO1_A - IO1_B (GP3) | 3.3 V-only build |
| JP3 | IO2_A - IO2_B (GP4) | 3.3 V-only build |
| JP4 | IO3_A - IO3_B (GP5) | 3.3 V-only build |
| JP5 | IO4_A - IO4_B (GP6) | 3.3 V-only build |
| JP6 | IO5_A - IO5_B (GP7) | 3.3 V-only build |
| JP7 | IO6_A - IO6_B (GP8) | 3.3 V-only build |
| JP8 | IO7_A - IO7_B (GP9) | 3.3 V-only build |
| JP9 | VSRC to 3V3 (1-2) or 5V (2-3), front, "3v3 / 5v" | target power source, ships open |

Close all eight together, and only with U2 unfitted.

## Target power (VTGT)

VTGT is both the target supply out and U2's B-side reference in.
JP9 picks the source. JP9 ships open: no target power until you solder 1-2 (3V3, module LDO) or
2-3 (5V USB). F1 500 mA PPTC,
Q1/Q2 AO3401A back-to-back (no back-feed from a self-powered target), Q3 BSS138 driver from GP14,
R9/R10/C3 soft start. Firmware must check VTGT (= ADC0 x 2) is ~0 V before switching power on.
D2 (red 0603 LED, R16 2.2k from VTGT) lights whenever VTGT is present, probe- or self-powered;
dark means JP9 open, F1 tripped or no target power.

Target power fault: after TPWR_EN high, if VTGT (ADC0 x 2) doesn't reach the expected rail within ~5 ms
(or sags while on), drop TPWR_EN, blink the LED red x2 and print 'target power fault:
short/overload or JP9 open' on the CDC port. F1 is a self-resetting PPTC; it recovers after power-off.

## Connectors

| Ref | Type | Pinout |
|-----|------|--------|
| J1 | JST-SH 3 side entry (Pi debug, SWD) | 1 SWCLK (IO0), 2 GND, 3 SWDIO (IO1) |
| J2 | JST-SH 3 side entry (Pi debug, UART) | 1 TX (IO2), 2 GND, 3 RX (IO3) |
| J3 | 2x10 2.54 mm (THT, hand-soldered) | odd row: 1 VTGT, 3 GND, 5-19 IO0-IO7; even row (board edge side): 2 VTGT, 4-20 GND, so every signal has a ground next to it |
| J4 | 2x5 1.27 mm Cortex Debug (SMD), full ARM 10-pin JTAG | 1 VTref, 2 SWDIO/TMS (IO1), 4 SWCLK/TCK (IO0), 6 SWO/TDO (IO5), 8 TDI (IO6), 10 nRESET (IO4), 3/5/9 GND, 7 key |

## Status

- Schematic: done (labels only, no wires; tidy as you like). Description + LCSC fields on all parts.
- PCB: routed, 2 layers, about 26 x 52 mm, U-slot under the RP2040-Zero, J4 on the back. JP9 ships open
  (solder 1-2 for 3V3 or 2-3 for 5V).
- Extended parts at JLC: TXS0108EPWR (C17206), JST-SH clone (C2845362) x2, 2x5 1.27 header (C2935458).

## Known limitations

- With U2 fitted, VTGT must stay above VCCA_SHIFT (~3.0-3.15 V), so targets below about 3.1 V need the
  3.3 V-only build (U2 unfitted, JP1-JP8 closed) and must tolerate 3.3 V inputs. With probe-supplied
  3V3 the margin shrinks with target current (F1 + Q1/Q2 drop); keep 3V3-powered targets to roughly
  100-200 mA, or use 5V / self-powered for heavier ones, and do the bench check above.
