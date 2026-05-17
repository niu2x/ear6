# NES PPU Migration Guide: ear6 <-> Mesen2

## 警告：必须先读

本项目的 ear6 PPU 实现存在大量细微但致命的 bug。Mesen2 是精确到 PPU clock cycle 级别的模拟器。**任何看起来"无关紧要"的细节差异都会导致游戏无法正确渲染**。此前的实现者忽略了许多 Mesen2 中的关键细节，导致：
- 黑屏（nametable 选择错误）
- 调色板完全未写入（$2006/$2007 序列被 batch 模型破坏）
- 颜色渲染错误（透明像素调色板处理错误）
- 调色板镜像错误

**迁移原则：逐行对照 Mesen2，不要自己发明。**

---

## 1. 执行模型差异：Interleaved vs Batch

## 0. 调试方法论（先于任何个例）

本节总结了近期迁移中“走弯路”的共同原因，并给出统一流程。目标是：
**尽快逼近真相，避免拍脑袋猜 CPU/PPU/IRQ**。

### A. 先分层，再定位

任何画面不一致，先拆成三层比较：

1. `raw_idx`（PPU 原始索引）
2. `mapped_idx`（LUT 后索引）
3. `final_rgb`（最终截图）

如果 `raw_idx` 已一致，就不要继续猜 CPU/IRQ；优先查 LUT/调色板/后处理路径。

### B. 双证据原则（必须同时满足）

每个结论都必须同时有：

1. **日志证据**：可复现实验命令 + 对比结果（百分比、首个差异点）
2. **代码证据**：明确指出 mesen2 与 ear6 的对应代码路径与语义差异

没有两者，不允许下“根因结论”。

### C. 默认先排除“输出路径噪音”

在比较 core 行为前，先固定输出口径：

- 禁用或旁路后处理（filter/scanline/rotate/scale）
- 对齐“取帧时机”（frame gating）
- 对齐 PPU model / NES DB / LUT 语义

否则会出现“core 已一致但截图不一致”的假分叉。

### D. 单次迭代最小闭环

每次只做一个小变更，然后立即回归：

1. `frame 11`（首差异帧）
2. `frame 60`（累计误差帧）

并记录：
- 匹配率变化
- 首差异坐标是否后移/消失
- 差异层级是否变化（raw -> mapped -> rgb）

### E. 迁移中的常见误区（已验证）

1. 把“颜色异常”直接归因于 CPU/IRQ（常错）
2. 忽略 NES DB 与 PPU model 对 VS 标题的影响
3. 用最终截图直接判 core，没先拆 raw/mapped/rgb
4. 在重日志状态下信任时序结果（可能被日志本身扰动）

### F. 推荐起手顺序（以后固定执行）

1. 固定 deterministic 运行 + 最小日志
2. 比 `raw_idx`/`mapped_idx`/`rgb`
3. 若 `raw` 不同：查 PPU 时序/寄存器状态
4. 若 `raw` 相同但 `rgb` 不同：查 LUT/palette/后处理路径
5. 仅在前两步无法解释时，才深入 CPU/IRQ/DMA

### G. Trace 开关基线（避免“日志干扰”与口径不一致）

ear6 调试日志统一采用**两级开关**：

1. 编译期开关：`EAR6_ENABLE_*`
2. 运行期开关：`EAR6_TRACE_*`

两者必须同时启用才会输出日志。常用组合：

- `EAR6_ENABLE_CPU_TRACE` + `EAR6_TRACE_CPU`
- `EAR6_ENABLE_PPU_TRACE` + `EAR6_TRACE_PPU`
- `EAR6_ENABLE_CPU8448_TRACE` + `EAR6_TRACE_CPU8448`
- `EAR6_ENABLE_MINIMAL_BAD_WINDOW_TRACE` + `EAR6_TRACE_MINIMAL_BAD_WINDOW`
- `EAR6_ENABLE_CYCLE_ALIGN_TRACE` + `EAR6_TRACE_CYCLE_ALIGN`
- `EAR6_ENABLE_EARLY_FRAME_SUMMARY_TRACE` + `EAR6_TRACE_EARLY_FRAME_SUMMARY`
- `EAR6_ENABLE_DMARIO_TRACE11` + `EAR6_TRACE_DMARIO11`
- `EAR6_ENABLE_PALETTE_TRACE` + `EAR6_TRACE_PALETTE`
  - 可选：`EAR6_TRACE_PALETTE_FRAME`, `EAR6_TRACE_PALETTE_DUMP_PREFIX`

建议：做像素对比时默认关闭所有 trace，只在定位阶段开启最小必要日志组。

### Mesen2（正确的）
```
CPU cycles 和 PPU cycles 是交织的，每个 PPU clock 单独执行：
Exec() {
    _cycle++;
    if (_cycle < 340) {
        ProcessScanlineImpl();  // 处理当前 cycle
    } else {
        ProcessScanlineFirstCycle();  // cycle 0 的整理工作
    }
    UpdateState();  // 处理延迟状态更新
}
```

### ear6（当前错误的 batch 模型）
```cpp
while (ppu_cycles < PPU_CYCLES_PER_FRAME) {
    cpu_->exec();          // 执行一个 CPU 指令
    ppu_->run(cpu_cycles * 3);  // 批量运行 3x CPU cycles 的 PPU
}
```

**关键问题**：`$2006` 写入后，PPU 批量运行时 `v_` 被渲染逻辑修改。当紧接着的 `$2007` 写入时，`v_` 已经是错误的值。

**修复方案**：使用独立的 `vram_access_addr_` 跟踪 CPU 寄存器访问地址，与渲染用的 `v_` 分离。每次 `$2006` 设置地址时同步两者，`$2007` 读写使用 `vram_access_addr_`。

```cpp
// set_addr （第二个写入）
v_ = t_;
vram_access_addr_ = v_;  // 保存寄存器访问地址

// set_data
write_vram(vram_access_addr_, value);  // 使用独立地址
vram_access_addr_ += (ctrl_ & 0x04) ? 32 : 1;
v_ = vram_access_addr_;  // 同步回 v_

// get_data
result = data_buffer_;
data_buffer_ = read_vram(vram_access_addr_);
vram_access_addr_ += (ctrl_ & 0x04) ? 32 : 1;
v_ = vram_access_addr_;
```

### 理想方案（完全匹配 Mesen2）

改为 cycle-interleaved 模型，不要 batch。每次 CPU 执行后只运行固定数量的 PPU cycles（而不是按 CPU instruction 消耗的周期成倍运行）。

---

## 2. `$2006` PPUADDR 写入延迟

### Mesen2
```cpp
// 第二个写入后，延迟 3 PPU cycles 才实际复制到 _videoRamAddr
_updateVramAddrDelay = 3;
_updateVramAddr = _tmpVideoRamAddr;
_needStateUpdate = true;
```

延迟结束后在 `UpdateState()` 中复制，并处理 "scroll glitch"：
- cycle 257：AND corruption
- cycle 8 的倍数：partial AND corruption
- 其他：正常复制

### ear6（当前）
```cpp
t_ = (t_ & 0xFF00) | value;
v_ = t_;   // 立即复制，没有延迟
w_ = false;
```

**必须实现 3-cycle 延迟**，否则：
- 某些游戏在渲染期间写入 `$2006` 时，地址会被 `inc_horizontal`/`inc_vertical` 破坏
- 扫描线边界处的写入会选中错误的 nametable

---

## 3. 水平/垂直滚动复制（Cycle 257 和 280-304）

### Mesen2
```cpp
// Cycle 257: 从 t 复制水平滚动到 v（每个扫描线）
_videoRamAddr = (_videoRamAddr & ~0x041F) | (_tmpVideoRamAddr & 0x041F);

// Cycles 280-304: 从 t 复制垂直滚动到 v（仅预渲染线）
if(_scanline == -1 && _cycle >= 280 && _cycle <= 304) {
    _videoRamAddr = (_videoRamAddr & ~0x7BE0) | (_tmpVideoRamAddr & 0x7BE0);
}
```

**掩码含义**：
- `0x041F` = coarse X (bits 0-4) + nametable X (bit 10)
- `0x7BE0` = fine Y (bits 12-14) + coarse Y (bits 5-9) + nametable Y (bit 11)

### ear6 之前的错误
```cpp
// 错误！cycle 257 用了垂直掩码
v_ = (v_ & 0x841F) | (t_ & 0x7BE0);
```

**后果**：nametable X 永远不会被重置，每个扫描线都读到错误的 nametable，产生黑屏。

**修复**：
```cpp
// Cycle 257: 复制水平位
v_ = (v_ & 0x7BE0) | (t_ & 0x041F);
// Cycles 280-304: 复制垂直位（原代码的 0x841F 掩码在这里是正确的）
v_ = (v_ & 0x841F) | (t_ & 0x7BE0);
```

---

## 4. 调色板镜像（Palette Mirroring）

### Mesen2 的 WritePaletteRam
```cpp
if(addr == 0x00 || addr == 0x10) {
    _paletteRam[0x00] = value;   // 双向镜像：写任一个，两个都写
    _paletteRam[0x10] = value;
} else if(addr == 0x04 || addr == 0x14) {
    _paletteRam[0x04] = value;
    _paletteRam[0x14] = value;
} else if(addr == 0x08 || addr == 0x18) {
    _paletteRam[0x08] = value;
    _paletteRam[0x18] = value;
} else if(addr == 0x0C || addr == 0x1C) {
    _paletteRam[0x0C] = value;
    _paletteRam[0x1C] = value;
} else {
    _paletteRam[addr] = value;
}
value &= 0x3F;  // NES palette 只有 6-bit
```

### ear6 之前的错误
```cpp
if ((addr & 0x13) == 0x10) addr &= 0x0F;  // 单向！只映射 0x10→0x00
palette_ram_[addr] = value;  // 只写了一个位置
```

**差异**：Mesen2 是**双向**镜像 — 写 `$10` 同时更新 `$00` 和 `$10`。ear6 只更新单向。这会影响 sprite 调色板的颜色值。

---

## 5. 透明像素的调色板处理

### Mesen2 的 GetPixelColor + DrawPixel
```cpp
// GetPixelColor 返回 palette_offset + pixel_value（0-3 或 4/8/12+1-3）
return paletteOffset + backgroundColor;

// DrawPixel 中检查 pixel 是否为 0
uint32_t color = GetPixelColor();
_currentOutputBuffer[...] = _paletteRam[color & 0x03 ? color : 0];
```

关键：`color & 0x03` 检查 pixel_value 是否为 0。如果是 0，总是使用 backdrop（`palette_ram[0]`），忽略 attribute palette。

### ear6 之前的错误
```cpp
bg_color = (palette << 2) | (hi_bit << 1) | lo_bit;
// 当 pixel=0 时，bg_color = palette_offset（非零！）
// 导致 palette_ram_[palette_offset] 被使用，而不是 palette_ram_[0]
```

**后果**：透明像素读取了错误的子调色板颜色。

**修复**：
```cpp
uint8_t pixel = (hi_bit << 1) | lo_bit;
if (pixel) {
    uint8_t palette = (bg_attr_hi_ << 1) | bg_attr_lo_;
    bg_color = (palette << 2) | pixel;
}
// pixel=0 时 bg_color 保持为 0 → backdrop
```

---

## 6. $2007 PPUDATA 写入屏蔽

### Mesen2
```cpp
case PpuRegisters::VideoMemoryData:
    if((_ppuBusAddress & 0x3FFF) >= 0x3F00) {
        WritePaletteRam(_ppuBusAddress, value);  // 调色板写入不受屏蔽
    } else {
        if(_scanline >= 240 || !IsRenderingEnabled()) {
            _mapper->WriteVram(_ppuBusAddress & 0x3FFF, value);
        } else {
            // 渲染期间写入被忽略！写入地址的 LSB 代替
            _mapper->WriteVram(_ppuBusAddress & 0x3FFF, _ppuBusAddress & 0xFF);
        }
    }
```

### ear6（当前）
```cpp
// 无条件写入！渲染期间也写入真实值
write_vram(v_, value);
```

**必须实现**：在可见扫描线（0-239）且渲染启用时，屏蔽 nametable 写入。

---

## 7. vblank 读取清除 write toggle

### Mesen2
```cpp
case PpuRegisters::Status:
    // 读取 $2002 会清除 write toggle
    _writeToggle = false;
    ...
```

### ear6（已正确）
```cpp
case 0x02:
    status_ &= 0x7F;
    w_ = false;  // 正确
    ...
```

这个逻辑是正确的，但要注意：**NMI handler 中读取 $2002 也会清除 w_**。如果游戏在 `$2006` 两次写入之间触发了 NMI，NMI handler 读取了 `$2002`，就会破坏 `$2006` 写入序列。

---

## 8. 精灵评估和加载

### 评估时机
- Mesen2：在 cycle 257-320 期间逐步评估 64 个精灵
- ear6：在 cycle 257 一次性调用 `evaluate_sprites()`，扫描所有 64 个条目

Mesen2 的精灵评估更复杂，涉及 OAM 地址起始、溢出 bug、secondary OAM 清零等。ear6 的简化实现可能对有复杂精灵场景的游戏产生问题。

### 精灵 Tile 加载
- Mesen2：在 cycle 260+ 期间逐步加载精灵 tile（每 8 个 cycle 一个）
- ear6：在 cycle 257 一次性调用 `load_sprite_tiles()`

### 必须检查的细节
1. **OAMADDR 在渲染期间被重置为 0**（Mesen2 在 cycle 257-320 期间设置 `_spriteRamAddr = 0`）
2. **精灵 0 命中检测**：Mesen2 有额外检查：
   - `_cycle != 256`（x=255 时不会命中）
   - `_mask.BackgroundEnabled`（背景必须启用）
   - `!_statusFlags.Sprite0Hit`（只触发一次）
3. **溢出标志**：检测到第 9 个精灵时设置，但 Mesen2 有更复杂的溢出 bug 模拟

---

## 9. 渲染启用的 1-cycle 延迟

### Mesen2
```cpp
// UpdateState 中检查
if(_prevRenderingEnabled != _renderingEnabled) {
    // 渲染状态改变有一个 cycle 的延迟
    _prevRenderingEnabled = _renderingEnabled;
    ...
}
```

ear6 当前在每个 cycle 都检查 `mask_` 的变化并立即更新 `rendering_enabled_`。**必须改为 1-cycle 延迟**。

---

## 10. 扫描线/cycle 编号

### Mesen2
- 复位：`_scanline = -1, _cycle = 340`
- scanline -1 = 预渲染行
- scanline 0-239 = 可见行
- scanline 240-260 = VBlank
- 下一个 scanline 261 → 回到 -1

### ear6
- 复位：`scanline_ = 261, cycle_ = 0`
- scanline 261 = 预渲染行
- scanline 0-239 = 可见行
- scanline 240-260 = VBlank
- 下一个 scanline 261 → 回到 261

两种编号方式等价（偏移 262）。**关键是 cycle 0 的处理**：
- Mesen2：cycle 0 在 `ProcessScanlineFirstCycle()` 中处理（整理工作，不渲染）
- ear6：cycle 0 不做任何特殊处理

---

## 11. 必须实现的 Mesen2 功能清单

### 核心功能（必须实现，否则游戏不工作）

| 功能 | 文件 | 状态 |
|------|------|------|
| `$2006` 3-cycle 延迟 | `NesPpu.cpp` UpdateState | ❌ 缺失 |
| PPUADDR 延迟中的 scroll glitch | `NesPpu.cpp` UpdateState | ❌ 缺失 |
| Cycle 257 水平复制掩码正确 | `NesPpu.cpp` ProcessScanlineImpl | ✅ 已修复 |
| Cycles 280-304 垂直复制 | `NesPpu.cpp` ProcessScanlineImpl | ✅ 正确 |
| 调色板双向镜像 | `NesPpu.cpp` / BaseNesPpu.cpp WritePaletteRam | ✅ 已修复 |
| 透明像素 backdrop 处理 | `DefaultNesPpu.h` DrawPixel | ✅ 已修复 |
| `vram_access_addr_` 分离 | `NesPpu.cpp` set_data/get_data | ✅ 已修复 |
| `$2007` 渲染期间写入屏蔽 | `NesPpu.cpp` VideoMemoryData | ❌ 缺失 |
| 渲染启用 1-cycle 延迟 | `NesPpu.cpp` UpdateState | ❌ 缺失 |
| 值 mask 到 6-bit（`value &= 0x3F`） | `BaseNesPpu.cpp` WritePaletteRam | ⚠️ 部分 |

### 次要功能（影响兼容性）

| 功能 | 文件 | 状态 |
|------|------|------|
| NMI handler 中的 `_preventVblFlag` | `NesPpu.cpp` UpdateStatusFlag | ❌ 缺失 |
| OAM 地址在 cycle 257-320 重置为 0 | `NesPpu.cpp` ProcessScanlineImpl | ❌ 缺失 |
| 精灵评估的 cycle 精确实现 | `NesPpu.cpp` ProcessSpriteEvaluation | ❌ 缺失 |
| OAM corruption（渲染开启时的硬件 bug） | `NesPpu.cpp` ProcessOamCorruption | ❌ 缺失 |
| OAM decay | `BaseNesPpu.h` | ❌ 缺失 |
| 预渲染线上的 sprite 加载 dummy $FF | `NesPpu.cpp` ProcessScanlineFirstCycle | ❌ 缺失 |
| 奇帧 cycle 跳过（NTSC） | `NesPpu.cpp` ProcessScanlineImpl | ❌ 缺失 |
| PAL 支持（260 vs 310 scanlines） | `NesPpu.cpp` UpdateTimings | ❌ 缺失 |
| Grayscale 和 intensify bits | `BaseNesPpu.cpp` UpdateGrayscaleAndIntensifyBits | ❌ 缺失 |
| `_paletteRamMask` 应用 | `BaseNesPpu.h` | ❌ 缺失 |
| Open bus decay | `BaseNesPpu.h` | ❌ 缺失 |

---

## 12. 关键参考代码位置

```
Mesen2/Core/NES/NesPpu.cpp:
  - Exec()           :1331  - 主执行循环
  - ProcessScanlineImpl() :868  - 每个 PPU cycle 的渲染逻辑
  - LoadTileInfo()   :667  - 背景 tile 读取管线
  - GetPixelColor()  :817  - 像素合成（背景+精灵）
  - LoadSpriteTileInfo() :805  - 精灵 tile 读取
  - ProcessSpriteEvaluationStart() :959  - 精灵评估开始
  - ProcessSpriteEvaluationEnd() :979  - 精灵评估结束
  - UpdateState()    :1421 - 延迟状态更新（渲染启用、$2006、$2007）
  - UpdateVideoRamAddr() :201  - VRAM 地址增量
  - IncHorizontalScrolling() :622 - 水平滚动增量
  - IncVerticalScrolling() :597   - 垂直滚动增量
  - ProcessScanlineFirstCycle() :1368 - cycle 0 处理

Mesen2/Core/NES/BaseNesPpu.h:
  - _videoRamAddr (v)
  - _tmpVideoRamAddr (t)
  - _xScroll (x)
  - _writeToggle (w)
  - _lowBitShift / _highBitShift
  - _spriteRam[0x100]
  - _secondarySpriteRam[0x20]
  - _paletteRam[0x20]
  - _tile (LowByte, HighByte, PaletteOffset, TileAddr)
  - _currentTilePalette / _previousTilePalette

Mesen2/Core/NES/DefaultNesPpu.h:
  - DrawPixel()      :24   - 最终像素输出

Mesen2/Core/NES/BaseNesPpu.cpp:
  - WritePaletteRam  :83   - 调色板写入（镜像逻辑）
  - IsRenderingEnabled :58 - 渲染启用判断
```

## 14. 完整架构：类间关系

### Mesen2 对象关系图

```
NesConsole
  |-- _cpu  (NesCpu)             -> has _memoryManager, _console
  |-- _ppu  (BaseNesPpu*)        -> has _mapper, _console, _emu
  |-- _apu  (NesApu)             -> has _console
  |-- _memoryManager (NesMemoryManager)  -> _ramReadHandlers[65536],
  |                                            _ramWriteHandlers[65536]
  |-- _mapper (BaseMapper)       -> has _console, _emu, _{prg,chr}Pages[256]
  |-- _controlManager (NesControlManager)
  +-- _mixer (NesSoundMixer)
```

所有子系统的构造函数都接收 `NesConsole*`，通过 `console->GetXxx()` 互相访问。

### ear6 当前的对象关系（需要重构）

```
NesConsole
  |-- cpu_ (NesCpu)          -> has console_
  |-- ppu_ (NesPpu)          -> has console_ (通过 console_->get_cpu/mapper)
  |-- apu_ (NesApu)          -> has console_
  +-- mapper_ (BaseMapper)
```

关键缺失：
- ear6 没有 `NesMemoryManager`，CPU 地址路由直接在 `NesConsole::cpu_read/cpu_write` 中用 if-else 硬编码
- ear6 的 `run_frame()` 是 batch 模型，不是 cycle-interleaved

---

## 15. CPU 地址路由（必须改为 dispatch table）

### Mesen2

Mesen2 使用 **两个 65536 条目的直查表** 来路由 CPU 读写：

```cpp
INesMemoryHandler** _ramReadHandlers;   // [0x10000]
INesMemoryHandler** _ramWriteHandlers;  // [0x10000]
```

每个 device 在初始化时注册自己的地址范围：

```cpp
// NesMemoryManager 构造函数：全部初始化为 OpenBusHandler
for (int i = 0; i < CpuMemorySize; i++) {
    _ramReadHandlers[i] = &_openBusHandler;
    _ramWriteHandlers[i] = &_openBusHandler;
}
RegisterIODevice(_internalRamHandler.get());  // $0000-$1FFF

// NesConsole::LoadRom 中：
_memoryManager->RegisterIODevice(_ppu.get());            // $2000-$3FFF, $4014
_memoryManager->RegisterIODevice(_apu.get());            // $4000-$4015, $4017
_memoryManager->RegisterIODevice(_controlManager.get()); // $4016-$4017
_memoryManager->RegisterIODevice(_mapper.get());         // $4020-$FFFF
```

每个 device 实现 `INesMemoryHandler` 接口：

```cpp
class INesMemoryHandler {
    virtual void GetMemoryRanges(MemoryRanges &ranges) = 0;
    virtual uint8_t ReadRam(uint16_t addr) = 0;
    virtual void WriteRam(uint16_t addr, uint8_t value) = 0;
};
```

CPU 读写直接查表，O(1)，无需任何 decode 逻辑：

```cpp
uint8_t NesMemoryManager::Read(uint16_t addr, ...) {
    return _ramReadHandlers[addr]->ReadRam(addr);
}
void NesMemoryManager::Write(uint16_t addr, uint8_t value, ...) {
    _ramWriteHandlers[addr]->WriteRam(addr, value);
}
```

### ear6（当前，需要重写）

```cpp
// NesConsole::cpu_read 用 if-else 逐段判断：
if (addr < 0x2000) { return wram_[addr & 0x7FF]; }
if (addr < 0x4000) { return ppu_read(0x2000 + (addr & 0x7)); }
if (addr < 0x4020) { return apu_read(addr); }
if (mapper_) { return mapper_->read(addr); }
return 0;
```

**问题**：
- 每个读操作都要执行 3-4 次分支判断（慢 + 无法缓存）
- 没有考虑 mirroring（比如 $2008 应该 mirror 到 $2000）
- `$4014` OAMDMA 写被放在这里，但对 PPU 来说它是另一个地址

### CPU 地址空间对照

| 地址范围 | Mesen2 所有者 | ear6 所有者 | 说明 |
|----------|---------------|-------------|------|
| `$0000-$1FFF` | InternalRamHandler | `wram_[addr & 0x7FF]` | ear6 需要处理 mirroring |
| `$2000-$3FFF` | NesPpu | NesPpu (if-else) | 每 8 字节重复，ear6 用 `addr & 0x7` 可以 |
| `$4000-$4013` | NesApu | NesApu | 正确 |
| `$4014` | NesPpu (OAM DMA) | NesConsole (特殊处理) | ear6 在 cpu_write 中处理 |
| `$4015` | NesApu | NesApu | 正确 |
| `$4016` | NesControlManager | ❌ 未实现 | 控制器读/写 |
| `$4017` | NesControlManager (读) / NesApu (写) | ❌ 未实现 | 控制器读、APU 帧序列器 |
| `$4018-$401F` | 通常 open bus 或 APU | ❌ 未实现 | |
| `$4020-$FFFF` | BaseMapper | BaseMapper | 正确 |

---

## 16. CPU-PPU 交织执行模型（最关键）

### Mesen2：cycle-interleaved

每次 CPU 访问内存时，PPU 都会同步推进：

```cpp
// 在 NesCpu 的每次 memory_read/memory_write 内部：
StartCpuCycle(forRead) {
    _masterClock += ...;
    _state.CycleCount++;
    _console->GetPpu()->Run(_masterClock - _ppuOffset);  // PPU 追赶到当前 master clock
}

EndCpuCycle(forRead) {
    _masterClock += ...;
    _console->GetPpu()->Run(_masterClock - _ppuOffset);  // PPU 再次追赶
}
```

PPU 的 `Run(runTo)` 内部循环执行 `Exec()`：

```cpp
template<class T>
void NesPpu<T>::Run(uint64_t runTo) {
    do {
        Exec();                     // 一个 PPU dot
        _masterClock += _masterClockDivider;  // NTSC=4, PAL=5
    } while (_masterClock + _masterClockDivider <= runTo);
}
```

每个 `Exec()`：
1. 递增 `_cycle`（1-340）
2. 对可见扫描线调用 `ProcessScanlineImpl()`
3. 对 cycle 340 调用 `ProcessScanlineFirstCycle()`（推进扫描线）
4. 调用 `UpdateState()`（处理延迟更新）

### ear6：batch 模型（必须改）

```cpp
void NesConsole::run_frame() {
    while (ppu_cycles < PPU_CYCLES_PER_FRAME) {
        cpu_->exec();                     // 执行完整指令（可能消耗 2-7 CPU cycles）
        int cpu_cycles = cpu_cycles_diff;
        ppu_->run(cpu_cycles * 3);        // PPU 批量运行 3x CPU cycles
        ppu_cycles += cpu_cycles * 3;
    }
}
```

**致命问题**：PPU 批量运行时，CPU 不执行任何代码。这意味着：

1. **$2006 → $2007 时序破坏**：`$2006` 写入后 v_ 被渲染修改，`$2007` 写入错误地址（已在 ear6 中用 `vram_access_addr_` 打补丁）
2. **中断响应延迟**：PPU 可能在批量运行时到达 vblank 并设置 NMI flag，但 CPU 要到批量结束后才看到
3. **寄存器写覆盖**：如果 CPU 在同一条指令内写多个 PPU 寄存器，PPU 批量运行会破坏中间状态

**必须改为 cycle-interleaved 模型**，让 PPU 每条指令的每次内存访问后都同步推进。

---

## 17. 通过 Mapper 的 PPU VRAM 访问路径

### Mesen2

PPU 不直接访问任何内存。所有 VRAM 读写都通过 mapper：

```cpp
// NesPpu::ReadVram
uint8_t NesPpu::ReadVram(uint16_t addr, MemoryOperationType type) {
    SetBusAddress(addr);                  // 更新 _ppuBusAddress，通知 MMC3 IRQ 计数器
    return _mapper->ReadVram(addr, type); // → BaseMapper::InternalReadVram
}

// BaseMapper::InternalReadVram
__forceinline uint8_t InternalReadVram(uint16_t addr) {
    if (_chrMemoryAccess[addr >> 8] & MemoryAccessType::Read) {
        return _chrPages[addr >> 8][(uint8_t)addr];  // 256 条目的页表直接解引用
    }
    return addr;  // 不可读时返回地址 LSB（open bus）
}
```

mapper 内部维护两张页表：

```cpp
uint8_t* _prgPages[0x100];  // CPU 地址 $8000-$FFFF 的 256 页（每页 256 字节）
uint8_t* _chrPages[0x100];  // PPU 地址 $0000-$3FFF 的 256 页
MemoryAccessType _chrMemoryAccess[0x100];
ChrMemoryType _chrMemoryType[0x100];  // ChrRom / ChrRam / NametableRam / MapperRam
```

页表初始化逻辑：

```cpp
void BaseMapper::SelectChrPage(int slot, uint8_t page, MemoryAccessType type) {
    SetPpuMemoryMapping(slot * 0x400, (slot + 1) * 0x400 - 1, page * 0x400, type);
}
```

Nametable 通过 `SetPpuMemoryMapping` 映射到 `_chrPages`：

```cpp
void BaseMapper::SetNametable(uint8_t index, uint8_t nametableIndex) {
    SetPpuMemoryMapping(0x2000 + index * 0x400, 0x2000 + (index + 1) * 0x400 - 1,
                        nametableIndex, ChrMemoryType::NametableRam);
    // $3000-$3FFF 镜像
    SetPpuMemoryMapping(0x3000 + index * 0x400, 0x3000 + (index + 1) * 0x400 - 1,
                        nametableIndex, ChrMemoryType::NametableRam);
}

void BaseMapper::SetMirroringType(MirroringType type) {
    switch(type) {
        case Vertical:   SetNametables(0, 1, 0, 1); break;
        case Horizontal: SetNametables(0, 0, 1, 1); break;
        case FourScreens: SetNametables(0, 1, 2, 3); break;
        case ScreenAOnly: SetNametables(0, 0, 0, 0); break;
        case ScreenBOnly: SetNametables(1, 1, 1, 1); break;
    }
}
```

### ear6 当前（需要重写）

```cpp
uint8_t NesPpu::read_vram(uint16_t addr) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return mapper->read_chr(addr);  // 通过 mapper 读 CHR
    }
    if (addr < 0x3F00) {
        uint8_t nt_index = (addr >> 10) & 3;
        uint8_t* nt = mapper->get_nametable(nt_index);
        return nt[addr & 0x3FF];       // ear6 直接索引 nametable
    }
    return palette_ram_[addr & 0x1F];
}
```

### ear6 mapper 当前模型（需要重写）

```cpp
// 当前：固定 1KB chr_pages_[8]，硬编码 8 个页
uint8_t* chr_pages_[8] = {};
uint16_t get_chr_page_size() { return 0x400; }  // 默认 1KB 每页

uint8_t read_chr(uint16_t addr) {
    uint8_t slot = (addr >> 10) & 0x7F;  // 用 bits 10-16 做 slot 索引
    uint16_t offset = addr & (get_chr_page_size() - 1);
    if (slot < 8 && chr_pages_[slot]) {
        return chr_pages_[slot][offset];
    }
    return 0;
}
```

**必须改为 256 条目的页表**，才能支持：
- 1KB、2KB、4KB、8KB 混合页大小
- MMC1/MMC3/MMC5 等复杂 mapper 的任意页映射
- Nametable RAM 统一通过页表访问
- 地址的读写权限单独控制

---

## 18. 必须实现的 Mesen2 核心机制

### NesMemoryManager（新增类）

```
class NesMemoryManager {
    INesMemoryHandler* _ramReadHandlers[0x10000];
    INesMemoryHandler* _ramWriteHandlers[0x10000];

    void RegisterIODevice(INesMemoryHandler* device);
    uint8_t Read(uint16_t addr);
    void Write(uint16_t addr, uint8_t value);
};
```

- 将 `NesConsole::cpu_read/cpu_write` 中的 if-else 逻辑替换为查表
- 将 PPU 寄存器、APU、控制器、mapper 全部注册为 `INesMemoryHandler`

### BaseMapper 页表系统

```cpp
// 将 ear6 的固定 8 页改为 256 页
uint8_t* chr_pages_[0x100] = {};
MemoryAccessType chr_memory_access_[0x100];
ChrMemoryType chr_memory_type_[0x100];

// 页选择函数改为通用的
void select_chr_page_1k(int slot, uint8_t page);
void select_chr_page_2k(int slot, uint8_t page);
void select_chr_page_4k(int slot, uint8_t page);
void select_chr_page_8k(uint8_t page);
void set_ppu_memory_mapping(uint16_t start, uint16_t end, uint8_t page, ChrMemoryType type);

// VRAM 读写统一走页表
uint8_t read_vram(uint16_t addr);   // 取代 ear6 的 read_chr
void write_vram(uint16_t addr, uint8_t value);

// Nametable 管理
void set_mirroring_type(MirroringType type);
void set_nametable(uint8_t index, uint8_t nametable_index);
```

### NesPpu::ReadVram/WriteVram 改为走 mapper

```cpp
// PPU 端：
uint8_t read_vram(uint16_t addr) {
    return console_->get_mapper()->read_vram(addr);
}
```

### CPU-PPU 交织

将 `NesConsole::run_frame()` 改为：

```cpp
void NesConsole::run_frame() {
    uint32_t frame = ppu_->get_frame_count();
    while (frame == ppu_->get_frame_count()) {
        cpu_->exec();
    }
}
```

CPU 的每次 `memory_read`/`memory_write` 内部都需要调用 `console_->get_ppu()->run()`。

---

## 19. 实施路线图

### Phase 1：基础设施（不改渲染逻辑）
1. 新增 `NesMemoryManager` 类，实现 dispatch table
2. 让 PPU、APU、mapper 实现 `INesMemoryHandler`
3. 用 dispatch table 替换 `cpu_read/cpu_write` 的 if-else

### Phase 2：Mapper 页表系统
1. 将 `chr_pages_[8]` 扩展为 `chr_pages_[0x100]`
2. 实现 `set_ppu_memory_mapping`、`read_vram`、`write_vram`
3. 将 nametable 映射改为通过页表
4. 重写现有 mapper 使用新的页表 API

### Phase 3：Cycle-interleaved 执行
1. 在 CPU 的每次内存访问中加入 `console_->get_ppu()->run()` 调用
2. 删除 ear6 的 batch 模型
3. 实现 `$2006` 3-cycle 延迟（`UpdateState`）
4. 实现 `$2007` 增量延迟（`_needVideoRamIncrement`）

### Phase 4：PPU 细节
1. 实现 `$2007` 渲染期间写入屏蔽
2. 实现渲染启用 1-cycle 延迟
3. 实现精灵评估的 cycle 精确版本
4. 实现 OAM corruption
5. 实现奇帧 cycle 跳过

---

## 13. 测试清单

### 单 ROM 截图对比流程（ear6 vs mesen2）

当你要验证某一个 NES ROM 的迁移结果时，**不要主观判断画面**，直接分别运行 ear6-cli 和 mesen2-cli 截图，再比较结果。

1. 先编译两个 CLI（只允许这两种编译方法）：

```bash
make
make cli -C ../mesen2/DesktopApp
```

2. 对同一个 ROM 在 60 帧和 120 帧分别截图：

```bash
./build/app/cli/ear6-cli screenshot <rom.nes> -f 60 -o /tmp/ear6_60.ppm
./build/app/cli/ear6-cli screenshot <rom.nes> -f 120 -o /tmp/ear6_120.ppm

../mesen2/dist/x86_64-PC-Linux/bin/mesen2-cli screenshot <rom.nes> -f 60 -o /tmp/mesen2_60.ppm
../mesen2/dist/x86_64-PC-Linux/bin/mesen2-cli screenshot <rom.nes> -f 120 -o /tmp/mesen2_120.ppm
```

3. 分别比较 60 帧和 120 帧截图是否一致：

```bash
cmp /tmp/ear6_60.ppm /tmp/mesen2_60.ppm
cmp /tmp/ear6_120.ppm /tmp/mesen2_120.ppm
```

`cmp` 无输出表示两个文件完全一致；有输出表示存在差异。

4. 若对比不一致，进入细节调试：
- 可以分别修改 ear6 和 mesen2 代码，在关键路径加日志（PPU 寄存器写入、scanline/cycle、mapper 写入等）。
- 修改后重新编译：ear6 用 `make`，mesen2 用 `make cli -C ../mesen2/DesktopApp`。
- 重新运行同一组截图与比较命令，持续对齐日志与像素结果，直到 60/120 帧都一致。

每次修改后，用以下 ROM 测试：

1. **nestest.nes** — CPU 指令集正确性
2. **blargg_ppu_tests** — PPU 功能测试（palette, sprite, vblank, scroll）
3. **Super Mario Bros.** — 综合功能测试
4. **Donkey Kong** — 简单 sprite 测试
5. **SMB3** — 复杂 mapper + PPU 交互测试

测试命令：
```bash
# 不输出调试信息
./build/app/cli/ear6-cli -f 120 <rom> -o /tmp/test.ppm 2>/dev/null
# 用 xxd 检查输出颜色
xxd /tmp/test.ppm | head -20
```

---

## 21. 修改 Mesen2 用于对照调试（构建与编辑规范）

当你需要在 Mesen2 中加 trace 做对照时，遵循以下规则，避免不必要的大规模重编译：

1. **优先只改 `.cpp`，尽量不改 `.h`**。
   - Mesen2 头文件变更会触发大范围重编译，调试迭代会明显变慢。

2. **Mesen2 CLI 只允许这条构建命令**：
   - `make cli -C ../mesen2/DesktopApp`

3. **不要删除 Mesen2 的 build 目录**。
   - 全量重建代价很高（通常 >10 分钟），会打断迁移排查节奏。

4. **需要强制重编某个文件时，只删对应单个 `.o` 文件**。
   - 例如：
   ```bash
   rm -f build-Release/x86_64-PC-Linux/MesenRT/CMakeFiles/MesenLib.dir/Core/NES/NesPpu.cpp.o
   make cli -C ../mesen2/DesktopApp
   ```
   - 这样 CMake 会仅重编目标文件并重链接。

5. **若必须改 `.h`（如 `SettingTypes.h`）**：
   - 尽量控制改动范围，只触发必要的 `.cpp` 级联重编；
   - 若无法避免，接受这次构建会变慢。

### 补充调试原则（避免误判）

- **先做代码路径对照，再做日志推断**：先确认 Mesen2/ear6 对应代码语义，再用日志验证，不靠日志“猜行为”。
- **区分“观测改动”和“行为改动”**：可先加 trace 收集证据，证据未闭环前不要同时做语义修复。
- **NES DB 视为权威元数据**：mirroring/input/PPU model 等覆盖要做字段级映射与边界检查。
- **NES DB 加载方式**：ear6 不在运行时读取外部 DB 文件。构建阶段会把 `assets/nes/nes_db.txt` 生成并嵌入为 `build/generated/nes_db_embedded.h`，`apply_nesdb_overrides()` 直接解析内嵌文本（兼容 wasm）。
- **分层定位**：RGB 不一致 -> raw index -> 寄存器/事件时间线 -> 内存读写来源。
- **修复后做定向回归**：除目标 ROM 外，至少回归 1-2 个同类 mapper 的已知正常 ROM。

## 20. 迁移教训与反思

### 2026-05-15：MMC1 写入时序守卫 — 批量模型 vs 交织模型的潜藏差异

**Bug：部分 Mapper 001 游戏 CHR bank 配置错误，画面异常。**
如 AD&D Hillsfar (J).nes，ear6 输出完全错误，mesen2 正确。

**根因：MMC1 `WriteRegister` 的 dummy-write 过滤器不生效。**

MMC1 通过 5 位串行协议配置寄存器：
- CPU 的 RMW 指令（`INC`/`DEC`/`ASL`/`LSR`/`ROL`/`ROR`）在写回修改值之前
  会先写回**原始值**（dummy write）。
- 如果 dummy write 的 bit 7 被置位（原始值恰为 `0x80-0xFF`），
  MMC1 会误触发 RESET，导致移位寄存器清空、模式寄存器回退。

防止机制：**写入时序守卫**——连续两次写入若相隔太近，忽略第二次写入。

**Mesen2（正确）：**
```cpp
// 用 master clock 计数，单位是 PPU dot ÷ 4（NTSC）
uint64_t currentCycle = _console->GetMasterClock();
if ((value & 0x80) || currentCycle - _lastWriteCycle >= 2) {
    ProcessBitWrite(addr, value);
}
_lastWriteCycle = currentCycle;
```
RMW 的两笔写入相隔仅 **1 master clock tick**（`diff=1 < 2`），dummy write 后的真实写入被过滤。

**ear6（错误）：**
```cpp
// 用 CPU cycle 计数
uint64_t current_cycle = console_->get_cpu()->get_cycle_count();
if ((value & 0x80) || current_cycle - last_write_cycle_ >= 1) {
    process_bit_write(addr, value);
}
last_write_cycle_ = current_cycle;
```
RMW 的两笔写入相隔 **1 CPU cycle**（`diff=1 >= 1`），未被过滤。
移位寄存器多接收了 1 bit，使后续全部 5 位串行协议偏移一位。
控制寄存器的 `chr_mode` 被错误设置为 1（4KB 模式），而正确值应为 0（8KB 模式）。

**为什么抄错？**

两个问题叠加。

**问题 1：数值本身不对。** Mesen2 的 guard 是 `>= 2`，ear6 直接写了 `>= 1`。
即使计数器基准完全相同，这两个值也不相等。抄的时候没有逐字核对门槛值。

**问题 2：即使数值对了，不同计数基准下效果也不同。**
Mesen2 用 **master clock**（PPU dot ÷ 4，一个 CPU cycle ≈ 12 master clock ticks），
ear6 用 **CPU cycle**（`state_.cycle_count`）。

| 场景 | Mesen2 master clock diff | ear6 CPU cycle diff |
|------|--------------------------|---------------------|
| RMW dummy write → real write | 1（< 2 → 过滤） | 1（>= 1 → 不过滤） |
| 两个独立 `STA` 指令 | 16+（>= 2 → 通过） | 4+（>= 1 → 通过） |

`>= 1` 在 ear6 的 CPU cycle 尺度上等价于 `>= 12` master clock ticks，
远超 Mesen2 `>= 2` 的容限，因此无法过滤 RMW 的紧挨着写入。

**修复：数值改为 `2` + 在 CPU cycle 基准下用 `>= 2` 等价截断。**
```cpp
if ((value & 0x80) || current_cycle - last_write_cycle_ >= 2) {
```
在 ear6 的 CPU cycle 尺度下：
- RMW 两笔写入 diff = 1 < 2 → **过滤**（匹配 Mesen2）
- 两个独立指令 diff = 4+ >= 2 → **通过**（匹配 Mesen2）

**教训与知识记录：**

1. **先追数值，再追基准。** 第一步应该发现 `>= 2` 和 `>= 1` 不一样。
   第二步再问计数器物理含义。两个问题会同时出现，不能只看一个。

2. **RMW 指令的 dummy write 是所有映射器寄存器的共同陷阱。**
   不光是 MMC1，任何需要在 `$8000-$FFFF` 范围内用写入序列编程的 mapper
   （MMC3、VRC 系列、FME-7 等）都可能受此影响。实现写入时序守卫时，
   必须确保 RMW 内部的两笔写入被过滤，同时保持独立指令间的正常写入。

3. **逐字对照 Mesen2 的魔法数字，不要凭"差不多"的印象写。**
   `>= 2` 就是 `>= 2`，不能写成 `>= 1`。先确保数值一致，
   再考虑不同的计数器基准是否需要换算。

4. **调试技巧：在双方代码同时打印时序守卫日志。**
   ```
   [MMC1W] addr=$FFD7 val=$FF cycle=14 last=0 diff=14 proc=1   // RESET
   [MMC1W] addr=$FFD7 val=$00 cycle=15 last=14 diff=1 proc=0   // 被过滤！
   ```
   这种日志可以直观看到 Mesen2 `diff=1 < 2` 过滤了第二笔写入，
   而 ear6 `diff=1 >= 1` 让它通过了。不需要推测，数据说话。

### 2026-05-15：PPU VBlank 触发位置 — exec() vs ProcessScanlineFirstCycle

**Bug：部分 MMC1 游戏在 VBlank 期间产生多余的 PPU 寄存器写入，渲染异常。**
如 Final Fantasy 2 (J)、Arabian Dream Sharezerd 等，ear6 输出与 mesen2 不同。

**根因：VBlank 触发位置在 `exec()` 中而非 `process_scanline_first_cycle()`。**

Mesen2 在 `ProcessScanlineFirstCycle` 中触发 VBlank：
```cpp
// Mesen2 在 scanline 从 240 切换到 241 时（cycle 340）触发
void ProcessScanlineFirstCycle() {
    _cycle = 0;
    if(++_scanline == 241) {
        BeginVblank();      // 设置 _statusFlags.VerticalBlank = true，触发 NMI
    }
}
```

ear6 原先在 `exec()` 中触发：
```cpp
// ear6 在 scanline 241 的 cycle 1 时才触发
void exec() {
    cycle_++;
    if (cycle_ < 340) {
        if (scanline_ < 240) { ... }
        else if (cycle_ == 1 && scanline_ == nmi_scanline_) {
            begin_vblank();   // 晚了一个 cycle
        }
    } else {
        process_scanline_first_cycle();
    }
}
```

**为什么抄错？**

Mesen2 的 `BeginVblank` 调用在 `ProcessScanlineFirstCycle` 里：
```cpp
void ProcessScanlineFirstCycle() {
    _cycle = 0;
    _scanline++;
    if(_scanline > 260) { ... }
    if(_scanline == 241) {
        BeginVblank();        // ← 在这里
    }
}
```

ear6 的 `process_scanline_first_cycle` 里没有对应代码（只处理了 scanline < 240 和 scanline == 240 的情况），
导致 VBlank 触发被错误地放到了 `exec()` 函数里，且偏移了 1 个 cycle。

**后果：**

VBlank 延迟 1 个 cycle 触发，导致游戏在 VBlank 期间写入 PPU 寄存器（`$2005`/`$2006`/`$2007`）
时的 timing 偏移。在 CHR RAM 游戏中，这会导致：
- 多余的 `$2005`/`$2006` 写入（scroll/address 设置偏移）
- 多余的 `$2007` 写入（CHR RAM 数据偏移）
- 最终画面渲染错误

**修复：将 VBlank 触发从 `exec()` 移到 `process_scanline_first_cycle()`，完全对齐 Mesen2。**

```cpp
void NesPpu::process_scanline_first_cycle() {
    ...
    } else if (scanline_ == 241) {
        if (!prevent_vbl_flag_) {
            status_flags_.vertical_blank = true;
            begin_vblank();
        }
        prevent_vbl_flag_ = false;
    }
}
```

移除了 `exec()` 中的冗余检查。修正后 4 个 MMC1 游戏输出与 mesen2 像素一致。

**教训与知识记录：**

1. **逐函数对照 Mesen2 的结构，不要漏掉任何一个 case 分支。**
   ear6 的 `process_scanline_first_cycle` 只复制了 scanline < 240 和 scanline == 240 的分支，
   完全漏掉了 scanline == 241 的分支。而 Mesen2 的 `ProcessScanlineFirstCycle` 显式处理了
   `_scanline == 241` 的 VBlank 触发。

2. **"看起来结构一样"不等于真的样。** ear6 的 `exec()` 和 mesen2 的 `Exec()` 表面结构相似
  （都调用 process_scanline_impl/ProcessScanlineImpl 和
   process_scanline_first_cycle/ProcessScanlineFirstCycle），
   但 ear6 在 `exec()` 中偷偷塞了一个额外的 VBlank 检查。这个"多余"的代码就是 bug 来源。

3. **PPU 时序相关的移植，应先确保关键事件（VBlank、NMI、$2002 状态）的触发时机对齐。**
   对比双方应同时打日志比对 `$2002` 读取数量及 VBlank 触发的 scanline/cycle，而不是直接比像素。
   像素不同再排查太慢，时序匹配了像素自然就对了。

4. **调试方法：双方同时打印所有 PPU 寄存器写入日志。**
   通过 `diff` 对比双方的 `[PPUW] a=$200X` 日志，可以精确定位多余的寄存器写入发生在哪个
   scanline/cycle。这次就是通过 `diff` 发现 ear6 在 VBlank 区域多了 10 笔 `$2005`/`$2006` 写入
   才追到根因。

### 2026-05-14：三次漏细节的教训

**Bug：uint16_t 回绕死循环**。`add_register_range(0x8000, 0xFFFF, WRITE)` 用
`uint16_t i` 做循环变量，`i++` 到 0xFFFF 后回绕到 0x0000，`i <= 0xFFFF` 永远为真。
加载 mapper 1 的 ROM 时卡在初始化阶段。**C++ 无符号整型回绕是经典陷阱，
但凡看一眼 Mesen2 的参数类型（`int32_t start, int32_t end`）就能避免**。

**Bug：NROM 16KB PRG 越界**。Mesen2 NROM 用 `SelectPrgPage(0,0)` + `SelectPrgPage(1,1)`，
`SelectPrgPage` 内部以 `page % max_page` 做自动 wrap。ear6 直接写
`set_cpu_memory_mapping(0x8000, 0xFFFF, 0)` 一段覆盖 32KB，`source_offset`
在 $C000-$FFFF 段突破 16KB ROM 边界读到垃圾。**对照只看接口、没追到内部实现**。

**Bug：DMA 死循环**。Mesen2 的 `processCycle` lambda 在每轮 DMA 循环开头清
`_needHalt`/`_needDummyRead`，保证 DMC 重新请求后 `need_halt_` 能被清掉、
DMC 能被服务。ear6 抄了 while 框架但漏了这 5 行，`need_halt_` 永远无人清，
DMC 永远不服务→死循环。**漏看了过程性逻辑**。

教训：**"看起来差不多"就是差很多**。迁移时必须逐行对照 Mesen2 源码，
对每一个局部变量、每一个 flag 的读写生命周期都要理解，不能凭印象抄框架。
遇到条件分支多、状态机复杂的函数（`process_pending_dma`、`update_state`），
应该把 Mesen2 源码和 ear6 代码并排打开，逐行核验。
