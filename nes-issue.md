# NES Issues (Frame 30)

100% mappers: 0, 1, 2, 3 (all with unit tests)

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
