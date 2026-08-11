# The Troubleshooting Journey

This project went through an unusually long diagnostic process — dozens of
build/flash/observe cycles over several sessions. This document is a
condensed narrative of that process: what we tried, what we learned from
each failure, and how we eventually converged on a working answer. If you're
fighting a similar "unsupported" HUB75 panel, the *process* here is probably
more useful to you than our specific numbers.

## Starting point

A pre-built LED sign (3 chained outdoor P10 panels) originally shipped with
a proprietary stock controller, driven by a Windows/Android app called
"iLedColor". The stock controller worked fine, but we wanted native Home
Assistant integration (clock + Plex now-playing), which meant replacing it
with an ESP32 running ESPHome.

## Phase 1: WLED and ESPHome's built-in `hub75` component

We first tried WLED's HUB75 support, then ESPHome's own built-in `hub75`
display platform, cycling through every documented `scan_wiring` and
`shift_driver` combination. Both consistently produced garbled or scrambled
output. In hindsight, this made sense: neither implementation's four-scan
support was built for whatever this specific panel does internally, and we
didn't yet know that.

**Lesson:** if a panel is scrambled under every "standard" scan-type setting
a library offers, it's worth suspecting the panel does something
non-standard, rather than continuing to cycle through settings.

## Phase 2: identifying the real panel type

Zooming into board photos revealed the model number silkscreened on the PCB:
`HYP10-3535(2727)16*32-4S-1`. Searching for this (and the "4S" suffix
specifically) eventually turned up the actual manufacturer spec sheet,
confirming this is a genuine "P10 Outdoor 1/4 Scan SMD3535" module — a true
four-scan (not two-scan) panel.

## The PlatformIO packaging detour

Once we knew we needed genuine four-scan support, we built a custom ESPHome
external component wrapping the
[mrcodetastic/ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
library directly (rather than ESPHome's built-in driver), since that library
has a dedicated `VirtualMatrixPanel` class for exactly this kind of panel.

Getting this to actually *compile* under the ESPHome Device Builder add-on
(no local dev machine, no terminal access to a real PlatformIO project) took
many rounds on its own:

- PlatformIO's `lib_deps` resolution repeatedly failed to correctly link
  `REQUIRES` between separately-fetched libraries (Adafruit GFX, Adafruit
  BusIO, the HUB75 library itself) under the ESP-IDF backend — a genuine,
  reproducible packaging bug, not a config mistake.
- The eventual fix: **vendor the library source directly** into the
  ESPHome external component's own folder (flat, no subfolders — ESPHome's
  local `external_components` only picks up files at the component's top
  level), bypassing `lib_deps` entirely. We stripped Adafruit GFX down to
  just the base class (removing `Adafruit_GrayOLED`/`Adafruit_SPITFT`, which
  were pulling in the broken BusIO dependency for functionality we didn't
  need).
- A subtler bug: our Python codegen forgot to actually wire the YAML
  `lambda:` block into the component (`set_writer()` was never called).
  `update()` was firing correctly the whole time — there was just nothing
  registered to call in response. This produced a "completely blank screen"
  symptom that looked identical to several *other* bugs we'd already fixed,
  which cost real time to isolate.

**Lesson:** when nothing shows up on screen at all, add logging directly
inside the draw-pixel function itself to confirm it's ever being called,
rather than assuming the drawing math is wrong.

## Phase 3: finding the right base configuration

With the component compiling, we still had scrambled output. This required
ruling out, one at a time, with clean single-variable tests each round:

- **Wrong driver chip** — fixed once we found a "138/6124" sticker on the
  physical panel PCB and set `driver = FM6124`. This alone fixed a
  watchdog-crash issue we'd been fighting separately.
- **Wrong `VirtualMatrixPanel` chain type** — we'd initially guessed
  `CHAIN_TOP_LEFT_DOWN` (for 2D serpentine grids); this was silently
  mirroring both X and Y before our own remap code even ran, since it's
  meant for multi-row panel grids, not our single-row 3-panel chain.
  `CHAIN_NONE` was correct.
- **Column-duplication ghosting** — pixels appearing twice, offset by a
  fixed distance — was fixed by setting `latch_blanking = 4`. The
  underlying library's own documentation describes this exact symptom
  ("clones with horizontal offset") as fixed by this setting; we just
  hadn't tried it in the right configuration yet.
- **R1/R2 hardware suspicion** — we spent a round suspecting the R2 data
  line might be physically dead. A clean, isolated single-pixel test
  (bypassing all our own remap code) proved both R1 and R2 work correctly
  on their own — this ruled out a hardware fault and refocused us back on
  software/addressing logic.
- **Address-line-order suspicion** — we tried every pairwise swap of
  A/B/C/D. None of them changed the behavior at all, which was itself
  useful information: it meant C and D genuinely aren't connected to
  anything on this panel, not just "connected to the wrong GPIO."

## The winning methodology

Once "wrong pin" theories were exhausted, we switched to pure empirical
mapping: draw **one single pixel at a known coordinate**, observe **exactly**
which physical LED lit up, and repeat systematically until a pattern emerges.

The key discipline that made this work:

1. **One variable at a time.** Testing multiple rows/columns simultaneously
   risks two logical positions colliding onto the same physical pixel, with
   the later draw silently overwriting the earlier one — producing
   misleading "missing" data.
2. **Fixed reference point.** Holding column constant while sweeping row
   (or vice versa) isolates exactly one axis of behavior per test round.
3. **Believe the data over the theory.** Several rounds produced results
   that contradicted our working hypothesis at the time; the productive
   move was always to trust the fresh data and revise the theory, not
   explain away the data.

This process eventually revealed: holding a "problem" row fixed and sweeping
the column value showed the physical row flipping between two options every
16 columns, controlled by exactly one bit (bit 3) of the column address —
the missing piece that let us write the final formula (see
[`THE_FORMULA.md`](THE_FORMULA.md)).

## Two final polish bugs

With the pixel mapping solid, two smaller issues remained:

- **Scrolling text was stuttering.** The display's `update_interval` had
  never been explicitly set, so it defaulted to 1 second — while the
  scroll-position variable was being updated every 50ms. The display was
  jumping ~20 pixels at a time instead of scrolling smoothly. Setting
  `update_interval: 50ms` to match fixed it immediately.
- **A few individual font pixels looked dropped** (top serif of "H",
  crossbar of "t"). We confirmed via a full-width solid-line test that the
  pixel mapping itself had zero gaps across the entire display — this
  turned out to just be normal small-bitmap-font rendering at 8px, not a
  remaining hardware bug.

## Summary of what actually mattered

If you take away one thing from this whole process: **when a HUB75 panel
scrambles under every standard configuration a library offers, stop trying
more presets and start doing direct, single-pixel, one-variable-at-a-time
empirical mapping.** It's slower per-round, but unlike guessing at config
values, it's guaranteed to eventually reveal the actual pattern — because
you're reading the hardware's real behavior directly instead of hoping a
formula written for different hardware happens to match yours.
