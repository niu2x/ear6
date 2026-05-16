# NES Core: All Known Issues versus Mesen2

This file catalogs every known discrepancy between ear6 and Mesen2.
Priority: 🔴 CRITICAL (game-breaking) / 🟡 HIGH (visible artifacts) / 🔵 MEDIUM (edge cases) / ⚪ LOW (polish).

## PPU: Cycle-Accurate Rendering (vs dr mario — 🔴 Root Cause)

**Symptom**: `vs dr mario.nes` (mapper 1, MMC1, iNES byte 7 bit 0 = VS flag) shows 0.00% pixel match at frame 60. Frame 5 matches Mesen2 identically. Colors wrong because game code diverges after ~5 frames.

**Debug history (2026-05-16)**:

- **Frame 5 matches perfectly** — all PPU register events identical between ear6 and Mesen2 for first 5 frames.
- **Frame 4 VBlank first divergence**: `W$2000=10` at ear6 scanline 259 vs Mesen2 scanline 260 (1 scanline early).
- **~393 PPU cycle gap** in frame 4 VBlank (DMA + post-DMA code takes fewer cycles in ear6).
- **Accumulation**: By frame 7, `W$2001=1E` at ear6 sl=257 vs Mesen2 sl=258. By frame 11/12, difference reaches 1 full frame.
- **Palette never written**: Game code takes different branch → never executes palette initialization code → boot palette persists. This is a **symptom**, not root cause.

**Root cause**: PPU internal rendering pipeline (tile shifters, attribute latches, sprite evaluation, DMA timing) has accumulated cycle-level differences from the Mesen2 reference. Despite ear6 already using per-cycle PPU stepping via `start_cpu_cycle/end_cpu_cycle`, the `exec()`, `process_scanline_impl()`, `process_sprite_evaluation()`, and `process_pending_dma()` implementations have subtle timing differences vs Mesen2's cycle-exact versions.

**Fix**: Not a single register bug. Requires systematic migration of all PPU internal pipeline functions to match Mesen2's cycle-level behavior. See `docs/migration_guide.md` Phase 3-4.

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
- [ ] **PPU cycle event logging infrastructure** — reusable trace macros in both emulators for diff-based debugging
