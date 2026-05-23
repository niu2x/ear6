# NES Issues (Frame 30)

100% mappers: 0, 2, 5, 10, 15

> Note: `Antarctic Adventure (J).nes` shows `0.00%` at frame 30 due to a
> one-frame capture timing offset on a full-screen solid-color transition
> (ear6 frame 31 matches mesen2 frame 30 at 100%). This entry is a
> frame-gating artifact in the current comparator, not a confirmed core
> rendering mismatch.

> ⚠️ **Choplifter (J).nes** at `0.00%` is the **opposite** case: ear6 correctly
> transitions to the game screen by frame 6 (blue sky), while Mesen2 remains
> stuck on the title screen (dark green) past 600+ frames. This is a confirmed
> **Mesen2 bug** — ear6's rendering is correct. Keep this ROM as a permanent
> regression test (see `docs/TODO.md` Testing section).

## Mapper 0 ✅

- Total: 62 ROMs
- Perfect: 62 (100%)
- Partial: 0 (0.0%)

## Mapper 1

- Total: 117 ROMs
- Perfect: 116 (99.1%)
- Partial: 1 (0.9%)
- None (0%): 0 (0.0%)

| ROM | Match |
|---|---:|
| `vs dr mario.nes` | 97.43% |

## Mapper 3

- Total: 24 ROMs
- Perfect: 22 (91.7%)
- Partial: 1 (4.2%)
- None (0%): 1 (4.2%)

| ROM | Match |
|---|---:|
| `Atlantis No Nazo (J).nes` | 83.63% |
| `Choplifter (J).nes` | 0.00% |

## Mapper 4

- Total: 107 ROMs
- Perfect: 101 (94.4%)
- Partial: 5 (4.7%)
- None (0%): 1 (0.9%)

| ROM | Match |
|---|---:|
| `Babel No Tou (J).nes` | 81.81% |
| `Capcom 30-in-1 [p][!].nes` | 0.00% |
| `Family Mahjong (J).nes` | 84.98% |
| `Family Pinball (J).nes` | 85.13% |
| `Family Stadium - Pro Yakyuu (J).nes` | 99.70% |
| `ddz.nes` | 97.02% |

## Mapper 7

- Total: 2 ROMs
- Perfect: 1 (50.0%)
- Partial: 1 (50.0%)
- None (0%): 0 (0.0%)

| ROM | Match |
|---|---:|---:|
| `Battletoads Double Dragon (U).nes` | 99.93% |
