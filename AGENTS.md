# Ear6 Agent Guide

Ear6 is a cross-platform, multi-system game emulator library. NES is currently
the most complete system, but do not describe or design the project as an
NES-only emulator.

## Required Reading

Agents must open the relevant source document before changing that area. The
following links are the maintained entry points:

- [Documentation map](docs/README.md): use at the start of documentation or
  unfamiliar cross-cutting work
- [Architecture and system model](docs/architecture.md): required for core,
  system, host-boundary, and new-system work
- [Public API manual](docs/api-reference.md): required for public headers, C
  ABI, callbacks, media buffers, installation, or host integration
- [Project roadmap](docs/TODO.md): required when assessing current support,
  choosing priorities, or completing roadmap work
- [NES compatibility results](nes-issue.md): required for any NES accuracy,
  ROM, mapper, PPU, CPU, APU, or input task
- [NES/Mesen2 comparison guide](docs/migration_guide.md): required before
  reference-porting, frame comparison, or trace debugging
- [Mapper implementation checklist](docs/mapper_checklist.md): required before
  adding or changing a mapper
- [Development guide](docs/development.md): required for build, test, repository
  layout, or contribution-process changes
- [Quick start](docs/getting-started.md): required when user-facing build, CLI,
  install, or Web startup steps change
- [Web UI design constraints](docs/web_ui_design.md): required for Web UI
  behavior or visual changes

Treat code as the source of truth. Update affected documentation in the same
change whenever behavior, commands, API, system support, or test coverage
changes.

## Product And API Boundaries

Public headers are split by responsibility:

- `<ear6/ear6.h>`: system-agnostic lifecycle, stepping, video, and audio
- `<ear6/nes.h>`: NES-only region, mapper, palette, and controller concepts
- `<ear6/flash.h>`: reserved Flash extension; the Flash core is not implemented

All declarations in `<ear6/ear6.h>` must have consistent semantics for every
implemented `Ear6SystemType`. Put new system-specific concepts in
`<ear6/<system>.h>`, never in the common header.

The public API is pure C (`extern "C"`) even though internals are C++. C++
exceptions must never cross the ABI boundary. Every fallible C entry point must
catch all exceptions and return an error result.

Public media guarantees:

- framebuffer: tightly packed RGBA8888, four bytes per pixel
- dimensions: current valid framebuffer width and height
- audio: signed 16-bit PCM; `nullptr` and zero when no packet is available
- `ear6_step()`: advances one emulated frame; zero means success

Returned buffers are owned by the context. Callers must not mutate or free
them. See `docs/api-reference.md` before changing pointer lifetime, callback,
or audio-consumption behavior.

Save state is also a system-agnostic memory API. The core serializes and
validates state buffers; host applications own file paths, slots, compression,
databases, and cloud persistence. Do not add state-file I/O to the core, and do
not confuse whole-machine state with battery-backed game save RAM. A system
must reject save/load state until all state that affects future execution is
covered.

Current support must be stated precisely:

| System | Runtime status |
|---|---|
| Test | Implemented |
| NES | Implemented; compatibility varies by mapper and ROM |
| Flash | Not implemented; `ear6_create(EAR6_SYSTEM_FLASH)` returns `nullptr` |

Some declared extension functions are not yet implemented. Keep that status
visible in the API manual and `docs/TODO.md`; never present declaration alone
as runtime support.

## Repository Layout

- `include/ear6/`: installed public C headers
- `src/`: private core and system interface
- `src/nes/`: NES implementation
- `app/cli/`: CLI host
- `app/desktop/`: Qt host
- `app/web/`: Emscripten bridge
- `app/web-ui/`: browser host
- `tests/`: API and ROM regression tests
- `assets/nes/`: embedded NES DB source and local ROM fixtures
- `docs/`: user, API, architecture, and development book

Generated `version.h` and `export.h` belong in `${CMAKE_BINARY_DIR}/ear6/` so
build and install consumers always include `<ear6/version.h>` and
`<ear6/export.h>`.

## Build And Test

```bash
make ear6
make ear6-web
make clean

cmake -B build -S . -DEAR6_BUILD_TESTS=ON
cmake --build build --target ear6-test
./build/ear6-test
./build/ear6-test --gtest_filter=ChoplifterRegression.*
```

`make ear6` builds the native shared library, CLI, and Qt desktop app when Qt 6
is available. `make ear6-web` requires `EMSCRIPTEN_CMAKE_TOOLCHAIN` in `.env`.
A rebuild is the C/C++ lint/type check:

```bash
cmake --build build --target ear6
```

For Mesen2, the only supported build command is:

```bash
make cli -C ../mesen2/DesktopApp
```

## NES Assets And Evidence

Tests use ROMs under `assets/nes/rom/mapper_N/`. ROM filenames and extensions
may vary in case. Do not commit copyrighted ROMs.

`assets/nes/nes_db.txt` is the NES DB source of truth. CMake embeds it through
`cmake/embed_nes_db.cmake`; runtime code must consume embedded text so native
and WASM behavior remain identical.

For every NES mapper or compatibility task:

1. Use `./build/app/cli/ear6-cli info <rom>` to confirm header mapper metadata.
2. Compare ear6 and Mesen2 at the same sampled frames.
3. Visually inspect both images before classifying a pixel difference: one may
   be valid while the other is corrupt, or both may be valid with local drift.
4. Find the first divergent frame, then compare CPU/PPU evidence as needed.
5. Record both 100% matches and known differences in `nes-issue.md`.
6. Add a focused regression test when the ROM can be represented by a stable
   local fixture path.
7. Commit each completed mapper or independent correction promptly when the
   user has requested incremental commits.

Do not infer full mapper correctness from mapper-factory support or one green
frame. `nes-issue.md` must state ROM count, sampled frames, match scope, and any
probe limitations.

## Naming

- Classes and type aliases: PascalCase (`NesCpu`, `MapperType`)
- Enum constants: UPPER_SNAKE_CASE (`EAR6_SYSTEM_NES`)
- Namespace/file constants: UPPER_SNAKE_CASE (`DEFAULT_NES_PALETTE`)
- Functions and variables: snake_case (`parse_value`)
- Non-public members: snake_case plus trailing underscore (`data_`)
- Getters: `get_xxx()`; boolean getters: `is_xxx()`
- Setters: `set_xxx()`

## Debugging

Do not guess at hangs or crashes. Build Debug and use GDB:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
gdb --args ./build/app/cli/ear6-cli screenshot -f 1 game.nes -o /tmp/frame.ppm
```

Use `bt` after a crash or after interrupting a hang. For behavior differences,
follow `docs/migration_guide.md`; it defines trace gates, frame alignment, and
first-divergence rules.

## Worktree And Commits

- Preserve unrelated user changes and stage only explicit paths.
- Never use destructive Git commands unless the user explicitly requests them.
- Do not auto-commit unless the user says `commit` or requests incremental
  commits. When they do, commit each completed independent change promptly.
- Do not use backticks in commit messages; the shell interprets them.
