# tinyLINK rev A fab

Generated 2026-10-28 with kicad-cli 10.0.3 from `../tinylink.kicad_pcb`, zones refilled.

- `tinylink-gerbers-JLC.zip`: upload this. 2 layers, 26 × 52 mm, 1.6 mm. The U-slot is routed in
  Edge.Cuts; there are no NPTH holes.
- `gerbers/`: the same files plus PDF drill maps.
- `tinylink-BOM-JLC.csv` / `tinylink-CPL-JLC.csv`: top-side PCBA, 13 BOM lines, 30 parts.

## Assembly (JLC, top side)

- Placed: C1-C5, D1, D2, F1, J1, J2, Q1-Q3, R1-R16, U2.
- Extended parts: U2 TXS0108EPWR (C17206), F1 MF-FSMF050X-2 (C210357), J1/J2 HC-1.0-3PWT (C2845362).
  The rest are basic; Q3 BSS138 (C7420339) is preferred.
- Rotations are KiCad's except U2, set to 180° (KiCad's TSSOP-20 is 90° off JLC's). In JLC's
  placement preview check U2 pin 1 (next to C1), the D2 cathode (GND pad, towards J1) and the J1/J2
  mouths facing the board edges.

## Hand-fitted

- **U1:** RP2040-Zero, soldered flat by its castellated edge over the U-slot.
- **J3:** 2×10 2.54 mm pin header (THT).
- **J4:** 2×5 1.27 mm SMD header (C2935458) on the back, keeping the assembly one-sided.
- **JP1-JP8** (3.3 V-only build) and **JP9** (target power source, ships open): see
  [Level shifter](../../README.md#level-shifter) and [Target power](../../README.md#target-power-vtgt).
