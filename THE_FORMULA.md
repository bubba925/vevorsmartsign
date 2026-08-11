# The Pixel-Mapping Formula

## Background: how a "normal" four-scan HUB75 hack works

Most HUB75 driver libraries (including the one this project builds on) assume
four-scan panels can be driven with a documented trick: configure the base
driver with **double the panel width and half the panel height**, then use a
coordinate-remapping function to translate real (x, y) coordinates into that
doubled/halved electrical space. The extra width gives you a spare "half" of
each electrical row to stash a 4th physical row's worth of data in.

This is the trick used by the reference library's own `VirtualMatrixPanel`
examples, and it's what we tried first. **It didn't work on this panel.**

## What we actually found

After extensive testing (see the
[troubleshooting journey](TROUBLESHOOTING_JOURNEY.md) for the full process),
we determined:

1. The panel's R1 and G1/B1 data lines work correctly, and independently
   address 8 rows each — R1 covers rows 0-7, R2 covers rows 8-15 (a totally
   standard split, once you get the *right* configuration).
2. The panel's C and D address lines are **not connected to anything** —
   swapping them with A, B, or each other changes nothing.
3. And yet — a 3rd address bit's worth of information **is** getting through
   to the panel somehow, because rows within the same 0-7 (or 8-15) half
   *can* be individually addressed. We just needed to find how.

The breakthrough came from directly sweeping the electrical column value
while holding the row fixed, and recording exactly which physical row lit up:

```
Row under test: TRUE_y = 4  (normally aliases with TRUE_y = 0)

Column sent | Physical row that lit up
------------|--------------------------
     4      | row 1  (wrong - this is TRUE_y=0's row)
    12      | row 5  (correct! - this is TRUE_y=4's own row)
    20      | row 1  (wrong again)
    28      | row 5  (correct again)
```

The pattern repeats every 16 columns, and it's controlled by exactly one
bit: **bit 3 of the column address** (value 8). When that bit is 0, you
reach one row-group; when it's 1, you reach the other. This is a hidden
row-select bit smuggled inside the column address, precisely compensating
for the two dead address lines.

## The formula

```cpp
// coords.x = true column (0-95, spans the full 3-panel chain)
// coords.y = true row (0-15)
int v = coords.x;
int g = (coords.y >> 2) & 1;              // the "hidden" row-select bit
coords.x = (v & 7) | (g << 3) | ((v >> 3) << 4);
// coords.y is passed straight through, unchanged
```

### Reading it piece by piece

- **`g = (coords.y >> 2) & 1`** — bit 2 of the row number. For rows 0-3 and
  8-11, this is 0. For rows 4-7 and 12-15, this is 1. This is the bit that
  the (non-functional) C address line *would* have carried.
- **`v & 7`** — the low 3 bits of the true column pass straight through
  unchanged.
- **`g << 3`** — the hidden row-select bit gets inserted at bit position 3
  of the electrical column.
- **`(v >> 3) << 4`** — everything *above* bit 3 in the true column shifts
  up by one bit position, to make room for the inserted bit at position 3.

In effect: take the true column's binary representation, and **insert** an
extra bit (carrying row information) right after bit 2, shifting everything
above it up by one place. This is why the electrical column space needs to
be *double* the true width — every real column needs 2 electrical slots,
one for each possible value of the hidden bit, even though only one of them
is ever "correct" for a given row.

### Why the row passes through unchanged

Because this hidden-bit trick handles the *within-half* addressing (0-3 vs
4-7, or 8-11 vs 12-15), the top-level R1/R2 split (rows 0-7 vs 8-15) can be
left entirely to the base driver's own standard logic — no remapping needed
there. This is also why the correct config uses **full height** (not the
halved height the "textbook" four-scan hack calls for) alongside the
doubled width.

## Final driver configuration

```cpp
HUB75_I2S_CFG mxconfig(
    panel_width * 2,   // doubled width - matches the electrical space the
                        // formula above needs
    panel_height,       // FULL height, NOT halved
    chain_length,
    pins
);
mxconfig.driver = HUB75_I2S_CFG::FM6124;
mxconfig.latch_blanking = 4;   // fixes a separate column-ghosting artifact
                                // this panel produces at the default value
```


## Important: this is almost certainly panel-specific

We found a public forum thread (linked in the main README) confirming that
other outdoor P10 quarter-scan panels use a **"snake pattern"** internal
wiring that varies between manufacturers/batches, requiring individually
reverse-engineered lookup tables. Our formula happens to reduce cleanly to a
bit-manipulation formula rather than needing a full lookup table, which was
a fortunate outcome — don't assume a different four-scan panel will yield
the same clean pattern. If you're adapting this for your own hardware, treat
this formula as a *starting hypothesis* to test against, not a guaranteed fix.
