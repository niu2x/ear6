# PPU Rendering Bug Investigation

## Current Status

PPU rendering now WORKS: tiles are read, pattern data is decoded, bg_color is non-zero for visible tiles. The output at 300+ frames shows recognizable patterns (tile data rendered at correct positions). However, only 2 colors appear: gray (backdrop, `palette_ram_[0]=0x00` → `mesen_palette[0]=0xFF666666`) and black (`palette_ram_[x]=0x0F` → `mesen_palette[0x0F]=0xFF000000`).

The root remaining problem is the **palette is never fully written** by the CPU.

---

## Fixed Bugs

### Bug #1: Cycle 257 horizontal scroll copy (CRITICAL)

**File**: `src/nes/nes_ppu.cpp:220`

**What**: The PPU's cycle-257 operation copies scroll bits from t_ (temporary) to v_ (active) register. The masks were WRONG — it copied vertical bits (fine Y, coarse Y, nametable Y) from t_ when it should copy horizontal bits (coarse X, nametable X).

**Before**:
```cpp
v_ = (v_ & 0x841F) | (t_ & 0x7BE0);  // WRONG: preserves horizontal from v_, copies vertical from t_
```

**After** (matches Mesen2):
```cpp
v_ = (v_ & 0x7BE0) | (t_ & 0x041F);  // CORRECT: preserves vertical from v_, copies horizontal from t_
```

**Effect**: Without this fix, nametable X was never properly reset between scanlines. By scanline 1+, the wrong nametable was being read, causing all tile indices to come from the wrong screen, resulting in a black screen.

### Bug #2: Transparent pixel palette handling

**File**: `src/nes/nes_ppu.cpp:85-89`

**What**: When a background pixel has pattern data = 0 (transparent), `bg_color` was incorrectly set to `palette_offset | 0 = palette_offset` (non-zero for non-zero attribute palettes). This caused the pixel to use a subpalette color instead of the universal backdrop color.

**Before**:
```cpp
bg_color = (palette << 2) | (hi_bit << 1) | lo_bit;
```

**After** (matches Mesen2's `color & 0x03` check in `DefaultNesPpu::DrawPixel`):
```cpp
uint8_t pixel = (hi_bit << 1) | lo_bit;
if (pixel) {
    uint8_t palette = (bg_attr_hi_ << 1) | bg_attr_lo_;
    bg_color = (palette << 2) | pixel;
}
```

---

## Remaining Issue: Palette Not Written

### Symptom

- Only `palette_ram_[0]` is ever written (4 times with value `0x0F`)
- All remaining palette entries stay at their reset value (0x00)
- Output shows only gray (backdrop) and black (palette index `0x0F`)
- No blue sky, no green, no red/brown — just gray and black

### Root Cause

The game writes the palette through `$2007` with `ctrl_` bit 2 set (vertical increment = +32). With inc=32 starting from `v=$3F10`:

```
$3F10 → addr &= 0x1F = 0x10 → mirror to 0x00 → palette_ram_[0]
$3F30 → addr &= 0x1F = 0x10 → mirror to 0x00 → palette_ram_[0]  (overwrites)
$3F50 → addr &= 0x1F = 0x10 → mirror to 0x00 → palette_ram_[0]  (overwrites)
$3F70 → addr &= 0x1F = 0x10 → mirror to 0x00 → palette_ram_[0]  (overwrites)
```

All 4 writes hit `palette_ram_[0]`. The remaining palette entries (1-31) are never written.

### Hypothesis

The game's NMI handler should write the full palette with inc=1 ($3F00+ with ctrl bit 2 = 0). But either:

1. **The NMI handler code path doesn't execute** — maybe a conditional branch taken incorrectly due to CPU emulation bug
2. **The VBlank flag / NMI timing is wrong** — the game's palette write code uses `$2002` read to check for VBlank, and the status register might not return correct values
3. **The PPUADDR write sequence for palette ($3F00) is broken** — the game sets PPUADDR to $3F00 but writes go elsewhere

### Investigation Data (from debug logging)

- Only 4 PAL_SETDATA calls across 60 frames, all at `v=0x3F10 inc=32 value=0F`
- 25M+ SETDATA calls total (nametable writes via PPUDATA), all with inc=1
- NMI handler runs at PC=4140 (65k hits)
- Main wait loop at PC=8057 (84k hits)
- Game fills nametable 1 ($2400) with tile 0x24 correctly
- PPUCTRL is set to $90 (bit 2 = 1, vertical increment) before palette writes

### Next Steps

1. **Trace the NMI handler code** — verify the NMI handler's PPUADDR/PPUDATA writes for palette setup
2. **Check $2002 reads** — verify the game reads the VBlank flag correctly
3. **Verify PPUADDR write sequence** — game writes `$3F` then `$00` to $2006 — check if `v_` ends up at $3F00
