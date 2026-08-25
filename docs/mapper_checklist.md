# NES Mapper 实现清单

本清单适用于新增 mapper、合并 mapper family 或修正 submapper。它只描述当前
Ear6 架构；兼容性结论写入 [nes-issue.md](../nes-issue.md)。

## 1. 先确认实际 mapper

```bash
./build/app/cli/ear6-cli info assets/nes/rom/mapper_N/game.nes
```

CLI 显示 header 元数据。运行时还可能被内嵌 NES DB 按 CRC 覆盖，因此同时查看
加载日志中的最终 `[NES] Mapper:`。与 Mesen2 比较时必须确认双方最终 mapper、
submapper、PRG/CHR 大小、battery、mirroring 和板卡/chip 信息一致。

不要仅按文件所在目录或文件名判断 mapper。

## 2. 选择实现边界

- 独立硬件使用独立 `MapperNNN` 类。
- 共享寄存器布局和 IRQ/audio 核心的 mapper family 复用一个实现，并在 `init()`
  中按 `info.mapper_number`、`info.submapper_id` 和 NES DB 元数据选 variant。
- 只有确实相同的硬件才能别名到已有 mapper；记录 Mesen2 对应类和选择条件。
- 同步更新 `mapper_factory.cpp` 的 `is_supported()` 与 `create()`，避免一个列表支持
  而另一个遗漏。

## 3. 初始化内存

每个 mapper 的 `init()` 都要核对：

- PRG page size、启动 bank 和固定末尾 bank
- CHR ROM/CHR RAM page size、启动 bank 和写权限
- `set_mirroring_type(info.mirroring)` 或板卡规定的固定 mirroring
- four-screen/额外 nametable RAM
- work RAM、save RAM、trainer 和 battery 行为
- bus conflict 是否来自 NES DB 或板卡默认值

`BaseMapper` 默认的 mirroring 枚举值不会替 mapper 建立正确 nametable 映射。
必须显式调用 `set_mirroring_type()`。

不要盲用组合 page helper。核对 slot 与 page 算术；遇到特殊 page size 时，使用
多个明确的 `select_prg_page()`/`select_chr_page()` 往往更容易验证。

## 4. 注册读写范围

- 用 `add_register_range()` 精确声明读、写或双向寄存器。
- 普通 PRG/WRAM 访问交给 `BaseMapper::read_ram()`/`write_ram()`；不要因为 mapper
  有寄存器就吞掉 RAM 写入。
- register read 的 open bus 位必须与 Mesen2 一致。
- 低地址寄存器、扩展音频和保护寄存器要确认是否覆盖 APU/输入设备范围。
- 动态取消范围时用 `remove_register_range()`，并验证 memory handler 映射更新。

## 5. Bank 与 mirroring 写入

对每一个 register bit 建表核对：

| 项目 | 必查内容 |
|---|---|
| PRG | bank mask、外层 bank、mode、固定 bank、负索引语义 |
| CHR | ROM/RAM 来源、1x/2x/4x/8x slot 算术、latch |
| Mirroring | horizontal/vertical/single-screen/four-screen 的切换条件 |
| RAM | enable、write protect、battery 与 open bus |
| Bus conflict | 写入值与当前 PRG ROM 数据的 AND 时机 |

优先逐行对应 Mesen2 逻辑，不根据某一个画面反推寄存器含义。

## 6. 时钟、IRQ 和 PPU hook

- CPU clock mapper 实现 `process_cpu_clock()`，并让 `has_cpu_clock_hook()` 返回
  true。
- A12/PPU 地址型 IRQ 使用 `notify_vram_address_change()`，由 PPU 总线地址变化的
  单一路径触发。
- 核对 counter 宽度、prescaler、reload、acknowledge、enable delay 和 Rev A/B
  差异。
- expansion audio 的时钟域、寄存器写入和 mixer delta 必须分别验证。
- reset 要区分 soft/hard reset，并初始化每个自定义状态。

Ear6 当前 CPU 在每个读写周期推进 PPU master clock。不要重新引入“执行整条指令
后批量推进 PPU”的路径。

## 7. 元数据与特殊设备

检查 header 与 NES DB 对以下字段的覆盖关系：

- mapper/submapper
- board、chip 与 bus conflict
- PRG/CHR RAM、save RAM 和 battery
- mirroring 与 VS PPU model
- 输入设备

如果 CPU trace 首帧就完全不同，先排查最终 mapper 和 CRC 口径。不要为了匹配一份
错误数据库条目在 mapper 中加入 ROM 名称或 hash 特判。

## 8. 验证顺序

1. mapper 创建成功，没有 fallback、崩溃或卡死。
2. reset vector 和前几十条 CPU sequence 与 Mesen2 一致。
3. frame 1、首个非空帧、30、60、128、256 截图比较。
4. 找到首差异帧，而不是只看最后一帧百分比。
5. 先比较 CPU sequence；CPU 一致后再进入 PPU/raw index。
6. 对两幅图做视觉分类：一方合理、一方乱码，或双方合理但局部不同。
7. 修复后重跑同 mapper 全部本地 ROM，检查共享基类回归。
8. 添加最小永久回归，并更新 `nes-issue.md`。

详细命令见 [NES 迁移与对比指南](migration_guide.md)。

## 9. 结果记录

每条 `nes-issue.md` 结果至少包含：

- mapper 与 ROM 数
- 测试帧号
- perfect/partial/none 数量或精确像素比例
- header mapper 与 NES DB 最终 mapper 的差异
- 是真实 ROM 覆盖还是人为 mapper probe
- 双方画面是否合理
- 永久测试覆盖的具体 ROM 和帧

“100%”只能表示已列 ROM 的已列帧像素一致，不能自动扩大为所有 ROM、所有帧、
所有 bank 或整个 mapper 完成。
