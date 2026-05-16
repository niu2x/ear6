# Ear6 - Game Emulator Library

## Build

```bash
make ear6          # native: libear6.so + ear6-desktop
make ear6-web      # wasm: libear6.a + ear6-web.js (requires Emscripten in .env)
make clean

# Mesen2 reference (for trace comparison):
make cli -C ../mesen2/DesktopApp   # ONLY valid way to build mesen2-cli
```

## Lint / Typecheck

```bash
cmake --build build --target ear6   # rebuild catches compile errors
```

## Naming

- **Classes / type aliases**: PascalCase (`NesCpu`, `MapperType`)
- **Enum constants**: UPPER_SNAKE_CASE (`DISCONNECTED`, `HANDSHAKING`, `CHOKE`, `BITFIELD`)
- **Constants (`constexpr` / `const` at file or namespace scope)**: UPPER_SNAKE_CASE (`DEFAULT_NES_PALETTE`)
- **Functions / variables**: snake_case (`parse_value`, `piece_length`)
- **Non-public members**: snake_case + trailing underscore (`data_`, `pos_`)
- **Getters**: MUST use `get_xxx()` prefix — no bare noun or property name. Example: `get_framebuffer()`, `get_frame_width()`, `get_state()`, `get_peer_info()`. Wrong: `framebuffer()`, `width()`, `state()`.
- **Boolean getters**: MUST use `is_xxx()` prefix. Example: `is_connected()`, `is_ready()`. Wrong: `connected()`, `ready()`.
- **Setters**: MUST use `set_xxx()` prefix. Example: `set_state()`, `set_callback()`. Wrong: `state()`, `callback()`.

## Project Structure

- `include/ear6/` — public API headers (installed)
- `src/` — library implementation (private)
- `app/desktop/` — Linux/macOS desktop app
- `app/web/` — HTML5/WASM app (Emscripten)
- `cmake/` — CMake modules and package config

## NES Core (Mesen2 Migration Complete)

The NES implementation has been migrated from Mesen2 with all critical bugs fixed:

- **Cycle-interleaved model**: PPU advances per CPU memory access via `StartCpuCycle`/`EndCpuCycle`
- **`$2006` 3-cycle delay**: VRAM address update deferred by 3 PPU cycles with scroll glitch
- **`$2007` write masking**: VRAM writes ignored during visible scanlines when rendering is on
- **Palette mirroring**: Bidirectional `$00/$10`, `$04/$14`, `$08/$18`, `$0C/$1C` pairs
- **Transparent pixel backdrop**: pixel=0 always reads `palette_ram[0]`
- **Mapper page tables**: 256-entry `chr_pages_[]` / `prg_pages_[]` 
- **CPU dispatch table**: `NesMemoryManager` with 65536-entry handler tables
- **Sprite evaluation**: Cycle-accurate OAM evaluation with overflow/sprite-0-hit

### Architecture

```
NesConsole
├── NesMemoryManager        — 65536-entry dispatch table (O(1) address routing)
├── NesCpu                  — 6502 CPU with cycle-interleaved PPU clocking
├── NesPpu                  — Cycle-accurate PPU (Mesen2 Exec/ProcessScanlineImpl/UpdateState)
├── NesApu                  — APU (basic)
├── NesControlManager       — Controller input ($4016/$4017)
└── BaseMapper              — 256-entry PRG/CHR page tables
    └── Mapper000 (NROM)    — NROM mapper (more to come)
```

## Critical Bugs Fixed & Lessons Learned

### Bug 1: CPU `fetch_operand()` double-fetch (Wrong PC)

**Symptom**: Every instruction that used `fetch_operand()` in its implementation body would re-fetch operands, advancing PC twice. Branches jumped to wrong addresses.

**Root cause**: `exec()` already calls `fetch_operand()` and stores result in `operand_`. But opcode implementations like `op_sta()`, `op_stx()`, `inc_op()`, `dec_op()`, `asl_addr()`, `branch_relative()` called `fetch_operand()` a second time. In Mesen2 these use `GetOperand()` which returns `_operand`.

**Fix**: All instruction bodies must use `operand_` (the pre-fetched value), NEVER call `fetch_operand()` again.

### Bug 2: `set_ppu_memory_mapping`/`set_cpu_memory_mapping` overload ambiguity

**Symptom**: Nametable writes silently failed on Clang (macOS). Background tiles were all 0.

**Root cause**: Two overloads — `(uint16_t, uint16_t, uint16_t page_number, ChrMemoryType, int8_t)` and `(uint16_t, uint16_t, uint8_t* source, uint32_t, uint32_t, int8_t)` — have 5 vs 6 params. With `enum class ChrMemoryType`, `int`→`scoped enum` is disallowed by the standard, so GCC correctly picks the pointer version. However, **Clang may differ** — when calling with 5 args and a pointer at arg3, Clang's overload resolution can unexpectedly pick the page-number version, reinterpreting the pointer as a `uint16_t` page number and `NAMETABLE_SIZE` (0x400) as `access_type` → `NoAccess`.

**Fix**: Pointer-based overloads renamed to `set_ppu_memory_mapping_ptr` / `set_cpu_memory_mapping_ptr` to eliminate ambiguity. **Never overload a function with `(ptr, int, int)` vs `(int, int, int)` — C++ may silently pick the wrong one, especially on Clang.**

### Bug 3: NROM PRG mapping (select_prg_page_4x too coarse)

**Symptom**: 32KB PRG mapped incorrectly; CPU ran from open bus.

**Root cause**: `select_prg_page_4x()` uses slots based on `get_prg_page_size()`. For 16KB page size, slot arithmetic wraps past 0xFFFF, corrupting the page table.

**Fix**: NROM maps directly via `set_cpu_memory_mapping(0x8000, 0xBFFF, 0)` and `(0xC000, 0xFFFF, 1)` — no multi-slot helpers for 16KB pages.

### Bug 4: Include ordering + namespace leak

**Symptom**: C++ standard library headers pulled into `ear6::nes::std` namespace, causing compilation errors.

**Root cause**: When a `.h` file opens `namespace ear6::nes {`, subsequent `#include` directives in the `.cpp` are inside the namespace. Headers that include `<memory>`, `<vector>` etc. before their own namespace block will have those system includes nested.

**Fix**: In `.cpp` files, always include ALL headers that have system includes (before their namespace) FIRST, then include any header that opens a namespace LAST. Better yet: NEVER include system headers from within an existing namespace.

## Public API Contract

All functions declared in `<ear6/ear6.h>` must be **system-agnostic** — their behavior and return values must be semantically consistent across every `Ear6SystemType` (NES, Test, Flash).

| API | 保证 |
|-----|------|
| `ear6_get_framebuffer()` | 返回 RGBA8888，每像素 4 字节，R/G/B/A 各 8 bit |
| `ear6_get_frame_width()` | 返回有效宽度 |
| `ear6_get_frame_height()` | 返回有效高度 |
| `ear6_get_audiobuffer()` | 返回 16-bit PCM 样本，无音频时返回 nullptr |
| `ear6_get_audio_num_samples()` | 返回样本数，无音频时返回 0 |
| `ear6_step()` | 推进一帧，返回 0 表示成功 |
| `ear6_load()` | 加载 ROM/Swap 数据，空数据可用于初始化 |

系统特有的行为（如 NES 调色板配置、Flash 版本设置）必须放在对应的系统专用头文件（`nes.h`、`flash.h`）中，不得在 `ear6.h` 中添加系统相关的 API。

## Known Limitations

### FDS BIOS ROM renders wrong colors (missing FDS hardware)

ROM: `Famicom Disk System BIOS ROM (J).nes` (mapper 0, NROM)

**Symptom**: Screenshot comparison at frame 60 shows ~58% pixel match with Mesen2. The stripe pattern positions match exactly, but colors use wrong palette indices (offset of +9: ear6 uses palette indices 0x01/0x21 vs Mesen2's 0x0A/0x2A).

**Root cause**: The ROM is an NROM cartridge containing the FDS BIOS. It reads FDS I/O registers at `$4024-$403F` to detect disk hardware. ear6 has no FDS implementation, so reads return open bus (0x00). The BIOS takes a different code path and writes different values to PPU palette RAM (`$3F00-$3F1F`). The NES palette lookup table itself is identical between ear6 and Mesen2.

**Fix when adding FDS support**: Implement minimal FDS stub for this ROM:
1. `$4024-$403F` I/O registers returning sane stub values
2. `$6000-$7FFF` 8KB WRAM mapping (FDS RAM Adapter)
3. Detect this ROM either by iNES 2.0 submapper or by hash

The lower ~90% of the frame is all-black in both emulators (false match in comparison). See `docs/TODO.md` for FDS implementation status.

### Duck Hunt shows "Connect Zapper" screen (missing Zapper/light gun emulation)

ROM: `Duck Hunt (JUE).nes` (mapper 0, NROM)

**Symptom**: Screenshot comparison shows ~71.77% pixel match with Mesen2 at frames 20-60. Top 128 rows (sky/grass background) match perfectly, but rows 128+ differ completely — ear6 shows a white-background "Connect Zapper" screen while mesen2 shows black/dark content.

**Root cause**: Duck Hunt reads controller port 2 (`$4017`) at startup to detect the NES Zapper (light gun). ear6 has no Zapper emulation, so `$4017` reads return `0x40` (standard controller, no buttons pressed), causing the game to display the "Connect Zapper" instruction screen. Mesen2's internal default for port 2 apparently returns different data, making the game detect a Zapper and proceed to the game/title screen.

**Impact**: Only affects games that read port 2 for Zapper detection. PPU and standard controller rendering are correct (confirmed by 128 perfect rows).

**Fix when adding Zapper support**: Implement Zapper emulation on controller port 2, or provide a CLI option to set port 2 device type.

### vs dr mario shows 0.00% match (PPU batch model vs cycle-interleaved)

ROM: `vs dr mario.nes` (mapper 1, MMC1, iNES byte 7 bit 0 = VS flag set)

**Symptom**: 0.00% pixel match at all frame counts (60, 120, 256). ear6 shows a repeating checkerboard pattern (Dr Mario pill tile from CHR bank 2) while mesen2 shows proper game content.

**Investigation history (2026-05-16):**

1. ❌ **VS System detection**: Initially blamed byte 7 bit 0 → created VsControlManager, 2C03 palette, odd-frame skip disable. Partially correct (mesen2 DOES use 2C03 palette for this ROM) but controller manager was wrong — mesen2 detects VS from filename `(vs)` not from header bit, so it uses standard NesControlManager.

2. ✅ **Resolved**: Split `is_vs_system` (controls VsControlManager creation) from `use_vs_palette` (controls 2C03 palette). Only the latter is set from header byte 7 bit 0.

3. ❌ **DIP switch / $4017 reads**: ear6 used VsControlManager which returns `0x00` for `$4017`; mesen2 uses standard NesControlManager returning `0x40`. The difference caused game code to branch differently.

4. ✅ **Resolved**: `is_vs_system` is never set from header (only from CLI `--system vs`), so standard NesControlManager is always used.

5. ❌ **MMC1 init differences**: ear6 set member variables directly; mesen2 used `ProcessRegisterWrite(0x8000, GetPowerOnByte() | 0x0C)` setting ScreenAOnly mirroring instead of header-specified Horizontal.

6. ✅ **Resolved**: MMC1 init rewritten to match mesen2's InitMapper.

7. ❌ **Reset order**: ear6 called CPU reset (8 dummy cycles advancing PPU) then PPU reset (wasting 24 PPU cycles). mesen2 calls PPU reset first, then CPU reset.

8. ✅ **Resolved**: Reset order changed to PPU→CPU matching mesen2.

9. ❌ **VRAM/palette data**: First 5468 PPU trace events are identical. Divergence at trace index 5505 — `$2006` second byte writes `$AA` vs `$CB` (33 bytes = 1 scanline + 1 pixel scroll offset) during nametable address setup after palette update.

10. ❌ **NMI timing**: Both emulators fire NMI at frames 4, 6, 8, 10, 12, 14 (every other frame). Not a NMI issue.

11. ❌ **$2007 write masking**: Already implements the "write LSB during rendering" behavior from mesen2. Not the issue.

12. ❌ **Open bus on $2000 writes**: ear6 did not update PPU open bus on $2000 writes (only $2001-$2007). mesen2 updates on ALL PPU writes except $4014. Fixed but did not affect this ROM's output.

13. ❌ **REG trace at index 8181**: After 8180 identical PPU register events, NMI handler writes `$2004` (OAMDATA) in ear6 vs `$4014` (OAMDMA) in mesen2. This means the CPU executed different instructions — a branch was taken differently. Since the first 8180 events are identical across both emulators, the branch difference must come from PPU internal rendering pipeline state (tile shifters, attribute latches) that does not manifest as register accesses.

**Confirmed root cause**: PPU batch model (ear6's current) vs cycle-interleaved model (mesen2). The batch model runs PPU in chunks of 3 cycles per CPU instruction, while cycle-interleaved runs PPU dot-by-dot interleaved with each CPU memory access. This causes subtle scanline/cycle timing differences that compound over frames, shifting the game's scroll position by ~1 scanline per 12 frames. By frame 60, the accumulated offset is measurable.

**Key evidence from cycle-level tracing**:
- All MMC1 register values identical (chr_reg0=2, chr_reg1=3→4, prg_reg=0)
- All palette RAM writes identical (same values, same addresses)
- VRAM nametable data content identical
- NMI firing pattern identical
- Only difference: `$2006` address setup after palette write differs by $21 (33 bytes = coarse X+1, coarse Y+4)
- This is consistent with the game's scroll counter being 1 frame ahead in ear6

**Fix**: Requires PPU architecture migration from batch model to cycle-interleaved model (docs/migration_guide.md Phase 3). The batch model cannot match mesen2's PPU cycle accuracy.

### RomInfo lacks SubMapperID field

`src/nes/nes_types.h:RomInfo` has no `SubMapperID` field. This means:
- **Mapper 002** (UNROM): Cannot detect submapper 2 variants that have bus conflicts (`HasBusConflicts()`)
- Other mappers may also depend on submapper info for variant behavior

Fix when adding `SubMapperID` support: add field to `RomInfo`, parse from iNES 2.0 header byte 15, and update affected mappers.

## Modifying Mesen2 (for trace comparison)

When you need to add trace logging to Mesen2:
- **Prefer editing .cpp files only** — avoid .h files to minimize rebuild time (Mesen2's build is very slow).
- The ONLY valid build command is `make cli -C ../mesen2/DesktopApp`.
- **NEVER delete the Mesen2 build directory** — a full rebuild takes >10 minutes. If you need to force a rebuild of a specific file, remove only the single `.o` file:
  ```bash
  rm -f build-Release/x86_64-PC-Linux/MesenRT/CMakeFiles/MesenLib.dir/Core/NES/NesPpu.cpp.o
  ```
  Then run `make cli -C ../mesen2/DesktopApp` normally — cmake will detect the missing `.o` and rebuild just that file + re-link.
- If you must change a `.h` file (e.g. `SettingTypes.h`), touch only the `.cpp` files that include it, or accept a slow rebuild.

## Notes for AI Agent

- **NEVER auto-commit.** Only commit when the user explicitly asks with `/commit` or says "commit". All changes should remain unstaged/untracked until the user requests a commit.
- Do not use backticks in git commit messages — bash interprets them as command substitution. Use single quotes instead.
- Exported API must be pure C (`extern "C"`), not C++. Internal implementation can use C++.
- C++ exceptions must never cross the `extern "C"` boundary. Every C API entry point must catch all exceptions and convert them to error codes (0 = success, non-zero = error).
- All public headers must live under `ear6/` namespace in both build and install: `#include <ear6/version.h>`, not `#include <version.h>`. CMake generated headers (version.h, export.h) must be placed in `${CMAKE_BINARY_DIR}/ear6/` so they are consistently accessed as `<ear6/xxx.h>` everywhere.
