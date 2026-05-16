# Debug: vs dr mario — PPU Cycle Divergence

## Summary

`vs dr mario.nes` (mapper 1, MMC1, iNES byte 7 bit 0 = VS flag set) produces 0.00% pixel match with Mesen2 at frame 60, despite frame 5 being pixel-identical.

**Root cause**: Accumulated PPU cycle-level timing differences between ear6's `process_scanline_impl()` and Mesen2's `ProcessScanlineImpl()`. These are NOT register-level bugs — events 0-8447 are identical. The first divergence is an 18-PPU-cycle (6-CPU-cycle) offset that appears between two PPU register writes ~10 scanlines apart, during which there are no PPU register accesses at all. The offset then propagates and causes NMI pattern divergence at frame 9.

**Status (updated 2026-05-16, handoff-ready)**:
- Confirmed and fixed one real divergence source: `$4016` controller read behavior in ear6 was incorrect vs Mesen2.
- This fix removes the previously confirmed `+18 PPU cycles` drift around frame 4 / scanline 259 (`R$2002`, `W$2006` sequence now aligns).
- Frame-level pixel match for `vs dr mario` at frame 60/120 is still `0.00%`; remaining divergence now starts earlier (around frame 2 VBlank / `$2002` timing window).
- Next session should continue from the new earliest divergence point, not from `$4016`/`$01DB`.

---

## Timeline of Divergence

| Frame | Event | ear6 timing | Mesen2 timing | Delta (PPU cyc) |
|-------|-------|-------------|---------------|-----------------|
| 4 | `W$2005=00` (scroll) | 249,173 | same | ±0 |
| 4 | `W$2001=00` (render off) | 259,57 | 259,39 | +18 |
| 4 | `W$2000=10` (nametable) | 259,129 | 259,111 | +18 |
| 4 | `W$2000=10` (2nd) | 259,276 | 259,258 | +18 |
| 4 | `W$2006=20` (addr hi) | 259,306 | 259,288 | +18 |
| 4 | `W$2006=00` (addr lo) | 259,324 | 259,306 | +18 |
| ... | All subsequent events | +18 | baseline | +18 constant |
| 8→9 | `W$2003=00` (DMA setup) | frame 9, cyc=107 | frame 9, cyc=104 | +3 |
| 9→10 | NMI pattern drifts | frame 10 has NMI | frame 10 NO NMI | 1 frame |
| 10+ | All events misaligned | +1 frame | baseline | entire frame |

---

## Experimental Setup

### Build commands

```bash
# ear6:
make ear6

# Mesen2 (ONLY this command, NEVER delete build dir):
make cli -C ../mesen2/DesktopApp
```

### Making Mesen2 deterministic

Change `Core/Shared/SettingTypes.h` line 659:
```cpp
RamState RamPowerOnState = RamState::Random;   // default, non-deterministic
RamState RamPowerOnState = RamState::AllZeros;  // deterministic for debugging
```

Rebuild by removing only the object file (NOT the entire build dir):
```bash
rm -f build-Release/x86_64-PC-Linux/MesenRT/CMakeFiles/MesenLib.dir/Core/NES/NesPpu.cpp.o
make cli -C ../mesen2/DesktopApp
```

### Screenshot comparison

```bash
./build/app/cli/ear6-cli screenshot <rom.nes> -f 60 -o /tmp/ear6.ppm 2>/dev/null
../mesen2/dist/x86_64-PC-Linux/bin/mesen2-cli screenshot <rom.nes> -f 60 -o /tmp/mesen2.ppm 2>/dev/null
cmp /tmp/ear6.ppm /tmp/mesen2.ppm
```

### PPU register trace comparison

Enable tracing in `src/nes/nes_ppu.cpp`:
```cpp
// Change line 12:
//#define ENABLE_PPU_TRACE  →  #define ENABLE_PPU_TRACE
```

Rebuild ear6, capture trace:
```bash
./build/app/cli/ear6-cli screenshot <rom.nes> -f 15 -o /tmp/e.ppm >/dev/null 2>/tmp/ear6_trace.txt
```

For Mesen2, add matching printf traces to `Core/NES/NesPpu.cpp` (see next section), rebuild with object file removal.

Compare traces:
```bash
diff <(sed 's/\[PPU\] *[0-9]* *[0-9\-]* *[0-9]* *[0-9]*/[PPU]/g' /tmp/ear6_trace.txt) \
     <(sed 's/\[PPU\] *[0-9]* *[0-9\-]* *[0-9]* *[0-9]*/[PPU]/g' /tmp/mesen2_trace.txt) | head -40
```

---

## Trace Infrastructure

### ear6 PPU trace (permanent, OFF by default)

**File**: `src/nes/nes_ppu.cpp`
**Enable**: Uncomment line 12: `//#define ENABLE_PPU_TRACE` → `#define ENABLE_PPU_TRACE`

```cpp
// Gate: #define ENABLE_PPU_TRACE
// Format: [PPU] frame scanline cycle cpu_cycle EVENT
// Example: [PPU]      4  249  161        87917 W$2005=00
```

Trace calls at every PPU register read/write, VBL_SET/VBL_CLR/NMI, with CPU cycle count.

To add a new trace call anywhere in NesPpu:
```cpp
trace_ppu("EVENT_NAME\n");              // no args
trace_ppu("W$200X=%02X\n", value);      // with hex value
```

**Available fields for custom traces:**
| Field | Type | Access |
|-------|------|--------|
| `frame_count_` | uint32 | current PPU frame |
| `scanline_` | int16 | current scanline (-1 to 260) |
| `cycle_` | uint32 | current PPU cycle (0-340) |
| CPU cycle | uint64 | `console_->get_cpu()->get_cycle_count()` |

### ear6 CPU trace (permanent, OFF by default)

**File**: `src/nes/nes_cpu.cpp`
**Enable**: Uncomment: `//#define ENABLE_CPU_TRACE` → `#define ENABLE_CPU_TRACE`

```cpp
// Format: [CPU] cpu_cycle EVENT
// Example: [CPU]        87917 IRQ_FLAGS irq=00 run_irq=0 ...
```

Currently traces:
- `IRQ_FLAGS` — every `end_cpu_cycle()`: irq_flag, run_irq, prev_run_irq, nmi flags
- `DMA_START` / `DMA_END` — `process_pending_dma()`: offset, sprite_dma, dmc_dma state

### ear6 APU trace (to be added)

If APU timing is suspected, add a trace function to `NesApu` following the same pattern.

### Mesen2 traces (manual addition)

Mesen2 has no permanent trace infrastructure. Add printf calls manually when debugging:

**Critical timing note (2026-05-16 update):**
- Do **NOT** print per-cycle logs directly in hot paths like `NesCpu::EndCpuCycle()` using `printf/fprintf`.
- Even stderr logging can change timing enough to make `vs dr mario` render all-black.
- If CPU tracing is needed, use buffered logging (ring buffer + periodic/batch flush), and log only state transitions in hot paths.
- Also use portable 64-bit format macros (`PRIu64`) instead of hardcoded `%llu` to avoid UB on platforms where `uint64_t` is `unsigned long`.

**PPU register traces** — in `Core/NES/NesPpu.cpp`:
```cpp
// Template for adding PPU traces:
printf("[PPU] %6d %4d %4d %12llu EVENT\n", _frameCount, _scanline, _cycle,
       _console->GetCpu()->GetState().CycleCount);
printf("[PPU] %6d %4d %4d %12llu W$200X=%02X\n", _frameCount, _scanline, _cycle,
       _console->GetCpu()->GetState().CycleCount, value);
```

**CPU IRQ traces** — in `Core/NES/NesCpu.cpp`, `EndCpuCycle()`:
```cpp
// Recommended: enqueue to ring buffer here, flush outside hot path.
// If printing directly for quick test, log only transitions (NOT every cycle).
fprintf(stderr, "[CPU] %12" PRIu64 " IRQ_FLAGS irq=%02X run_irq=%d prev_run=%d nmi=%d prev_nmi=%d need_nmi=%d\n",
        _state.CycleCount, _state.IrqFlag, _runIrq, _prevRunIrq,
        _state.NmiFlag, _prevNmiFlag, _needNmi);
```

### Mesen2 CPU vars (for traces)

| Variable | Code |
|----------|------|
| CPU cycle count | `_state.CycleCount` |
| IRQ flag | `_state.IrqFlag` |
| NMI flag | `_state.NmiFlag` |
| Need NMI | `_needNmi` |
| Prev NMI flag | `_prevNmiFlag` |
| Run IRQ | `_runIrq` |
| Prev Run IRQ | `_prevRunIrq` |
| DMA active | `_spriteDmaTransfer` |
| DMC active | `_dmcDmaRunning` |

Mesen2 has a slower rebuild cycle. When adding traces, prefer modifying only `Core/NES/NesPpu.cpp`, NOT `.h` files. Add printf calls:

```cpp
printf("[PPU] %6d %4d %4d EVENT\n", _frameCount, _scanline, _cycle);
printf("[PPU] %6d %4d %4d W$200X=%02X\n", _frameCount, _scanline, _cycle, value);
```

To add CPU cycle count:
```cpp
_console->GetCpu()->GetState().CycleCount
```

---

## Mesen2: Where to Add Traces

| Event | WriteRam/ReadRam | Line (approx) | Code to insert |
|-------|-----------------|---------------|----------------|
| W$2000 | WriteRam Control | 417 | `printf("[PPU] %6d %4d %4d W$2000=%02X\\n", ...)` |
| W$2001 | WriteRam Mask | 425 | same |
| W$2003 | WriteRam SpriteAddr | 433 | same |
| W$2004 | WriteRam SpriteData | 437 | same |
| W$2005 | WriteRam ScrollOffsets | 454 | same |
| W$2006 | WriteRam VideoMemoryAddr | 470 | same |
| W$2007 | WriteRam VideoMemoryData | 491 | same |
| W$4014 | WriteRam SpriteDMA | 509 | same |
| R$2002 | ReadRam Status | 338 | `printf(... R$2002)` |
| R$2004 | ReadRam SpriteData | 352 | same |
| R$2007 | ReadRam VideoMemoryData | 374 | same |
| VBL_SET | Exec() | 1347 | after `_statusFlags.VerticalBlank = true;` |
| VBL_CLR | ProcessScanlineImpl() | 901 | at `_statusFlags.VerticalBlank = false;` |
| NMI | TriggerNmi() | 1260 | before `_console->GetCpu()->SetNmiFlag()` |

IRQ flag changes: In `NesCpu.cpp`, `EndCpuCycle()` around line 142-155.

VRAM reads: In `ReadVram()` at line ~667.

---

## What We Know

### Confirmed identical up to event 8447

Events 0-8447 have the SAME scanline/cycle values in both emulators. This means:
- All PPU register writes (PPUCTRL, PPUMASK, PPUSCROLL, PPUADDR, PPUDATA) are at the same cycles
- All $2002 reads are at the same cycles
- All OAM DMA ($4014) starts and ends at the same cycles
- The frame 5 pixel output is identical

### The 18-cycle offset appears at event 8448

- **ear6**: `W$2001=00` at `scanline=259, cycle=57`
- **Mesen2**: `W$2001=00` at `scanline=259, cycle=39`
- Delta: +18 PPU cycles = +6 CPU cycles
- The PREVIOUS event (8447: `W$2005=00` at `249,173`) is identical in both
- Between 8447 and 8448 there are ZERO PPU register events
- CPU-only code executes for ~1098 CPU cycles (ear6) vs ~1092 (Mesen2)

### Update: why that 18-cycle offset happened (resolved)

Additional instrumentation (`[CPU8448]`, `[CPUSTATE]`, `[CPUMEM]`, `[CPUMEMW]`, `[ADPROBE]`, `[WTGL]`, `[VS8448]`) showed:
- `B84C` executes `LDA $4016` (not RAM).
- Before fix: ear6 read `$4016=0x40`, Mesen2 read `$4016=0x01`.
- This caused `A`/`Z` mismatch, then `BEQ` path split at `B813`, then `$01DB` write mismatch (`02` vs `00`), then the observed `W$2006` timing drift.

Fix applied in ear6:
- `NesControlManager` now applies `$4016` writes on pending-write completion (`process_writes`) instead of immediate state change.
- While strobe is set, reads return bit0 of current latched state; when strobe is clear, reads shift; after 8 bits, reads return `1`.
- Removed hardcoded `| 0x40` behavior from `$4016/$4017` reads (it was forcing wrong high bits for this ROM path).

After fix:
- `ADPROBE` confirms ear6 now reads `$4016=0x01` at the same point as Mesen2.
- `WTGL` confirms former `+18 PPU cycles` offset at frame 4 / sl 259 is gone.
- New earliest divergence is earlier: frame 2 around `sl=241` (`VBL_SET` vs repeated `R$2002` pattern).

### The 3-cycle "bump" at frame 8→9

- Between frames 7→9, another +3 PPU cycles accumulate
- This shifts the `$2002` read timing by 3 PPU cycles
- `prevent_vbl_flag_` is set/cleared differently
- NMI fires on frame 10 in ear6 but NOT in Mesen2
- After this, all events are on different frame numbers

### NOT the cause

- `process_scanline_impl()` vs `ProcessScanlineImpl()` are structurally identical
- `load_tile_info()` vs `LoadTileInfo()` are identical
- DMA timing (start, end, count of writes) is the same
- CPU master clock calculation is the same (12 per CPU cycle = 3 PPU)
- `update_state()` vs `UpdateState()` have equivalent behavior
- RAM contents are the same (both AllZeros)
- Odd-frame cycle skip matches (both disabled for Ppu2C03)
- PPU register event trace matches for first 8447 events
- `ppu_offset_` matches (= 1)
- APU frame counter reset-pending behavior matches Mesen2 (`new_value`/`_newValue` logic)

---

## Hypotheses for the 6 Extra CPU Cycles

### Hypothesis 1: Page-crossing penalty difference

A 6502 instruction that accesses memory across a page boundary adds 1 cycle. If branch destinations differ by 1 byte → page cross happens in one emulator but not the other → 1-cycle difference.

**Testing**: Compare CPU cycle counts at each instruction boundary. Use a NES debugger or instruction trace.

### Hypothesis 2: Branch taken/not-taken difference

A conditional branch taken from a non-page-crossing address costs 2 cycles if taken, 1 if not. If `prev_run_irq_` or `prev_need_nmi_` differs by 1 cycle → the NMI/IRQ check after `exec()` changes behavior → possibly takes a different path.

**Testing**: Add IRQ flag tracing to both emulators. Compare `irq_flag` values and `need_nmi_` at each `EndCpuCycle()`.

### Hypothesis 3: DMA state machine consumes different cycles

`process_pending_dma()` has multiple branches for DMC DMA, dummy reads, sprite DMA. If the state machine enters a different branch (e.g., DMC DMA active in one but not the other), the total cycle count changes.

**Testing**: Add trace at `process_pending_dma()` entry/exit showing which branches were taken.

### Hypothesis 4: APU frame counter reset-pending behavior mismatch

~~The reset-pending path might differ and inject timing drift.~~

**Result (2026-05-16): ruled out.** ear6 and Mesen2 are aligned here:
- Reset seeds a pending `$4017=$00`-equivalent value.
- Apply happens after the same 3/4-cycle delay rule.
- Pending state is cleared by setting `new_value`/`_newValue` to `-1`.

No code change needed for this hypothesis.

### Hypothesis 5: DMC/SPR-DMA concurrency

If both DMC DMA and sprite DMA are active at the same time, the `process_pending_dma()` loop handles both, potentially changing the cycle count. If one emulator has DMC active when the other doesn't, the cycle count differs.

**Testing**: Add trace to `process_pending_dma()` showing `dmc_dma_running_` and `sprite_dma_transfer_` values.

---

## Next Steps for Debugging

## Next Session Handoff (start here)

1. Keep the controller fix (do not revert): it is validated and removes a confirmed branch divergence source.
2. Re-run short trace (`-f 15`) and focus on earliest mismatch now at frame 2 around `scanline 240-241`.
3. Investigate `VBL_SET` / `R$2002` ordering:
   - compare `update_status_flag()` / `prevent_vbl_flag_` semantics and timing window,
   - verify exact cycle condition for suppressing VBlank on `$2002` reads at NMI boundary.
4. Once earliest divergence is shifted again, repeat “first mismatch” workflow instead of continuing from old frame-4 assumptions.

---

## 2026-05-16 Addendum: Logging Instability + Frame-8 Color Divergence

### What was confirmed in this round

1. `$4016` fix remains correct and must be kept. Earlier branch split was removed as expected.

2. CPU probe points that were previously suspected now align under low-noise tracing:
   - `BADD`, `B84A`, `B85B`, `B87D`, `B880`, `B885` are aligned through frame 28 in clean runs.
   - Stable state split starts at frame 29 in those probes (`A/Y` differs), but this is not the first visual split.

3. Frame-level screenshot binary diff (no runtime instrumentation) shows first visual mismatch appears much earlier:
   - first mismatching frame: **7** (later experiment indicates effective breakpoint around frame 8 depending on run path).
   - symptom: whole-frame single-color mismatch (all 61440 pixels differ), not local tile/sprite corruption.

4. Palette value itself did not explain the split in ear6:
   - `pal0` stayed `0x09` in early frame summaries.
   - `fb0` also reached `0x09` in ear6 frame summaries, yet mesen2 screenshot output still remained black in problematic runs.

5. Temporary ear6 experiment (force black output for early frames) validated that the mismatch is frame-window dependent:
   - with forced black for early frames, frames 6-7 matched exactly,
   - mismatch reappeared immediately after forced window ended.
   - This experiment was reverted (do not keep this behavior).

### Critical finding: Mesen2 debug logging is unstable

Adding logs to Mesen2 (even reduced logs) repeatedly changed execution behavior:

- Runs often stopped producing expected probe points after frame 5-8.
- Same code path sometimes reached target CPU cycle windows, sometimes not.
- High-frequency PPU/CPU traces clearly caused severe timing perturbation.

This means many runtime traces collected from heavily instrumented Mesen2 are not trustworthy for root-cause timing comparisons.

### New priority task (before further cycle-level conclusions)

**Stabilize Mesen2 into deterministic single-thread debug mode.**

Goal: remove timing perturbation from thread scheduling and I/O interleaving so trace runs are reproducible.

Required work:

1. Investigate Mesen2 threading used by CLI screenshot path (video decoder / notifications / event processing).
2. Add a single-thread debug mode (compile-time or runtime flag) that forces deterministic execution order.
3. Keep logging minimal and buffered; no hot-path `fprintf` in per-cycle loops.
4. Re-validate baseline:
   - same ROM, same frame count should produce stable probe coverage every run,
   - then resume earliest-divergence search.

### Immediate next steps for next session

1. Keep ear6 black-frame experiment reverted (already done).
2. Work in Mesen2 first: implement/enable single-thread deterministic debug path for screenshot runs.
3. Re-run first-mismatch frame scan (1..60) in stable mode.
4. Only after stability is proven, reintroduce tiny probes (`BADD`/`B84A`/frame summary) and continue bisection.

---

## 2026-05-16 Addendum B: Mesen2 Output Path Root Cause + Current Baseline

### 1) Why mesen2 also became "wrong" during debugging

This was caused by **debug-side logic drift**, not only by logging overhead:

- Heavy probes were added in `Core/NES/NesCpu.cpp` and `Core/NES/NesPpu.cpp` hot paths (IRQ/NMI/VBL edge windows).
- A non-trace semantic write was introduced in one debug variant (VBlank flag path touched during pre-render clear window).
- CLI output path was changed multiple times (software-renderer callback vs PPU-index export + external palette mapping), which changed screenshot semantics.

Result: stashing debug changes restored expected mesen2 rendering.

### 2) Verified clean reference point

- Known-good mesen2 commit for screenshot behavior: `adc959cfd121f17d25e81de3e3179a2b9b550fae`.
- `master` diverged by adding CPU/PPU debug instrumentation + altered CLI export logic.
- A debug snapshot was preserved for traceability: `a28ad818`.
- Then cleanup commit kept only intended direction: deterministic CLI path cleanup + export focus.

### 3) PPU-index export lesson

When CLI converted PPU index (`uint16_t`) to RGB using hardcoded `DEFAULT_NES_PALETTE`, screenshots could be globally wrong for VS palette cases.

Key insight: if exporting from PPU index, RGB mapping must follow mesen2 internal runtime palette/model path, otherwise screenshot comparison is invalid.

### 4) Current direction for mesen2 CLI

Use mesen2-internal final RGBA frame for screenshot output (no external palette mapping in CLI):

- Add API to expose internal RGBA frame buffer from mesen2 renderer/video pipeline.
- CLI writes that RGBA buffer directly to PPM.

This keeps palette/model/filter responsibility inside mesen2 core path.

### 5) Ear6 vs mesen2 baseline (latest run)

Using current local trees during this session, frame scan `1..60` for `vs dr mario.nes` showed:

- first mismatch frame: `1`
- frame `1..60`: `61440/61440` mismatched pixels each frame (`100%`)

This baseline was captured **before** final cleanup/alignment of all debug branches; rerun baseline after locking both repos to agreed clean states.

### 6) Next-session checklist

1. Freeze mesen2 to known clean branch state for comparison (no hot-path probes in CPU/PPU core).
2. Keep only stable screenshot path (internal RGBA export) in mesen2 CLI.
3. Re-run frame scan `1..60` and record per-frame mismatch ratio.
4. If mismatch pattern suggests color-only divergence, dump first N palette indices + resolved RGB on both sides.
5. Continue first-real-divergence bisection only after baseline is repeatable.

### Temporary trace knobs currently in tree

- ear6:
  - `src/nes/nes_ppu.cpp`: `ENABLE_PPU_TRACE`, `ENABLE_VS8448_TRACE`, `[WTGL]`, `[VS8448]`
  - `src/nes/nes_cpu.cpp`: `ENABLE_CPU8448_TRACE`, `[CPU8448]`, `[CPUSTATE]`, `[CPUMEM]`, `[CPUMEMW]`, `[ADPROBE]`
- Mesen2:
  - `Core/NES/NesPpu.cpp`: `[PPU]` register traces + `[WTGL]`/`[VS8448]`
  - `Core/NES/NesCpu.cpp`: `[CPU8448]`, `[CPUSTATE]`, `[CPUMEM]`, `[CPUMEMW]`, `[ADPROBE]`

These are intentionally verbose for root-cause work and should be gated/cleaned in a follow-up once debugging ends.

### Step 1: Enable CPU IRQ traces and compare

Already built into ear6 (`#define ENABLE_CPU_TRACE` in `src/nes/nes_cpu.cpp`).
Add matching printf to Mesen2's `EndCpuCycle()` (see Mesen2 traces section above).

Run both with 6 frames:
```bash
./build/app/cli/ear6-cli screenshot <rom> -f 6 -o /tmp/e.ppm >/dev/null 2>/tmp/ear6_cpu.txt
../mesen2/dist/.../mesen2-cli screenshot <rom> -f 6 -o /tmp/m.ppm >/tmp/mesen2_cpu.txt 2>/dev/null
```

Compare IRQ timing:
```bash
grep IRQ_FLAGS /tmp/ear6_cpu.txt | head -20
grep IRQ_FLAGS /tmp/mesen2_cpu.txt | head -20
```

### Step 2: Instruction-level trace

Use nestest or a custom trace to compare instruction-by-instruction execution between the two emulators. This would show exactly which instruction costs an extra cycle.

### Step 3: APU state comparison

Add APU register dump and frame counter state comparison at each NMI or $4017 write.

### Step 4: Compare process_pending_dma behavior

Add tracing at every DMA branch point to see if DMC DMA is active.

---

## Code References

| ear6 | Mesen2 | Function | 
|------|--------|----------|
| `src/nes/nes_ppu.cpp:168` | `Core/NES/NesPpu.cpp:868` | `ProcessScanlineImpl()` |
| `src/nes/nes_ppu.cpp:240` | `Core/NES/NesPpu.cpp:667` | `LoadTileInfo()` |
| `src/nes/nes_ppu.cpp:267` | `Core/NES/NesPpu.cpp:-` | `ShiftTileRegisters()` |
| `src/nes/nes_ppu.cpp:707` | `Core/NES/NesPpu.cpp:1004` | `ProcessSpriteEvaluation()` |
| `src/nes/nes_ppu.cpp:630` | `Core/NES/NesPpu.cpp:1421` | `UpdateState()` |
| `src/nes/nes_cpu.cpp:124` | `Core/NES/NesCpu.cpp:-` | `StartCpuCycle()` |
| `src/nes/nes_cpu.cpp:131` | `Core/NES/NesCpu.cpp:-` | `EndCpuCycle()` |
| `src/nes/nes_cpu.cpp:280` | `Core/NES/NesCpu.cpp:-` | `ProcessPendingDma()` |
| `src/nes/nes_apu.cpp:152` | `Core/NES/NesApu.cpp:-` | `ProcessCpuClock()` |

---

## Tools

| Tool | Purpose |
|------|---------|
| `cmp` (unix) | Compare two PPM files byte-by-byte |
| `diff` (unix) | Compare two trace files line-by-line |
| `sed 's/\[PPU\].*/.../'` | Strip frame/scanline/cycle from traces for event-only comparison |
| `python3` with `awk` | Parse trace files, extract specific events, calculate deltas |
| `xxd` | Hex dump PPM to check pixel values |
| `strings` | Check binary for embedded trace strings |

### Trace diff workflow

```bash
# 1. Strip frame/scanline/cycle, compare events
diff <(sed 's/\[PPU\] *[0-9]* *[0-9\-]* *[0-9]* *[0-9]*/[PPU]/g' ear6.txt) \
     <(sed 's/\[PPU\] *[0-9]* *[0-9\-]* *[0-9]* *[0-9]*/[PPU]/g' mesen2.txt) | head -20

# 2. Find first event where scanline/cycle differs
paste <(sed 's/\[PPU\] *//' ear6.txt) <(sed 's/\[PPU\] *//' mesen2.txt) | \
  awk '{if($2!=$7||$3!=$8||$4!=$9){print NR": ear6="$1,$2,$3,$4,"mesen2="$6,$7,$8,$9;exit}}'

# 3. Calculate CPU cycle delta between two events
grep "W\$2005\|W\$2001=00" trace.txt | awk '{print $1,$5}' | paste - - | \
  awk '{diff=$4-$2; print "CPU cycles:",diff," PPU cycles:",diff*3}'
```
