# NES Core: All Known Issues versus Mesen2

This file catalogs every known discrepancy between ear6 and Mesen2.
Priority: 🔴 CRITICAL (game-breaking) / 🟡 HIGH (visible artifacts) / 🔵 MEDIUM (edge cases) / ⚪ LOW (polish).

## PPU: Cycle-Accurate Rendering (General — 🔴 Root Cause Class)

**Symptom class**: Early frames may appear correct while later frames diverge, often with large full-frame color/layout mismatch.

**Root-cause class**: PPU cycle-level behavior mismatch, not a single register API bug. Typical sources:
1. Tile/attribute/pattern fetch cycle placement (1-256)
2. Sprite evaluation timing (65-256, 257-320)
3. Idle/pre-fetch behavior (337-339)
4. Deferred state application timing (`$2006`, `$2007`, rendering-enable latency)

**Fix requirement**: systematic cycle-by-cycle migration to Mesen2 behavior (see `docs/migration_guide.md` Phase 3-4).

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

### VS-class titles (mapper/PPU-model sensitive)
- [ ] Ensure NES DB/PPU model mapping and raw->RGB path match Mesen2 defaults.
- [ ] Keep cycle-accurate migration tasks in place for timing-sensitive VS titles.

---

## Testing

- [ ] **nestest.nes** — CPU instruction set verification
- [ ] **blargg PPU tests** — palette, sprite, vblank, scroll
- [ ] **blargg APU tests** — length counter, envelope, sweep, DMC
- [ ] **Full compatibility suite** — 100+ popular ROMs
- [ ] **Frame-by-frame trace compare** — script that runs ear6 and Mesen2 side-by-side for the same ROM, finds the first divergent PPU register event
- [ ] **PPU cycle event logging infrastructure** — ✅ DONE. `trace_ppu()` (`src/nes/nes_ppu.cpp`) and `trace_cpu()` (`src/nes/nes_cpu.cpp`) now use two-level gating: compile-time `EAR6_ENABLE_*` macros + runtime `EAR6_TRACE_*` env vars (both required).
- [ ] **Trace/Debug switch reference** — ✅ DONE. Current two-level controls:
  - `EAR6_ENABLE_CPU_TRACE` + `EAR6_TRACE_CPU`
  - `EAR6_ENABLE_PPU_TRACE` + `EAR6_TRACE_PPU`
  - `EAR6_ENABLE_CPU8448_TRACE` + `EAR6_TRACE_CPU8448`
  - `EAR6_ENABLE_MINIMAL_BAD_WINDOW_TRACE` + `EAR6_TRACE_MINIMAL_BAD_WINDOW`
  - `EAR6_ENABLE_CYCLE_ALIGN_TRACE` + `EAR6_TRACE_CYCLE_ALIGN`
  - `EAR6_ENABLE_EARLY_FRAME_SUMMARY_TRACE` + `EAR6_TRACE_EARLY_FRAME_SUMMARY`
  - `EAR6_ENABLE_DMARIO_TRACE11` + `EAR6_TRACE_DMARIO11`
  - `EAR6_ENABLE_PALETTE_TRACE` + `EAR6_TRACE_PALETTE` (optional: `EAR6_TRACE_PALETTE_FRAME`, `EAR6_TRACE_PALETTE_DUMP_PREFIX`)
- [ ] Build reusable first-divergence workflow script: compare raw index, mapped index, and final RGB in separate stages.
