# Ear6 - Game Emulator Library

## Build

```bash
make ear6          # native: libear6.so + ear6-desktop
make ear6-web      # wasm: libear6.a + ear6-web.js (requires Emscripten in .env)
make clean
```

## Lint / Typecheck

```bash
cmake --build build --target ear6   # rebuild catches compile errors
```

## Naming

- **Classes / type aliases**: PascalCase (`NesCpu`, `MapperType`)
- **Enum constants**: UPPER_SNAKE_CASE (`DISCONNECTED`, `HANDSHAKING`, `CHOKE`, `BITFIELD`)
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

## Notes for AI Agent

- Do not use backticks in git commit messages — bash interprets them as command substitution. Use single quotes instead.
- Exported API must be pure C (`extern "C"`), not C++. Internal implementation can use C++.
- C++ exceptions must never cross the `extern "C"` boundary. Every C API entry point must catch all exceptions and convert them to error codes (0 = success, non-zero = error).
- All public headers must live under `ear6/` namespace in both build and install: `#include <ear6/version.h>`, not `#include <version.h>`. CMake generated headers (version.h, export.h) must be placed in `${CMAKE_BINARY_DIR}/ear6/` so they are consistently accessed as `<ear6/xxx.h>` everywhere.
