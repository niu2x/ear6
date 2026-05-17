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

## PPU Core Features (Remaining)

### 🔴 Critical (still open)

- [ ] **Cycle-accuracy drift in timing-sensitive titles** — architecture-level residual mismatch still appears in VS-class titles; keep migration/trace workflow focused on first-divergence at register/cycle level

### 🟡 High (affects compatibility)

- [ ] **First-frame PPU access restriction parity** — verify/port `RestrictPpuAccessOnFirstFrame` behavior for `$2000-$2007`
- [ ] **Grayscale + intensify bits output parity** — verify full emphasis behavior in final framebuffer path vs Mesen2
- [ ] **Open bus decay parity** — complete/verify decay semantics across PPU register interactions

### 🔵 Medium

- [ ] **`$2004` open bus read** — precise read behavior during rendering (returns OAM copy buffer)
- [ ] **Palette read open bus** — `palette_ram_mask & open_bus` high bit merging on `$2007` reads
- [ ] **Memory read buffer corruption on palette reads** — `memory_read_buffer_` updated from mapper for $3F00+ addresses, should NOT be updated (internal PPU RAM)

---

## PPU: Additional Implementation Gaps

- [ ] **CPU/PPU master clock alignment** — `ppu_offset_` may still need adjustment vs Mesen2 `_ppuOffset` for exact dot timing
- [ ] **PAL / Dendy parity** — scanline count and timing behavior still need validation against reference

---

## Mappers

- [ ] **MMC1** (mapper 1) — SMB3, Zelda, Metroid, Mega Man 2 (mostly working, needs more cycle-level verification on edge timing)
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
- [ ] **$4017 write = APU frame counter reset** — verify frame-counter behavior/timing parity against Mesen2

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
- [ ] **VS System / VS DualSystem** — arcade NES with coin input/device model coverage
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
- [ ] Build reusable first-divergence workflow script: compare raw index, mapped index, and final RGB in separate stages.
