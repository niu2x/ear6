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

## Test

```bash
cmake -B build -DEAR6_BUILD_TESTS=ON && cmake --build build --target ear6-test
./build/ear6-test                    # run all tests
./build/ear6-test --gtest_filter=Choplifter*   # single test suite
```

**ROM directory**: Tests search `assets/nes/rom/`. Actual ROMs are organized under
`assets/nes/roms/mapper_N/`. Create a symlink if needed:
```bash
mkdir -p assets/nes/rom && ln -sf ../roms/mapper_3/Choplifter\ \(J\).nes assets/nes/rom/
```

## Migration Notes

- NES <-> Mesen2 migration workflow, debug methodology, trace gating, and Mesen2-specific build/edit rules are maintained in `docs/migration_guide.md`.
- Keep this file focused on project-wide conventions and stable API/engineering constraints.

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

## NES DB Asset Flow

- `assets/nes/nes_db.txt` is the source-of-truth DB snapshot (from Mesen).
- ear6 embeds this file at build time via `cmake/embed_nes_db.cmake`, generating `build/generated/nes_db_embedded.h`.
- Runtime code (`apply_nesdb_overrides`) must consume embedded text, not filesystem reads, so native and wasm behavior stay consistent.


## Notes for AI Agent

- **NEVER auto-commit.** Only commit when the user explicitly asks with `/commit` or says "commit". All changes should remain unstaged/untracked until the user requests a commit.
- Do not use backticks in git commit messages — bash interprets them as command substitution. Use single quotes instead.
- Exported API must be pure C (`extern "C"`), not C++. Internal implementation can use C++.
- C++ exceptions must never cross the `extern "C"` boundary. Every C API entry point must catch all exceptions and convert them to error codes (0 = success, non-zero = error).
- All public headers must live under `ear6/` namespace in both build and install: `#include <ear6/version.h>`, not `#include <version.h>`. CMake generated headers (version.h, export.h) must be placed in `${CMAKE_BINARY_DIR}/ear6/` so they are consistently accessed as `<ear6/xxx.h>` everywhere.
