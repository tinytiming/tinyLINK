# tinyLINK

<p align="center"><img src="hardware/renders/tinylink-iso.png" alt="tinyLINK rev A, 3D render" width="600"></p>

Open-hardware multi-mode debug probe on a Waveshare RP2040-Zero. One firmware covers SWD/JTAG
(CMSIS-DAP), WCH SWIO, UPDI, AVR ISP, SPI flash, a UART bridge and an 8-channel logic analyser.
The I/O is level-shifted for 3.1-5.5 V targets, and target power is switchable.

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

The BOOT button cycles modes, and the module's WS2812 shows the mode colour.

## Layout

- [`hardware/`](hardware): KiCad 10 project (schematic, PCB, project libraries), the schematic PDF,
  the silkscreen logo generator in `logo/`, and the frozen rev A with JLC gerbers, BOM and CPL in
  `revA/fab/`. The pinout, channel map and power notes are in [`hardware/README.md`](hardware/README.md).
- [`firmware/`](firmware): Pico SDK firmware for all eight modes. Build and usage notes are in
  [`firmware/README.md`](firmware/README.md).

## Status

- Hardware rev A: 26 × 52 mm, 2 layers. Fab files are ready; boards are not built or tested yet.
- Firmware: builds, not yet tested on hardware.

## License

MIT, see [`LICENSE`](LICENSE). Third-party parts keep their own licences: `firmware/third_party/free-dap`
(BSD-3-Clause, Alex Taradov), the PicoRVD and pico-examples code listed in
[`firmware/README.md`](firmware/README.md#licences), and `hardware/RP2040-Zero-Kicad-Fixed`
(harshitgoel96/RP2040-Zero-Kicad-Fixed).
