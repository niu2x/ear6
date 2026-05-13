# NES Core: Missing Features versus Mesen2

## Mappers (critical for game compatibility)

- [ ] **MMC1** (mapper 1) — SMB3, Zelda, Metroid, Mega Man 2
- [ ] **UNROM** (mapper 2) — Mega Man, Castlevania, Contra
- [ ] **CNROM** (mapper 3) — Arkanoid, Mappy
- [ ] **MMC3** (mapper 4) — SMB3, Ninja Gaiden, Super C, Megaman 2-6
- [ ] **MMC5** (mapper 5) — Castlevania 3, Just Breed
- [ ] **AxROM** (mapper 7) — Battletoads, Marble Madness
- [ ] **MMC3 variant** — MMC6 (StarTropics), dozens of clone mappers
- [ ] **VRC6** (mapper 24/26) — Castlevania 3 (JP), Wai Wai World 2
- [ ] **VRC7** (mapper 85) — Lagrange Point (extra FM synthesis sound)
- [ ] **FDS** (mapper 0xFFFF) — Disk System games (Zelda, Metroid original)
- [ ] **Namco163** (mapper 19/210) — Splatterhouse, Dragon Spirit
- [ ] **Sunsoft 5B** (mapper 69/68) — Gimmick!, Batman
- [ ] **MMC3 IRQ counter** — scanline-timed IRQ for split-screen scrolling
- [ ] **Bus conflict** — Anrom/GXROM/CNROM bus conflict behavior
- [ ] **CHR-RAM/ROM page switching** — need full 256-entry page table manipulation
- [ ] **SRAM battery backup** — save game support

## APU / Audio

- [ ] **Square 1 & 2** — duty cycle, sweep, envelope, length counter
- [ ] **Triangle** — linear counter, sequence generator, length counter
- [ ] **Noise** — shift register, mode, envelope, length counter
- [ ] **DMC** — delta modulation channel, sample DMA, IRQ
- [ ] **Frame Counter** — 4-step/5-step mode, IRQ, sequencer
- [ ] **Sound Mixer** — channel mixing, DC filter, sample rate conversion
- [ ] **DMC DMA** — DMC reads memory during CPU idle cycles (cycle stealing)
- [ ] **PAL timing** — APU rate changes between NTSC/PAL

## PPU (polish)

- [ ] **PAL / Dendy support** — 310 vs 262 scanlines, master clock divider
- [ ] **Grayscale + intensify bits** — apply to framebuffer output (`_paletteRamMask` / `_intensifyColorBits`)
- [ ] **$2004 (OAMDATA) open bus** — precise read behavior during rendering
- [ ] **OAM decay** — periodic OAM read refresh on PAL, bit decay on NTSC
- [ ] **Sprite overflow bug** — PPU 2C02B early model overflow emulation
- [ ] **Palette read open bus** — `_paletteRamMask & _openBus` high bit merging
- [ ] **`_preventVblFlag` edge case** — precise scanline 241 cycle 0 behavior
- [ ] **First-frame PPU access restriction** — `RestrictPpuAccessOnFirstFrame`
- [ ] **Odd-frame cycle skip** — NTSC-only (currently implemented but untested)

## CPU

- [ ] **CPU/PPU phase randomization** — `RandomizeCpuPpuAlignment` at reset
- [ ] **DMC DMA cycle timing** — DMC read interleaving with instruction cycles
- [ ] **SPR-DMA + DMC-DMA concurrency** — precise timing when both run
- [ ] **Illegal opcode DMA timing** — `SHY`/`SHX`/`SHAA`/`SHAZ`/`TAS` with DMA interruption detection
- [ ] **NMI/IRQ edge detection** — confirm exact φ1/φ2 timing (currently implemented but unverified)

## System / Platform

- [ ] **Game Genie / cheat codes**
- [ ] **NSF player** — NES Sound Format playback
- [ ] **FDS** — Famicom Disk System (extra mapper, disk rotation, audio)
- [ ] **VS System / VS DualSystem** — arcade NES with coin input
- [ ] **Expansion audio** — VRC6/VRC7/Namco163/Sunsoft5B extra channels
- [ ] **HD Pack support** — high-resolution texture replacement packs
- [ ] **NTSC video filter** — blargg NTSC filter, Bisqwit NTSC filter
- [ ] **Overclocking** — `PpuExtraScanlinesBeforeNmi` / `AfterNmi`
- [ ] **Save/Load state** — serialize/deserialize all component state
- [ ] **Debugger** — register view, PPU viewer, trace logger, breakpoints
- [ ] **Event viewer** — PPU cycle event tracing
- [ ] **Input devices** — Zapper, Four Score, Arkanoid controller, Power Pad, etc.

## Testing

- [ ] **nestest.nes** — CPU instruction set verification
- [ ] **blargg PPU tests** — palette, sprite, vblank, scroll tests
- [ ] **blargg APU tests** — length counter, envelope, sweep, DMC
- [ ] **Full compatibility suite** — smoke test across 100+ popular ROMs
