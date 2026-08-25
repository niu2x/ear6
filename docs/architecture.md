# 架构与系统模型

Ear6 的核心产品是模拟器库，不是某一个前端，也不局限于某一台游戏机。
当前开发深度集中在 NES，但公共边界从一开始就按多系统设计。

## 分层

```text
宿主应用（CLI / Desktop / Web / 第三方应用）
                    |
                    v
      <ear6/ear6.h> 通用 C API
                    |
             Ear6 不透明上下文
                    |
          内部 ear6::System 接口
             /       |       \
         Test       NES      future systems
                     |
             <ear6/nes.h> 扩展
```

### 宿主层

宿主负责文件选择、主循环、帧显示、音频播放和输入采集。`app/cli/`、
`app/desktop/` 和 `app/web/` 都是库的消费者，不应绕过 Public API 直接访问
NES 内部对象。

### 通用 API 层

`include/ear6/ear6.h` 提供所有系统共有的生命周期：

- 按 `Ear6SystemType` 创建一个不透明上下文
- 从路径或内存加载内容
- 把版本化整机状态保存到调用者内存，或从调用者内存恢复
- 每次推进一帧
- 通过拉取或回调取得 RGBA8888 视频和 16-bit PCM 音频

同名函数对所有系统必须具有相同语义。新增系统不能把系统专有枚举、配置或
设备概念塞入这个头文件。

### 系统扩展层

系统专有能力放在独立头文件中。例如 `<ear6/nes.h>` 定义 NES 区域、手柄按钮
和调色板接口。未来系统遵循同样模式：

```text
include/ear6/ear6.h       通用生命周期和媒体输出
include/ear6/nes.h        NES 专有配置与输入
include/ear6/flash.h      Flash 专有配置（当前为占位）
include/ear6/<system>.h   未来系统扩展
```

### 核心实现层

`src/system.h` 中的内部 C++ `ear6::System` 接口承接公共 C ABI。具体系统实现
负责把自己的原生帧格式转换为统一 RGBA8888，并把音频暴露为 PCM。C++ 类型、
异常和内部对象所有权都不能越过 `extern "C"` 边界。

## 一帧的生命周期

```text
宿主更新系统输入
       |
       v
  ear6_step(ctx)
       |
       +--> 系统推进到下一帧边界
       +--> 更新内部 framebuffer/audio packet
       +--> 同步调用 frame callback（若已设置）
       +--> 同步调用 audio callback（若有音频）并消费该音频包
       |
       v
宿主显示帧或用 pull API 读取媒体数据
```

上下文拥有所有返回缓冲区。宿主只能读取，不能释放；指针的有效期和音频消费
规则详见 [Public API 手册](api-reference.md)。

Save state 的系统 payload 由各系统实现序列化；通用层负责系统类型、内容身份、
原始内容、名称、当前 framebuffer 预览图、长度和校验。核心不选择文件路径，也不
管理存档槽位。宿主可以把同一内存 buffer 写入文件、数据库或云端。battery-backed
save RAM 属于游戏硬件持久化，不等同于整机 save state。

state 是自包含快照，会嵌入 ROM。恢复时通用层创建临时系统，加载内嵌内容并校验
identity，再恢复系统 payload，全部成功后才替换当前系统。这使浏览器下载的网络
ROM 能在以后只凭 state 恢复，也保证损坏 state 不会留下半加载的游戏。代价是 state
体积和分发属性包含 ROM 本身；压缩、加密、槽位和文件管理仍属于宿主层。

保存时通用层同时复制系统当前可见的 RGBA8888 framebuffer。它是独立于系统 payload
的只读预览图，供桌面或 Web 的存档选择器在不启动游戏的情况下显示缩略图；预览图
本身不参与恢复模拟状态。没有视频输出的未来系统可以不提供预览。

公共容器使用 protobuf body 保存 `Ear6SystemType` 和通用 metadata。未知字段可跳过，
schema 中的 required 字段缺失时拒绝加载。container wire、preview message 和每个
system payload 各自拥有版本边界，因此增加 optional 外壳字段或修改 Flash payload
都不要求 NES state 升级。完整 wire layout 和演进规则见
[Save State 格式](state-format.md)。

## 当前系统

### Test

Test 系统生成固定尺寸的动态 RGBA 图案，不需要 ROM，也不产生音频。它用于验证
ABI、渲染上传和宿主主循环，不是游戏系统。

### NES

NES 系统包含 6502 CPU、PPU、APU、输入管理、iNES/NES 2.0 加载、内嵌 NES DB
覆盖以及 mapper 工厂。CPU 每个读写周期推进 PPU master clock，mapper、APU 和
输入挂在 CPU 时钟与内存分发表上。

“mapper 工厂可以创建”不表示所有游戏路径已经验证。实测覆盖和差异统一记录在
[NES 兼容性结果](../nes-issue.md)。

### Flash

`EAR6_SYSTEM_FLASH` 和 `<ear6/flash.h>` 目前只保留 API 方向，核心未实现。
`ear6_create(EAR6_SYSTEM_FLASH)` 返回 `nullptr`。在实现完成前，宿主不能把枚举
存在误判为运行时支持。

## 添加新系统

新增系统至少包含以下工作：

1. 实现内部 `ear6::System` 接口，并把系统原生输出转换到公共媒体格式。
2. 在 `create_system()` 中接入新的 `Ear6SystemType`。
3. 把系统专有配置放入新的 `<ear6/<system>.h>`，保持纯 C ABI。
4. 为每个 C API 入口验证上下文类型并捕获所有 C++ 异常。
5. 在 CLI/Desktop/Web 中显式决定检测、选择和不支持时的表现。
6. 增加生命周期、无效输入、帧格式、音频消费和重复创建销毁测试。
7. 更新本文、API 手册、路线图和安装清单。

## 设计约束

- 一个 `Ear6*` 在整个生命周期内只对应一个系统类型；切换系统要销毁并重建。
- `ear6_step()` 的公共单位是一帧，内部系统可以使用任意周期模型。
- framebuffer 始终是紧密排列的 RGBA8888；不能把调色板索引或系统原生像素暴露
  给通用 API。
- 公共头文件必须可被 C 编译器包含；C++ 只存在于实现侧。
- 系统扩展必须拒绝错误类型的上下文，不能依赖不安全的强制转换。
- 原生和 WASM 应共享同一核心数据来源；NES DB 因此在构建时嵌入库中。
