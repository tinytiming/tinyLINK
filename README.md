# tinyLINK

<p align="center"><img src="hardware/renders/tinylink-iso.png" alt="tinyLINK rev A, 3D render" width="600"></p>

Open-hardware multi-mode debug probe on a Waveshare RP2040-Zero, with level-shifted I/O for
3.1-5.5 V targets and switchable target power. Rev A hardware and the firmware are not tested yet.

| # | Mode | LED | Tools |
|---|------|-----|-------|
| 1 | SWD + UART (CMSIS-DAP) | green | OpenOCD, probe-rs, pyOCD |
| 2 | WCH SWIO (CH32V003) | purple | minichlink |
| 3 | UPDI (SerialUPDI) | yellow | pymcuprog, avrdude serialupdi |
| 4 | UART bridge (ESP32 auto-reset) | white | any terminal, esptool |
| 5 | Logic analyser, 8 ch (sigrok-pico) | blue | PulseView |
| 6 | SPI flash (serprog) | orange | flashrom |
| 7 | JTAG (CMSIS-DAP) | cyan | openFPGALoader, OpenOCD |
| 8 | AVR ISP (STK500v2) | red | avrdude |

The BOOT button cycles modes; the module's WS2812 shows the mode colour.

**Target power jumper:** JP9 ("3v3 / 5v" on the front) ships open, so there is no target power
until you solder its centre pad to 3v3 or to 5v, never both. See
[Target power](hardware/README.md#target-power-vtgt).

- [`hardware/`](hardware/README.md): KiCad 10 project, renders, logo generator, and rev A fab files
  (JLC gerbers, BOM, CPL) in `revA/fab/`.
- [`firmware/`](firmware/README.md): Pico SDK firmware, usage and build.

MIT licence ([`LICENSE`](LICENSE)). Third-party parts keep their own licences: see
[firmware licences](firmware/README.md#licences) and `hardware/RP2040-Zero-Kicad-Fixed`.
