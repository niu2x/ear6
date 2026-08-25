# Ear6

Ear6 is a small, cross-platform, multi-system game emulator library with a
system-agnostic C API. The NES core is the most mature implementation today,
while the library boundary is designed for NES and future systems alike.

```text
host application -> <ear6/ear6.h> -> selected emulator system
                                      + <ear6/nes.h> for NES extensions
```

## Start here

- [Documentation](docs/README.md)
- [Quick start](docs/getting-started.md)
- [Public API manual](docs/api-reference.md)
- [Architecture](docs/architecture.md)
- [Roadmap](docs/TODO.md)

Build the native library, CLI, and optional Qt desktop app:

```bash
make ear6
./build/apps/cli/ear6-cli info path/to/game.nes
```

Run the test suite:

```bash
make test
```

Build and serve the WebAssembly frontend:

```bash
make serve
```

Live demo: <https://niu2x.github.io/ear6>

## Current scope

| System | Status |
|---|---|
| Test | Implemented for host/API validation |
| NES | Implemented and actively compared against Mesen2 |
| Flash | Public API placeholder; core not implemented |

NES compatibility is tracked by evidence, not by mapper source-file count.
See [NES compatibility results](docs/compatibility/nes.md) for sampled 100% matches and
known differences.

ROMs and BIOS files are not distributed with Ear6. Use only content you are
legally entitled to use.
