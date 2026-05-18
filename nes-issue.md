# NES Issues (Frame 30)

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

## Mapper 0

- Total: 62 ROMs
- Perfect: 60 (96.8%)
- Partial: 1 (1.6%)
- None (0%): 1 (1.6%)

| ROM | Match |
|---|---:|
| `Antarctic Adventure (J).nes` | 0.00% |
| `Family BASIC (Ver 3) (J).nes` | 99.59% |

## Mapper 1

- Total: 117 ROMs
- Perfect: 116 (99.1%)
- Partial: 1 (0.9%)
- None (0%): 0 (0.0%)

| ROM | Match |
|---|---:|
| `vs dr mario.nes` | 97.43% |

## Mapper 2

- Total: 43 ROMs
- Perfect: 43 (100.0%)
- Partial: 0 (0.0%)
- None (0%): 0 (0.0%)

| ROM | Match |
|---|---:|
| (none) | 100.00% all |

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

- Total: 89 ROMs
- Perfect: 75 (84.3%)
- Partial: 14 (15.7%)
- None (0%): 0 (0.0%)
- E6 Crash: 0 (0.0%)

| ROM | Match |
|---|---:|
| `Babel No Tou (J).nes` | 81.81% |
| `Captain Tsubasa Vol 2 - Super Striker (J).nes` | 98.12% |
| `Cross Fire (J).nes` | 95.65% |
| `Dark Lord (J).nes` | 99.99% |
| `Doki! Doki! Yuuenchi (J).nes` | 50.15% |
| `Downtown - Nekketsu Koushin Kyoku - Soreyuke Daiundoukai (J).nes` | 79.03% |
| `Family Mahjong (J).nes` | 84.98% |
| `Family Pinball (J).nes` | 85.13% |
| `Family Stadium - Pro Yakyuu (J).nes` | 99.70% |
| `Fuzzical Fighter (J).nes` | 85.26% |
| `KICKMAST.NES` | 96.36% |
| `Ningen Heiki - Dead Fox (J).nes` | 91.44% |
| `Super Mario Bros 3 (J).nes` | 58.11% |

## Mapper 7

- Total: 2 ROMs
- Perfect: 0 (0.0%)
- Partial: 0 (0.0%)
- None (0%): 0 (0.0%)
- E6 Crash: 2 (100.0%)

| ROM | Match |
|---|---:|
| `Battletoads Double Dragon (U).nes` | E6-KO |
| `Densetsu No Kishi - Elrond (J).nes` | E6-KO |
