# Hardware Reference

## LED Panels

**Model:** `HYP10-3535(2727)16*32-4S-1` (printed on the panel PCB)

Matches the manufacturer spec for a **P10 Outdoor 1/4 Scan SMD3535** module
(model `P10-O4S-SMD3535-32x16`):

| Spec | Value |
|---|---|
| Pixel pitch | 10mm |
| LED package | SMD3535 |
| Resolution per panel | 32px (W) × 16px (H) |
| Physical size | 320mm × 160mm |
| Driving mode | Constant current, **1/4 duty** (four-scan) |
| Driver chip | **FM6124** (confirmed via sticker on panel PCB — not otherwise documented) |

Three panels are chained horizontally (1 row × 3 columns) for a total
addressable area of **96×16 pixels**.

### Row-multiplexing quirk (the actual hard part)

This panel's HUB75 connector exposes all 5 standard address lines
(A/B/C/D/E), but **only A and B are electrically functional**. This was
confirmed by testing every combination of address-pin swaps (A↔B, B↔C, C↔D)
and observing *zero change* in behavior each time — a real, functioning
address line would produce different behavior when swapped with a
non-functional one.

Instead of using C as a third address bit, the panel encodes the missing
row-select information in **bit 3 of the column address**. See
[`THE_FORMULA.md`](THE_FORMULA.md) for the full technical explanation.

## Controller Board

**Model:** Seengreat "RGB Matrix HUB75 S3" (Rev 1.0)

- MCU: ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB **octal** PSRAM)
- Onboard HUB75 output header
- MicroSD slot, RTC, audio codec (unused in this project)

### Pin mapping used

| Signal | GPIO | Signal | GPIO |
|---|---|---|---|
| R1 | 5 | G1 | 4 |
| B1 | 6 | R2 | 15 |
| G2 | 7 | B2 | 17 |
| A | 8 | B | 18 |
| C | 10 | D | 9 |
| E | 16 | CLK | 12 |
| LAT | 11 | OE | 13 |

(C and D are wired per the board's silkscreen labeling, but — per above —
don't do anything on this particular panel.)

## Power

Panels are powered directly from a dedicated 5V supply inside the sign
enclosure, wired separately from the ESP32-S3 controller's own power. Do not
try to power the panels from the controller board's own 5V rail — LED
matrices at this size draw far more current than a microcontroller board can
supply.
