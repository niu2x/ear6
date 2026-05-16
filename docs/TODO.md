# NES Core: All Known Issues versus Mesen2

This file catalogs every known discrepancy between ear6 and Mesen2.
Priority: 🔴 CRITICAL (game-breaking) / 🟡 HIGH (visible artifacts) / 🔵 MEDIUM (edge cases) / ⚪ LOW (polish).

## PPU: Cycle-Accurate Rendering (vs dr mario — 🔴 Root Cause)

**Symptom**: `vs dr mario.nes` (mapper 1, MMC1, iNES byte 7 bit 0 = VS flag) shows 0.00% pixel match at frame 60. Frame 5 matches Mesen2 identically. Colors wrong because game code diverges after ~5 frames.

**Debug history (2026-05-16)**:

- **Step 1 — Frame 5 matches perfectly**: Both emulators produce identical pixel output at frame 5 (confirmed via `cmp`).
- **Step 2 — Mesen2 made deterministic**: Changed `RamPowerOnState` from `Random` to `AllZeros` in `SettingTypes.h:659`.
- **Step 3 — Trace diff found ~18 PPU cycle offset at frame 4**:
  ```
  ear6:   W$2001=00 at sl=259 cyc=57
  Mesen2: W$2001=00 at sl=259 cyc=39
  ```
  Offset = +18 cycles (ear6 ahead), CONSTANT for all subsequent events (not growing scanline-to-scanline).
- **Step 4 — Offset origin**: Between `W$2005=00` (sl=249 cyc=173, SAME in both) and `W$2001=00` (sl=259 cyc=57/39), there are 10 scanlines of pure PPU internal work (no CPU register accesses). The 18-cycle gap accumulates ENTIRELY inside the PPU rendering pipeline.
- **Step 5 — Offset tracked scanline-by-scanline**: Across 76 scanlines (frame 3 scanlines 184-259), ear6 averages ~0.24 extra PPU cycles per scanline vs Mesen2. The 18-cycle offset is constant once established — it does NOT grow further until frame 9.
- **Step 6 — NMI pattern diverges at frame 9**: +3 more cycles accumulate between frames 7→9, shifting the `$2002` read timing → `prevent_vbl_flag_` set differently → NMI fires on an extra frame in ear6 (frame 10 vs Mesen2 frame 11). After this, all subsequent events are on different frame numbers.
- **Step 7 — Trace infrastructure pitfall fixed**: Direct per-cycle `printf/fprintf` in Mesen2 `EndCpuCycle()` caused severe timing perturbation (vs dr mario rendered all-black). Resolved by buffering CPU logs (ring buffer), logging hot-path transitions only, and using `PRIu64` for portable 64-bit formatting.
- **Palette never written**: Game code takes different branch → never executes palette initialization code → boot palette persists. This is a **symptom**, not root cause.

**Root cause**: PPU rendering pipeline (`process_scanline_impl()`) has micro-timing differences vs Mesen2's `ProcessScanlineImpl()`:
1. VRAM read pattern during tile/attribute/pattern fetches (cycles 1-256 per scanline)
2. Sprite evaluation cycle timing (cycles 65-256, 257-320)
3. Idle reads (cycles 337-339)
4. The `process_sprite_evaluation()`, `load_tile_info()`, `shift_tile_registers()` functions differ from Mesen2's equivalents in exactly which cycles they perform each operation

**Fix requirement**: Systematic cycle-by-cycle migration of `process_scanline_impl()` to match Mesen2's `ProcessScanlineImpl()`:
- Phase A: Align VRAM read pattern (exact cycles for NT/AT/PT fetches)
- Phase B: Align sprite evaluation timing (split into per-cycle steps)
- Phase C: Align scroll increment timing (inc_horizontal at cycle 0 mod 8 exactly)
- Phase D: Align idle cycle behavior (cycles 337-339, pre-fetch, dummy reads)
- See `docs/migration_guide.md` Phase 3-4.

**Note**: This is NOT fixable by any single register-level TODO item. It requires systematic pipeline migration.

---

## PPU Core Features (migration_guide.md §11)

### 🔴 Critical (game-breaking without these)

- [ ] **`$2006` 3-cycle delay** — VRAM address update deferred by 3 PPU cycles (`UpdateState`)
- [ ] **PPUADDR scroll glitch** — `$2006` write during rendering corrupts `v` register depending on cycle (257=full AND, cycle%8=0=partial AND, else=normal copy)
- [ ] **`$2007` write masking during visible scanlines** — VRAM writes ignored during 0-239 when rendering on; writes LSB instead
- [ ] **Rendering enable 1-cycle delay** — `prev_rendering_enabled` vs `rendering_enabled` has 1 PPU cycle latency
- [ ] **`$2007` increment delay** — VRAM address auto-increment deferred (`_needVideoRamIncrement`)

### 🟡 High (affects compatibility)

- [ ] **Sprite evaluation cycle-exact** — Mesen2 evaluates sprites across cycles 65-256, not in a single call
- [ ] **OAM address reset** — `_spriteRamAddr = 0` during cycles 257-320
- [ ] **OAM corruption** — PPU hardware bug corrupts OAM when rendering is toggled mid-frame
- [ ] **OAM decay** — periodic OAM bit decay on NTSC, refresh on PAL
- [ ] **Pre-render sprite dummy $FF** — pre-render line fetches dummy tiles (fixes Ninja Gaiden 3)
- [ ] **Odd-frame cycle skip (NTSC)** — PPU skips cycle 339 on odd frames for Ppu2C02 only
- [ ] **NMI `_preventVblFlag` edge case** — reading `$2002` at scanline 241 cycle 0 prevents next VBL
- [ ] **First-frame PPU access restriction** — `RestrictPpuAccessOnFirstFrame` for `$2000`-`$2007` writes
- [ ] **Grayscale + intensify bits** — must apply to framebuffer output, not just `$2007` reads
- [ ] **`_paletteRamMask` & `_intensifyColorBits`** — emphasis encoding not written to framebuffer (missing 3 bits in pixel output)
- [ ] **Open bus decay** — PPU open bus bits decay over time (set per register on write)

### 🔵 Medium

- [ ] **PAL / Dendy support** — 310 vs 262 scanlines, different master clock divider, OAM refresh behavior
- [ ] **`$2004` open bus read** — precise read behavior during rendering (returns OAM copy buffer)
- [ ] **Palette read open bus** — `palette_ram_mask & open_bus` high bit merging on `$2007` reads
- [ ] **Memory read buffer corruption on palette reads** — `memory_read_buffer_` updated from mapper for $3F00+ addresses, should NOT be updated (internal PPU RAM)

---

## PPU: Additional Implementation Gaps

- [ ] **`process_scanline_impl()` not called for scanline 240+** — Mesen2 calls `ProcessScanlineImpl()` for ALL scanlines; ear6 skips for `scanline >= 240`. This affects internal state transitions during VBlank (e.g., sprite evaluation PAL behavior).
- [ ] **CPU/PPU master clock alignment** — `ppu_offset_` may need adjustment vs Mesen2's `_ppuOffset` to match exact dot timing
- [ ] **`update_state()` called conditionally** — ear6 calls `update_state()` only when `need_state_update_`; Mesen2 calls `UpdateState()` every `exec()`. Missing updates for rendering state transitions that don't set `need_state_update_`.

---

## Mappers

- [ ] **MMC1** (mapper 1) — SMB3, Zelda, Metroid, Mega Man 2 (mostly working, needs cycle-level verification vs batch-model artifacts)
- [ ] **UNROM** (mapper 2) — Mega Man, Castlevania, Contra
- [ ] **CNROM** (mapper 3) — Arkanoid, Mappy
- [ ] **MMC3** (mapper 4) — SMB3, Ninja Gaiden, Super C, Megaman 2-6
- [ ] **MMC5** (mapper 5) — Castlevania 3, Just Breed
- [ ] **AxROM** (mapper 7) — Battletoads, Marble Madness
- [ ] **MMC3 variant** — MMC6 (StarTropics), clone mappers
- [ ] **VRC6** (mapper 24/26) — Castlevania 3 (JP), Wai Wai World 2
- [ ] **VRC7** (mapper 85) — Lagrange Point (FM synthesis)
- [ ] **FDS** (mapper 0xFFFF) — Disk System (Zelda, Metroid JP)
- [ ] **Namco163** (mapper 19/210) — Splatterhouse, Dragon Spirit
- [ ] **Sunsoft 5B** (mapper 69/68) — Gimmick!, Batman
- [ ] **MMC3 IRQ counter** — scanline IRQ for split-screen
- [ ] **Bus conflict** — ANROM/GXROM/CNROM bus conflict behavior
- [ ] **CHR-RAM/ROM page switching** — needs full 256-entry page table
- [ ] **SRAM battery backup** — save game support
- [ ] **`RomInfo` lacks `SubMapperID`** — required for mapper 002 submapper 2 bus conflict detection. Add field, parse from iNES 2.0 byte 15.
- [ ] **MMC1 write timing guard** — `>= 2` guard on CPU cycles (already fixed per AGENTS.md) but may need re-verification with cycle-accurate PPU

---

## APU / Audio

- [ ] **Square 1 & 2** — duty cycle, sweep, envelope, length counter
- [ ] **Triangle** — linear counter, sequence generator, length counter
- [ ] **Noise** — shift register, mode, envelope, length counter
- [ ] **DMC** — delta modulation channel, sample DMA, IRQ
- [ ] **Frame Counter** — 4-step/5-step mode, IRQ, sequencer
- [ ] **Sound Mixer** — channel mixing, DC filter, sample rate conversion
- [ ] **DMC DMA** — cycle stealing during CPU idle
- [ ] **PAL timing** — APU rate changes NTSC/PAL
- [ ] **$4017 write = APU frame counter reset** — current NesPpu::write_ram 0x07 check only covers PPU registers; $4017 is also for APU frame counter

---

## CPU

- [ ] **CPU/PPU phase randomization** — `RandomizeCpuPpuAlignment` at reset
- [ ] **DMC DMA cycle timing** — DMC read interleaving with instruction cycles
- [ ] **SPR-DMA + DMC-DMA concurrency** — precise timing when both run
- [ ] **Illegal opcode DMA timing** — SHY/SHX/SHAA/SHAZ/TAS with DMA interruption
- [ ] **NMI/IRQ edge detection** — confirm exact φ1/φ2 timing

---

## System / Platform

- [ ] **Game Genie / cheat codes**
- [ ] **NSF player** — NES Sound Format playback
- [ ] **FDS** — Famicom Disk System (extra mapper, disk rotation, audio)
- [ ] **VS System / VS DualSystem** — arcade NES with coin input
- [ ] **Expansion audio** — VRC6/VRC7/Namco163/Sunsoft5B
- [ ] **HD Pack support** — high-resolution texture packs
- [ ] **NTSC video filter** — blargg NTSC, Bisqwit NTSC
- [ ] **Overclocking** — `PpuExtraScanlinesBeforeNmi` / `AfterNmi`
- [ ] **Save/Load state** — serialize/deserialize component state
- [ ] **Debugger** — register view, PPU viewer, trace logger
- [ ] **Input devices** — Zapper, Four Score, Arkanoid controller, Power Pad

---

## ROM-Specific Regressions

### FDS BIOS (NROM mapper 0)
- [ ] Missing FDS hardware stub → wrong palette values. Reads `$4024-$403F` return open bus (0x00) → BIOS takes wrong code path.
- **Fix**: Minimal FDS stub returning sane values, `$6000-$7FFF` WRAM, detect by submapper or hash.

### Duck Hunt (NROM mapper 0)
- [ ] Missing Zapper emulation → "Connect Zapper" screen instead of game/title. Port 2 `$4017` returns 0x40.
- **Fix**: Implement Zapper on port 2, or CLI option for port 2 device type.

### vs dr mario (MMC1 mapper 1)
- [ ] **Root cause**: PPU cycle-level timing divergence (see top of this file).
- **Fix**: Cycle-accurate PPU migration (Phase 3-4).

---

## Testing

- [ ] **nestest.nes** — CPU instruction set verification
- [ ] **blargg PPU tests** — palette, sprite, vblank, scroll
- [ ] **blargg APU tests** — length counter, envelope, sweep, DMC
- [ ] **Full compatibility suite** — 100+ popular ROMs
- [ ] **Frame-by-frame trace compare** — script that runs ear6 and Mesen2 side-by-side for the same ROM, finds the first divergent PPU register event
- [ ] **PPU cycle event logging infrastructure** — ✅ DONE. `trace_ppu()` in `nes_ppu.cpp` and `trace_cpu()` in `nes_cpu.cpp`, both gated by `#define ENABLE_PPU_TRACE` / `#define ENABLE_CPU_TRACE`.
- [ ] **vs dr mario: find the 6 extra CPU cycles** — see `docs/debug_vs_dr_mario.md` for full research doc, trace infra, and step-by-step debugging guide.
