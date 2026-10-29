# tinyLINK rev A fab

Generated 2026-10-28 with kicad-cli 10.0.3 from `tinylink.kicad_pcb` (saved 22:35), zones refilled at export.
DRC: 0 unconnected; 4 starved-thermal errors on J3 pins 1/2 to the VTGT zone (1 spoke each, electrically
fine); 10 schematic-parity warnings on unconnected U1/J4 pads (auto `unconnected-` net names, no real
mismatch). ERC: 0 errors.

- `tinylink-gerbers-JLC.zip`: upload this. 2 layers, 26 × 52 mm, 1.6 mm thick. The U-slot under the
  RP2040-Zero is in Edge.Cuts (routed), no NPTH holes.
- `gerbers/`: the same files, plus PDF drill maps.
- `tinylink-BOM-JLC.csv` / `tinylink-CPL-JLC.csv`: top-side PCBA, 13 BOM lines, 30 parts.

Assembly scope (JLC, top side only):

- Assembled: C1-C5, D1, D2, F1, J1, J2, Q1-Q3, R1-R16, U2.
- Extended parts: U2 TXS0108EPWR (C17206), F1 MF-FSMF050X-2 (C210357), J1/J2 HC-1.0-3PWT (C2845362).
  Everything else is basic, Q3 BSS138 (C7420339) is preferred.
- CPL rotations are KiCad's, except U2: its KiCad TSSOP-20 footprint is 90° off JLC's, so the CPL has
  180° instead of -90° (pin 1 at 116.72, 108.12, next to C1). The EasyEDA footprints (D1, D2, Q1-Q3)
  and the JST-SH connectors match JLC as-is. Check U2 pin 1, D2 cathode (GND pad, towards J1) and
  J1/J2 mouths facing the board edges in JLC's placement preview.

Hand-fitted after assembly:

- **U1:** RP2040-Zero module, soldered flat by its castellated edge over the U-slot.
- **J3:** 2×10 2.54 mm pin header (THT).
- **J4:** 2×5 1.27 mm SMD header on the back (C2935458), hand-soldered to keep the assembly one-sided.
- **JP1-JP8:** bypass solder jumpers, close only for the 3.3 V-only build (U2, C1, C2 unfitted).
- **JP9:** ships open; solder 1-2 (3V3) or 2-3 (5V) before using target power.
