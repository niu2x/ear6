# NES 与 Mesen2 对比开发指南

本章是 Ear6 NES 核心的参考实现迁移、mapper 开发和精确性调试手册。Mesen2 是
当前参考目标，但“与 Mesen2 不同”不自动等于 Ear6 错误：必须同时检查两边画面
是否合理，并用日志与代码建立证据。

当前兼容性结果见 [NES 兼容性结果](compatibility/nes.md)，项目级未完成能力见
[TODO](TODO.md)。本文件只维护可重复的方法，不维护会迅速过期的 mapper 状态表。

## 1. 参考环境

Ear6：

```bash
make ear6
```

Mesen2 CLI 只使用以下入口构建：

```bash
make cli -C ../mesen2/DesktopApp
```

不要为 Mesen2 猜测仓库根目录 CMake 命令。对比前记录两边 commit、构建类型、
ROM hash、命令行和环境变量。

## 2. 当前执行模型

不要再依据旧文档把 Ear6 描述为“整条 CPU 指令后批量运行 PPU”。当前模型是：

1. `NesConsole::run_frame()` 重复调用 `NesCpu::exec()`，直到 PPU frame count 改变。
2. CPU 的每个读写周期分别调用 `start_cpu_cycle()` 和 `end_cpu_cycle()`。
3. 两个半周期都按 CPU master clock 调用 `NesPpu::run()`。
4. PPU 在 `run()` 内逐个 `exec()` 到目标 master clock。
5. mapper/APU/输入 pending writes 在 CPU clock hook 中推进。

当前内存模型也已经包含：

- `NesMemoryManager` 的 64K read/write handler 分发表
- `BaseMapper` 的 PRG/CHR 256-entry page tables
- 统一的 PPU bus address notification 路径
- `RomInfo.submapper_id`、work RAM、battery、chip 和 NES DB 覆盖

因此调试必须从当前代码开始，不能把历史迁移计划当作尚未实现的事实。

## 3. 一次最小闭环

每次只验证一个明确假设：

1. 固定双方输入和外部状态。
2. 确认最终 mapper/metadata。
3. 找到首个差异帧。
4. 先做视觉分类。
5. 判断差异在 CPU、PPU index、palette mapping 还是 final RGB。
6. 用双方代码路径解释首差异。
7. 做一个最小修改。
8. 回归首差异帧、后续累计帧和同 mapper 其他 ROM。
9. 更新 `docs/compatibility/nes.md` 并提交这个独立改动。

不要一次迁移一大片代码后再看最终截图。那样无法知道哪一条语义真正改变了结果。

## 4. 固定输入与外部状态

### ROM

记录文件 hash，不只记录文件名：

```bash
shasum -a 256 path/to/game.nes
./build/apps/cli/ear6-cli info path/to/game.nes
```

CLI `info` 显示 header mapper；运行时日志显示 NES DB 覆盖后的最终 mapper。两者都要
记录。iNES header 里的旧格式垃圾位、尾部填充和 trainer 都可能改变判断。

### Save 与配置

Mesen2 可能自动读取已有 `.sav`，Ear6 当前通常从内存初始值开始。对比前隔离双方
save 目录，或明确使用同一份初始 save。不要把持久化状态差异误判为 CPU/mapper
错误。

同时固定：

- region 和 PPU model
- controller/input device
- palette 与后处理
- 帧号定义和启动输入
- ROM DB 版本

## 5. 截图和帧对齐

生成 Ear6 截图：

```bash
./build/apps/cli/ear6-cli screenshot \
  -f 60 path/to/game.nes -o /tmp/ear6-f60.ppm
```

使用同一 ROM、同一帧号生成 Mesen2 截图。先采样 `1/2/3/4/5/10/30/60/128/256`，
再在第一个不一致区间二分或逐帧查找。

比较时区分三个问题：

1. framebuffer 尺寸和输出格式是否一致？
2. 游戏状态是否处于同一帧阶段？
3. 同一坐标的像素是否一致？

只有第 3 项是纯像素差异。完整、合理但早一帧出现的版权画面属于 frame-phase
差异；不能描述成乱码或 mapper banking 完全错误。

## 6. 先看两幅图

像素百分比之前，必须同时查看 Ear6 和 Mesen2 图像，可使用人工或多模态检查。
把结果归入一种：

| 分类 | 后续方向 |
|---|---|
| 一方合理、一方黑屏/乱码 | 合理一方可能正确；追踪另一方加载、bank、VRAM 或设备 |
| 双方都合理，局部不同 | 优先 sprite、scroll、palette、IRQ 或帧相位 |
| 双方都乱码但像素不同 | 不能宣称任一方正确；先找共同的内容/初始化问题 |
| 双方合理但处于不同画面 | 检查输入、save、随机状态和帧计数 |
| 画面结构一致，仅颜色不同 | 分离 raw index、mapped index 和 final RGB |

所有结论都应写清“画面合理性”和“像素一致性”是两种证据。

## 7. 分层定位

视频路径至少分三层：

```text
PPU raw palette index
        |
        v
PPU model / NES DB LUT mapped index
        |
        v
palette + emphasis -> final RGBA8888
```

- `raw_idx` 已不同：继续查 PPU fetch、VRAM、sprite、scroll 或 mapper。
- `raw_idx` 相同、`mapped_idx` 不同：查 VS PPU model 与 LUT。
- mapped 相同、RGB 不同：查调色板、emphasis、grayscale 或输出路径。

不要在 raw index 已一致时继续猜 CPU IRQ；也不要在 final RGB 未对齐时直接改
mapper bank logic。

## 8. CPU sequence 对比

默认构建启用 `EAR6_ENABLE_CPU_SEQ_TRACE` 编译 gate，运行时用环境变量开启：

```bash
EAR6_TRACE_CPU_SEQ=1 \
  ./build/apps/cli/ear6-cli screenshot \
  -f 30 path/to/game.nes -o /dev/null \
  2>/tmp/ear6-cpu-seq.txt
```

Mesen2 的对应 trace：

```bash
MESEN2_TRACE_CPU_SEQ=1 \
  ../mesen2/dist/x86_64-PC-Linux/bin/mesen2-cli screenshot \
  -f 30 path/to/game.nes -o /dev/null
```

Mesen2 trace 当前写入 `/tmp/mesen2_cpu_seq.txt`。不同平台的 dist 目录可能不同，
先定位 `mesen2-cli` 实际产物。

双方行格式应包含：

```text
f sl cy cpu pc op a x y sp ps
```

先按帧比较指令条数，再规范化 emulator prefix 和必要的绝对 cycle offset。不能
一开始删除 PC、opcode、寄存器或 PPU frame/scanline/cycle；这些正是首差异证据。

CPU trace 的判断：

- 首个不同是 load 后寄存器值：追踪该内存地址的 owner 和 read handler。
- PC/opcode 分叉：向前找首次 status 或读取值不同。
- CPU sequence 完全一致但画面不同：停止修改 CPU/PRG banking，转向 PPU、CHR、
  palette 或 frame output。
- 指令序列一致但 PPU cycle 坐标有固定偏移：检查 reset 和 CPU/PPU phase。

## 9. PPU 与内存事件

只在 CPU sequence 无法解释差异时增加更窄的 trace。优先复用当前已有 gate：

| Runtime gate | 用途 |
|---|---|
| `EAR6_TRACE_NESDB` | NES DB 命中和覆盖字段 |
| `EAR6_TRACE_CPU_SEQ` | 每条 CPU 指令状态 |
| `EAR6_TRACE_REG_WRITES` | PPU register writes，可按 frame 限制 |
| `EAR6_TRACE_TILE_INDEX` | tile/raw index，可按 frame/scanline 限制 |
| `EAR6_TRACE_TILE_FETCH` | nametable/attribute/pattern fetch |
| `EAR6_TRACE_PIXEL_PROBE` | 指定 frame/x/y 的像素路径 |
| `EAR6_TRACE_PALETTE` | raw/mapped/RGB 与 palette dump；需要对应编译 gate |

某些专用 trace 还受 `EAR6_ENABLE_*` 编译宏保护。开启环境变量没有输出时先检查
源代码 gate 和 CMake 定义，不要以为执行路径没有发生。

新增 trace 必须：

- 默认关闭
- 能按 frame/scanline/address 缩小输出
- 使用稳定字段，便于双方脚本规范化
- 不改变模拟状态或额外读取有副作用的寄存器
- 在问题完成后保留为通用 gate，或删除一次性噪音

## 10. 从首差异追到代码

每个根因结论需要两种证据：

1. 运行证据：可重复命令和首差异点。
2. 代码证据：Ear6 与 Mesen2 对应函数、条件和状态更新顺序。

常见地址 owner：

| 范围 | 优先检查 |
|---|---|
| `$0000-$1FFF` | internal RAM mirror |
| `$2000-$3FFF` | PPU registers 和 register mirror |
| `$4000-$4015` | APU、DMA |
| `$4016-$4017` | controller、扩展输入、open bus |
| `$4020-$5FFF` | mapper/FDS/扩展设备 |
| `$6000-$7FFF` | work/save RAM、mapper registers |
| `$8000-$FFFF` | PRG ROM、mapper registers、bus conflict |

如果内存读取不同，先确认 `NesMemoryManager` 最终注册的是哪个 handler；如果 CHR
数据不同，确认 `BaseMapper` 的 page pointer、memory type 和 access flag；如果地址
不同，检查 PPU `v/t/x/w`、延迟状态和 fetch cycle。

## 11. NES DB 与 CRC

当 header mapper 和最终 mapper 不同，先判断 NES DB 是否命中。Ear6 当前按 ROM
header/trainer 之后的剩余内容计算 CRC，并用嵌入数据库覆盖 mapper 等字段。

常见误区：

- 只对 header 声明的 PRG+CHR 长度计算 CRC，忽略尾部数据。
- 双方使用不同版本 NES DB。
- 把错误 DB 条目当成核心真值。
- 修改 mapper 逻辑去适配一个错误 hash 条目。

数据库是元数据参考，不是不可质疑的输出 oracle。若一方因为错误 mapper 渲染
异常，而修正条目后双方合理且一致，应修正数据并记录证据。

## 12. 输入与 save 导致的假差异

CPU/PPU 完全正确时，以下状态仍可让画面分叉：

- Mesen2 自动加载 `.sav`
- 标准 controller 与 Family BASIC keyboard/Zapper 等设备不同
- `$4017` 的设备位与 open bus 组合不同
- Start/coin 输入发生在不同帧
- VS DIP switch、coin 或 PPU model 不同

在修改核心前，先用 CPU trace 找到分叉是否来自 `$4016/$4017` 或 `$6000-$7FFF`。

## 13. Mapper 工作流

新增或修正 mapper 时使用 [Mapper 实现清单](mapper_checklist.md)。最小交付包括：

1. factory 支持和正确 variant 选择。
2. 初始化 PRG/CHR/mirroring/RAM。
3. register、IRQ、hook 和 expansion audio（如有）。
4. 至少一个能进入有效画面的 ROM 对比。
5. 首差异定位或列定帧 100% 证据。
6. 永久回归测试。
7. `docs/compatibility/nes.md` 记录。

Mapper source file 存在只说明可创建，不说明行为完整。

## 14. 修改 Mesen2

只有现有 CLI/trace 无法暴露首差异时才修改参考仓库。修改前：

- 记录 Mesen2 commit 和工作区状态。
- 先找到对应代码路径，不做大范围重构。
- 新日志默认关闭，并使用 `MESEN2_*` gate。
- 保持字段与 Ear6 trace 一致。
- 使用 `make cli -C ../mesen2/DesktopApp` 重建。

Mesen2 的临时 instrumentation 不应混入 Ear6 提交，也不能把未重建的旧二进制
输出当成新代码证据。

## 15. 回归范围

一次修复至少检查：

- 首差异帧
- 一个较晚累计帧（通常 60、128 或 256）
- 同 mapper 的其他本地 ROM
- 共享基类/family 的已知 100% ROM
- `./build/ear6-test` 中相关测试

像素 100% 结果记录 sampled ROM 和 frame。只有覆盖所有相关 ROM、启动与游戏路径、
bank/IRQ/submapper 分支后，才讨论更强的“mapper 完成”结论。

## 16. 记录模板

在 `docs/compatibility/nes.md` 中使用以下信息结构：

```markdown
## Mapper N

- ROMs: X
- Frames: 1/30/60/128/256
- Perfect: X/Y
- Partial: X/Y
- None: X/Y

| ROM | Match | Visual classification | First difference |
|---|---:|---|---|
| `game.nes` | 99.90% | both valid, local sprite drift | frame 11 |

Evidence: CPU sequence matches through ..., first PPU difference is ...
Permanent regression: test name and frame.
```

100% 结果也必须记录。没有差异不等于没有信息，它能保护已完成路径并确定后续
改动的回归边界。
