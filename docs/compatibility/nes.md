# NES Compatibility Results

100% sampled mappers: 0, 1, 2, 3, 64, 69, 74, 90, 99, 118, 245

Unless a section says that every ROM was tested, "100%" means that every pixel
matched Mesen2 for the listed ROMs and sampled frames. It does not claim that
untested ROMs, later gameplay, every bank, or every IRQ path is covered. Mapper
45 also has one 100% ROM, but is not listed above because its second known ROM
has the documented CHR-RAM/PPU difference below. Mappers 64, 69, 74, 90, 99,
118, and 245 have a permanent frame-256 comparison regression test. Mapper 252
has a permanent frame-256 visual regression against its canonical dump. Mappers
0, 1, 2, and 3 have full-ROM-set regression coverage at their documented frames.

> ⚠️ **Choplifter (J).nes** 在 mesen2 中渲染异常（仅显示单色错误画面），
> ear6 渲染正常（第 6 帧进入游戏画面）。
> 该 ROM 在 mesen2 的 NES DB 中原被标记为 mapper 6（commit `0322dba` 修正为 mapper 3），
> 修正后渲染正确。推测 NES DB 中该条目的 mapper 字段存在错误。
> ear6 已将该 ROM 纳入永久回归测试。

## Mapper 1

- Total: 117 ROMs (frame 30/60)
- Perfect: 117/117 (100.0%/100.0%)
- Partial: 0/0 (0.0%/0.0%)
- None (0%): 0/0 (0.0%/0.0%)

> 以下 3 个 ROM 原本存在 `.sav` 文件，mesen2 启动时通过 `LoadBattery` 加载非零的 save RAM 数据，
> 而 ear6 默认将 save RAM 全零初始化。删除 `.sav` 后两个模拟器均从全零开始，输出一致。
>
> - `Battle Stadium - Senbatsu Pro Yakyuu (J).nes` — f=30: 98.59% → **100%**
> - `Bloody Warriors - Shan Goo No Gyakushuu (J).nes` — f=30: 99.92% → **100%**
> - `Best Play - Pro Yakyuu '90 (J).nes` — f=60: 98.85% → **100%**

## Mapper 3

- Total: 24 ROMs
- Perfect: 24 (100.0%)
- Partial: 0 (0.0%)
- None (0%): 0 (0.0%)

All 24 ROMs verified 100% pixel match vs Mesen2. Frame-by-frame regression
tests cover every ROM at frame 30 and frame 60.

> `Choplifter (J).nes` — previously listed as 0% due to Mesen2 NES DB bug
> (incorrectly mapped as mapper 6). Fixed via `nes_db.txt` correction
> (commit `0322dba`). ear6 renders correctly. See separate regression test.

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

### `Yong Ze Do Re Long 6 (C).nes` frame-phase difference

Although its iNES header says mapper 245, both emulators apply the NES DB entry
and run it as mapper 4. Frames 1/30/60/256 are pixel-perfect, but frame 128 is
95.97% identical (58,964/61,440 pixels). At that frame ear6 shows a complete,
valid Waixing Computer Science copyright screen while Mesen2 is still black.
This is a transient mapper 4/PPU frame-phase difference, not mapper 245 banking
behavior or corrupted rendering.

## Mapper 45

### 100% sampled ROM

`BrainSeries13in1.nes` is pixel-perfect at frames 1/30/60/128/256. The permanent
regression test covers frame 256.

### Known difference

`Super 8 in 1 Fighting (UNL).nes` has mapper 251 in its iNES header, but both
emulators apply the NES DB entry and run it as mapper 45. Frames 1-4 are
pixel-perfect; from frame 5 through the sampled frames 30/60/128/256, the match
is 77.53% (47,634/61,440 pixels). ear6 is black while Mesen2 shows a dotted,
garbled screen with broken text, so neither output is a valid game screen.

At frame 5 both CPU traces contain 42,605 instructions and match line-for-line,
including frame/scanline/cycle, PC, opcode, A/X/Y/SP, and status. The remaining
difference is in the shared CHR-RAM/PPU initialization path, not mapper 251 or a
mapper 45 CPU banking/IRQ divergence.

## Mapper 7

- Total: 2 ROMs
- Perfect: 1 (50.0%)
- Partial: 1 (50.0%)
- None (0%): 0 (0.0%)

| ROM | Match |
|---|---:|---:|
| `Battletoads Double Dragon (U).nes` | 99.93% |

## Mapper 23

- Total: 6 ROMs (frames 1/30/60/128)
- Perfect ROMs: 5/6
- Partial ROMs: 1/6
- None (0%): 0/6

| ROM | Frame 30 | Frame 60 | Frame 128 |
|---|---:|---:|---:|
| `Ganbare Goemon 2 (J).nes` | 99.01% | 98.99% | 99.37% |

Both emulators render a valid title screen. The difference is limited to the
small walking sprites along the bottom; the title, background, text, and palette
match. Frames 1-10 are pixel-perfect and the first difference is 31 pixels at
frame 11. At frame 12 both CPU traces contain 102,449 instructions and match
line-for-line after normalizing the emulator prefix, including frame, scanline,
cycle, PC, opcode, A/X/Y/SP, and status. This is a shared PPU/sprite timing
difference rather than a mapper 23 CPU, IRQ, or banking divergence.

## Mapper 64

- Sampled ROMs: 1
- Perfect sampled ROMs: 1 (100.0%)
- Frames: 1/30/60/128/256

`Excitebike (JU).nes` is identified as mapper 64 by its iNES header, but the NES
DB maps the original dump to mapper 0. Changing the final CHR byte changes the
CRC without affecting the sampled startup content, allowing both emulators to
exercise mapper 64. All five sampled frames are pixel-perfect.

The ROM only produces a solid green screen in this probe. The CPU traces share
the same core prefix, but the probe does not exercise meaningful bank-switching
or IRQ-driven gameplay. The permanent test therefore guards mapper creation,
initial mapping, and the frame-256 output only; it is not broad mapper 64
behavioral coverage.

## Mapper 69

- Sampled ROMs: 1
- Perfect sampled ROMs: 1 (100.0%)
- Frames: 1/30/60/128/256

`Batman (J).nes` is pixel-perfect at every sampled frame. The frame-30 CPU
traces also match line-for-line. The permanent regression test covers frame
256.

## Mapper 74

- Sampled ROMs: 3
- Perfect sampled ROMs: 3 (100.0%)
- Frames: 1/30/60/128/256

`d4cjqrdz.nes`, `ds.nes`, and `srw2.nes` are pixel-perfect at every sampled
frame. The permanent regression test uses `srw2.nes` at frame 256.

## Mapper 90

- Sampled ROMs: 2
- Perfect sampled ROMs: 2 (100.0%)
- Frames: 1/30/60/128/256

`finalf3.nes` and `scontra.nes` are pixel-perfect at every sampled frame. The
permanent regression test uses `finalf3.nes` at frame 256.

## Mapper 99

- Sampled ROMs: 1
- Perfect sampled ROMs: 1 (100.0%)
- Frames: 1/30/60/128/256

`vs battle city.nes` is pixel-perfect at every sampled frame. The permanent
regression test covers frame 256.

## Mapper 118

- Sampled ROMs: 2
- Perfect sampled ROMs: 2 (100.0%)
- Frames: 1/30/60/128/256

`Arumajiro (J).nes` and `Pro Sport Hockey (U).nes` are pixel-perfect at every
sampled frame. The permanent regression test uses `Arumajiro (J).nes` at frame
256.

## Mapper 245

- Sampled ROMs: 1 mapper probe
- Perfect sampled ROMs: 1 (100.0%)
- Frames: 1/30/60/128/256

`Yong Ze Do Re Long 6 (C).nes` has mapper 245 in its iNES header, but the NES DB
maps the unmodified dump to mapper 4. Appending trailing data changes the CRC
without changing the declared PRG/CHR data, allowing both emulators to exercise
mapper 245. The probe is pixel-perfect at all five sampled frames, and the
permanent regression test covers frame 256.

This mapper 245 probe result is separate from the original dump's mapper 4
frame-128 phase difference documented above.

## Mapper 252

- Sampled ROMs: 1
- Frames: 1/30/60/128/256

The canonical `3GO.NES` dump (PRG+CHR CRC32 `8B7EE49B`) reaches the valid
Waixing Computer Science copyright screen. The permanent regression covers its
frame 256 output. A fresh Mesen2 comparison was unavailable for this result, so
mapper 252 is not included in the 100% comparison list above.

An earlier local file at this path had PRG+CHR CRC32 `BE1713C7` and rendered a
stable garbled screen in both Ear6 and the previously sampled Mesen2 build. Its
CHR data was page-shuffled: the startup registers selected 1 KiB banks
`74`-`77`, which contained the canonical banks `64`-`67`; the canonical
`74`-`77` data instead appeared at banks `3C`-`3F`. Reordering all sixteen 8 KiB
CHR pages restored a pixel-identical frame 256 without changing mapper logic.
The local regression fixture was therefore replaced with the canonical dump
rather than adding a CRC-specific mapper workaround.
