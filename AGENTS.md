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

## Debug Trace Controls (Two-Level Gating)

All debug TRACE/LOG output must be enabled by both:
1. compile-time macro (`EAR6_ENABLE_*`)
2. runtime env var (`EAR6_TRACE_*`)

If either is missing, no debug log is emitted.

- `EAR6_ENABLE_CPU_TRACE` + `EAR6_TRACE_CPU`
- `EAR6_ENABLE_PPU_TRACE` + `EAR6_TRACE_PPU`
- `EAR6_ENABLE_CPU8448_TRACE` + `EAR6_TRACE_CPU8448`
- `EAR6_ENABLE_MINIMAL_BAD_WINDOW_TRACE` + `EAR6_TRACE_MINIMAL_BAD_WINDOW`
- `EAR6_ENABLE_CYCLE_ALIGN_TRACE` + `EAR6_TRACE_CYCLE_ALIGN`
- `EAR6_ENABLE_EARLY_FRAME_SUMMARY_TRACE` + `EAR6_TRACE_EARLY_FRAME_SUMMARY`
- `EAR6_ENABLE_DMARIO_TRACE11` + `EAR6_TRACE_DMARIO11`
- `EAR6_ENABLE_PALETTE_TRACE` + `EAR6_TRACE_PALETTE`
  - Optional: `EAR6_TRACE_PALETTE_FRAME`, `EAR6_TRACE_PALETTE_DUMP_PREFIX`

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

- **FDS hardware is not implemented**: FDS BIOS/classic FDS-dependent code paths can diverge because `$4024-$403F` and FDS RAM adapter behavior are missing.
- **PPU model parity is incomplete**: Some VS/System and mapper-heavy titles still require tighter cycle-level parity with mesen2's interleaved model.
- **Mapper-specific submapper logic is partial**: `RomInfo.submapper_id` exists and can be filled from NES DB, but mapper behavior must be implemented per mapper.

## Debugging Lessons

- **Prefer code-path confirmation over log-only inference**: read mesen2 implementation first, then add trace points to validate hypotheses.
- **Differentiate observation vs behavior changes**: add trace instrumentation freely; gate or postpone semantic changes until evidence is complete.
- **Treat NES DB as authoritative metadata**: apply DB overrides (mirroring/input/PPU model/etc.) with explicit field mapping and safe bounds checks.
- **Use layered localization**: RGB mismatch -> raw index mismatch -> register/event timeline -> memory read/write source.
- **Always run targeted regressions after fixes**: verify the target ROM and at least 1-2 known-good ROMs in related mappers.

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
