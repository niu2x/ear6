# ear6

A tiny but serious cross-platform emulator library project.

## What this project is

- NES implementation is primarily benchmarked and aligned against the **Mesen2** project (timing, behavior, migration workflow).
- Our core output is a **cross-platform emulator library**: native + web targets from one codebase.

## Why this matters

- Build once, integrate anywhere: desktop apps, tools, and browser runtimes.
- Keep emulator behavior consistent across platforms, including wasm.

## Project Status

| Mapper | Name | Status |
|--------|------|--------|
| 0 | NROM | ✅ Complete, unit tests |
| 1 | MMC1 (SMB3, Zelda, Metroid) | ✅ Complete, unit tests |
| 2 | UNROM (Mega Man, Castlevania) | ✅ Complete, unit tests |
| 3 | CNROM (Arkanoid, Mappy) | ✅ Complete, unit tests |

All four basic mappers are implemented and verified with frame-by-frame regression
tests against Mesen2 reference output. See `nes-issue.md` for detailed coverage.

> The author is currently busy with other things. Contributions and issues are
> still welcome, but responses may be slower.

## Live demo

- https://niu2x.github.io/ear6

If you just want to see it running in browser, open the link and start there.
