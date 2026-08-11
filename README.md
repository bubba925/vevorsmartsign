# DIY LED Sign: Driving a "Broken" Quarter-Scan HUB75 Panel with ESPHome

A Home Assistant / ESPHome-integrated LED matrix sign built from a VEVOR programmable P10 LED sign, showing a clock and "Now Playing" info from Plex.

## What I Wanted
I wanted to create an LED sign for our home theater that would auto update via Home Assistant to show what was currently playing in the Home Theater. I have a TV acting as a constantly rotating movie poster based on my Plex library already that shows Now Playing when this happens but I wanted to add the sign as well. When nothing is playing, the sign would just scroll text like "Home Theater [TIME]". I could also change it for parties or any other scenario. All of these things turn on and off based on occupancy sensors in my home.

The panels turned out to use a non-standard, undocumented internal wiring
scheme that no existing library or driver supported out of the box. This repo
contains the final working firmware **and** the full diagnostic process that
led to it, because the process is likely more useful to the next person than
the code alone — if you have a similarly "unsupported" quarter-scan panel,
the [troubleshooting journey](/TROUBLESHOOTING_JOURNEY.md) below is a
template for figuring out yours too.

## Quick facts

- **Panels:** 3× `HYP10-3535(2727)16*32-4S-1` — P10 outdoor, 1/4-scan (four-scan),
  SMD3535, 32×16px each, chained horizontally to 96×16px total. [Vevor Sign Amazon Link](https://a.co/d/08ug8vGD)
- **Controller:** [Seengreat RGB Matrix HUB75 S3](https://seengreat.com/product/359/) —
  ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB octal PSRAM). [Amazon Link](https://a.co/d/07XlClHT)
- **Driver chip on panel:** FM6124 (confirmed from a sticker on the board — not
  documented anywhere else).
- **Firmware:** ESPHome, with a custom external component (`four_scan_hub75`)
  wrapping the [mrcodetastic/ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
  library, vendored locally (see [why](/TROUBLESHOOTING_JOURNEY.md#the-platformio-packaging-detour)).
- **Home Assistant integration:** current time, Plex "now playing" title for
  one specific user/client, and an editable idle-text entity — all via
  ESPHome's native HA API sensors, no MQTT needed.

## Why this was hard

This panel type is a "1/4 duty" (four-scan) outdoor P10 module. Standard HUB75
panels light 2 rows at a time (`R1`/`R2` data lines, driven by address lines
`A`-`D`/`E`). Four-scan panels are supposed to light 4 rows at a time using
the same wires, which normally requires a special coordinate-remapping trick.

This exact panel turned out to have **two of its four address lines (C, D)
completely non-functional** — confirmed by swapping every combination of
address pins with zero change in behavior. Instead, the panel encodes the
missing row-select bit *inside the column address itself* (specifically, bit
3 of the electrical column). This is not documented anywhere we could find,
and isn't something any existing HUB75 library or WLED build handles. It was
reverse-engineered from scratch by methodically testing single pixels at
known coordinates and reading back exactly where they physically appeared.

See [`/TROUBLESHOOTING_JOURNEY.md`](/TROUBLESHOOTING_JOURNEY.md) for
the full story, and [`/THE_FORMULA.md`](/THE_FORMULA.md) for the
technical breakdown of the final pixel-mapping formula.

## Repo contents

```
led-sign.yaml                          Full ESPHome configuration
components/four_scan_hub75/            Custom ESPHome display component
  __init__.py                          (empty - marks the Python package)
  display.py                           Component config schema + codegen
  four_scan_hub75_display.h            C++ header - the pixel-mapping formula
  four_scan_hub75_display.cpp          C++ implementation - driver setup
  <vendored library files>             ESP32-HUB75-MatrixPanel-DMA + Adafruit
                                        GFX source, vendored flat (see below)
/
  HARDWARE.md                          Panel specs, pin mapping, board photos
  TROUBLESHOOTING_JOURNEY.md           The full debugging story
  THE_FORMULA.md                       How the pixel-mapping formula works
```

## Quick start (if you have the *exact* same hardware)

1. Install the [ESPHome add-on](https://esphome.io) in Home Assistant (or use
   the standalone ESPHome CLI).
2. Copy `led-sign.yaml` and the `components/four_scan_hub75/` folder into
   your ESPHome config directory, next to each other.
3. Edit the `wifi:`, `api:`, and Plex `entity_id:` fields in `led-sign.yaml`
   for your own network and Home Assistant setup.
4. Flash. Framework must be `arduino` (not `esp-idf`) — the vendored library
   needs Arduino-level APIs.

## Quick start (if you have a *similar but different* quarter-scan panel)

Your panel's exact bit-mapping is very likely **different** from this one —
"four-scan" panels from different manufacturers wire their internal
multiplexing differently, and there's no universal standard. Don't assume our
formula in `four_scan_hub75_display.h` will work unmodified.

Instead:
1. Read [`/THE_FORMULA.md`](/THE_FORMULA.md) to understand *why* our
   formula looks the way it does.
2. Follow the single-pixel testing methodology in
   [`/TROUBLESHOOTING_JOURNEY.md`](/TROUBLESHOOTING_JOURNEY.md#the-winning-methodology)
   to derive your own panel's formula.
3. Swap in your own formula in `CustomFourScanMapping::apply()`.

## Credits / prior art

- [mrcodetastic/ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) —
  the underlying DMA driver library this whole project is built on.
- The Arduino Forum thread ["P10 32x16 LED panel 1/4 scan now works with
  Adafruit_GFX"](https://forum.arduino.cc/t/p10-32x16-led-panel-1-4-scan-now-works-with-adafruit_gfx/483621) —
  first documented the existence of "snake-pattern" wiring in this panel
  family, which was the key insight that pointed us toward the right kind of
  fix.
- [Eager-LED P10 Outdoor 1/4 Scan SMD3535 datasheet](https://www.eagerled.com/) —
  confirmed the panel's true electrical spec ("1/4 duty" driving mode).
- Claude for documenting everything and making the entire process faster.
