# Ear6 路线图

更新基线：2026-08-25。

本文件记录项目级能力和下一步。NES 的逐 ROM/mapper 实测结果不在这里重复维护，
统一查看 [NES 兼容性结果](compatibility/nes.md)。

状态定义：

- 已实现：代码路径存在，并有至少一个直接测试或实际消费者
- 已验证：在已写明的输入和范围内与参考结果一致
- 完成：满足明确的发布标准，而不是“有一个类/文件”

## 当前基线

### 已有能力

- [x] 纯 C 的多系统公共边界：`<ear6/ear6.h>`
- [x] 系统扩展头边界：`<ear6/nes.h>`、`<ear6/flash.h>`
- [x] Test 系统：用于 framebuffer 和宿主循环自检
- [x] NES 系统：iNES/NES 2.0 加载、CPU、PPU、APU、输入和 mapper 工厂
- [x] 通用 RGBA8888 framebuffer 输出
- [x] 16-bit PCM 回调和拉取式输出
- [x] 原生共享库、CLI、Qt 宿主、WASM 桥接和浏览器宿主
- [x] NES DB 构建时嵌入，原生/WASM 共用数据
- [x] NES CPU 周期驱动 PPU master clock，并通过内存 handler 分发表路由设备
- [x] NES PRG/CHR 页表、work/save RAM 内存区和 submapper 元数据
- [x] NES 基础 APU 和部分扩展音频（VRC6、Namco 163、Sunsoft 5B）
- [x] 自包含的版本化内存 save state：嵌入原始内容，覆盖 Test 系统及全部受支持
      NES mapper，含内容身份、CRC、原子恢复和连续运行测试
- [x] 多个 mapper 的固定帧回归与 Mesen2 抽样对比

### 系统成熟度

| 系统 | 创建 | 加载 | 视频 | 音频 | 系统扩展 | 发布状态 |
|---|---:|---:|---:|---:|---:|---|
| Test | 是 | 是 | 是 | 无 | 不需要 | 开发工具 |
| NES | 是 | 是 | 是 | 是 | 部分 | 开发中 |
| Flash | 否 | 否 | 否 | 否 | 仅声明 | 未实现 |

## P0：稳定 Public API

这些项目在把 0.1.x API 推荐给第三方长期集成前必须完成。

- [ ] 处理已声明但无实现符号的 `ear6_nes_set_region()`：实现并测试，或在破坏性
      版本中移除声明
- [ ] 处理已声明但无实现符号的 `ear6_nes_set_mapper()`，并明确它是调试覆盖还是
      正式加载配置
- [ ] Flash 核心实现前，处理无实现符号的 `ear6_flash_set_version()`
- [ ] 定义公开错误枚举，替代文档化但未命名的 `-1/-2/-3`
- [ ] 增加音频格式查询：sample rate、channel count、sample format
- [ ] 明确并测试 `ear6_load_from_memory()` 的空指针、负 size、超大输入和输入缓冲区生命周期
- [ ] 为 framebuffer 和音频指针有效期建立 API contract 测试
- [ ] 增加安装后 consumer 测试：纯 C 编译、C++ 编译和 `find_package(Ear6)` 链接
- [ ] 决定 `ear6_test()` 是正式诊断 API 还是移除的早期兼容符号
- [ ] 定义线程与回调重入策略；当前只支持宿主串行访问一个上下文

## P0：NES 正确性

兼容性优先级以 [NES 兼容性结果](compatibility/nes.md) 的最新证据为准。当前最明确的
开放差异包括：

- [ ] Mapper 4：处理多个 ROM 的局部/完全差异与 frame-phase 差异
- [ ] Mapper 45/shared CHR-RAM：定位 CPU trace 一致后的首个 PPU 差异
- [ ] Mapper 7：定位 `Battletoads Double Dragon` 的局部像素差异
- [ ] Mapper 23：定位 `Ganbare Goemon 2` 从 frame 11 开始的 sprite 时序差异
- [ ] 对只有单 ROM、单帧或 mapper probe 的 100% 结果扩展覆盖，避免把探针成功
      表述为 mapper 完成
- [ ] 对 mapper factory 中尚无 `docs/compatibility/nes.md` 证据的实现逐个建立基线

通用硬件差距：

- [ ] FDS RAM adapter、寄存器、磁盘和扩展音频
- [ ] PAL/Dendy 的 CPU、PPU、APU 帧时序与公共配置路径
- [ ] battery-backed save RAM 的宿主持久化 API
- [ ] Zapper、Four Score、Arkanoid controller、Power Pad 等设备选择 API
- [ ] VS System/DualSystem 的 coin、DIP switch 和双机设备模型
- [ ] DMC DMA 与 sprite DMA 并发、cycle stealing 和边缘时序验证
- [ ] PPU open bus decay、`$2004/$2007` 边缘行为和首帧访问限制验证
- [ ] mapper submapper、bus conflict、IRQ 和扩展音频的系统化覆盖

## P1：多系统平台

- [ ] 定义“系统实现可用性”查询，避免宿主通过枚举猜测支持状态
- [ ] 把 CLI 内容检测从 NES magic 扩展为可注册的系统探测器
- [ ] 让 Desktop/Web 的系统选择、输入映射和媒体配置不依赖 NES 常量
- [ ] 设计第二个真实系统实现，用它验证通用 API 是否真的系统无关
- [ ] 明确每个系统的内容格式、区域/时钟、输入设备和持久化扩展头
- [ ] 建立原生与 WASM 对同一输入的帧/音频一致性测试

Flash 是否作为第二个真实系统继续实现，需要先形成核心范围、文件格式、帧推进
语义和安全模型；在此之前它只是保留的 API 方向。

## P1：工具与自动化

- [ ] 把 ear6/Mesen2 批量截图、像素统计和首差异帧查找收敛为仓库脚本
- [ ] 为 CPU sequence trace 增加规范化和自动首差异报告
- [ ] 增加 raw index、mapped index、final RGB 三层自动比较
- [ ] 对本地 ROM 集生成 mapper/ROM/帧覆盖清单，区分 skipped 与 passed
- [ ] 把 100% 结果和差异报告的固定字段自动校验进 `docs/compatibility/nes.md`
- [ ] 增加 sanitizers 和长时间创建/加载/销毁压力测试

## P2：用户能力

- [ ] battery-backed 游戏 save RAM 的导入、导出和安全落盘
- [x] Desktop/Web 宿主的 save state 单槽持久化、覆盖和 Load Save 选择菜单
- [ ] 可配置控制器与多玩家输入
- [ ] 调试器：CPU 寄存器、内存、PPU viewer、断点和 trace
- [ ] cheat/Game Genie
- [ ] NSF 播放
- [ ] 视频滤镜、缩放策略和可选 NTSC 模拟

这些能力不得污染通用 API。只有跨系统共有的语义进入 `ear6.h`；系统硬件概念
进入各自扩展头，宿主产品功能留在 app 层。

## 文档完成标准

- [x] 根 README 作为简洁入口
- [x] 按用户、API 使用者和核心开发者组织的文档目录
- [x] 多系统架构和扩展边界说明
- [x] Public API 参数、错误、所有权、回调和示例手册
- [x] 构建、测试、安装和开发指南
- [x] 当前路线图与 NES 兼容性实测分离
- [ ] 每次 Public API 或系统支持变化时，在同一个提交中更新文档
