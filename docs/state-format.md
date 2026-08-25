# Ear6 Save State 格式

Ear6 save state 使用 `.e6s` 扩展名。它是自包含的整机快照，包含原始 content、
机器状态和可选预览图。它不等同于游戏硬件的 battery-backed save RAM。

格式分为两层：外层是所有系统共享的 Ear6 容器，内层是 NES、Flash 等系统独占的
opaque machine dump。两层独立演进，修改 Flash payload 不会改变 NES state 版本。

## 版本边界

state 有三条独立的版本轴：

| 版本 | 所有者 | 何时变化 |
|---|---|---|
| container wire version | Ear6 通用层 | preamble 或 protobuf framing 不兼容变化 |
| preview version | 通用 preview 消息 | 当前 preview 图像语义不兼容变化 |
| system state version | NES、Flash 等各自实现 | 对应系统 machine dump 布局变化 |

向 protobuf 消息增加 optional 字段不升级 container wire version。NES 和 Test payload
version 当前为 `1`，只由对应系统的 `save_state()` / `load_state()` 解释。

## Container Preamble

文件以 32 字节 preamble 开始，所有整数均为 little-endian：

| Offset | 类型 | 字段 | 规则 |
|---:|---:|---|---|
| 0 | `char[8]` | magic | `EAR6STAT` |
| 8 | `u32` | wire version | 当前为 `1` |
| 12 | `u32` | preamble size | 当前为 `32` |
| 16 | `u64` | body size | 必须等于文件剩余长度 |
| 24 | `u32` | body CRC32 | 覆盖完整 protobuf body |
| 28 | `u32` | reserved | 必须为 `0` |

`EAR6_STATE_CONTAINER_WIRE_VERSION` 只描述外层 framing。当前 loader 不读取旧的固定头
state，也不提供旧 state 的兼容迁移。

## Protobuf Body

preamble 后是 `ear6.state.StateContainer` 的 protobuf 二进制编码。schema 位于
`src/core/state/state_container.proto`：

```proto
message StateContainer {
  required uint32 system_type = 1;
  required fixed64 content_identity = 2;
  optional string content_name = 3;
  required bytes content = 4;
  required bytes system_state = 5;
  optional Preview preview = 6;
}
```

| ID | 字段 | 含义 |
|---:|---|---|
| 1 | `system_type` | 稳定的 `Ear6SystemType` 数值 |
| 2 | `content_identity` | 原始 content 的 FNV-1a 64 |
| 3 | `content_name` | UTF-8 basename，不含路径、query 或 fragment |
| 4 | `content` | 原始 ROM、SWF 或其他输入；允许零长度 |
| 5 | `system_state` | 由目标 system 独占解释的 opaque bytes |
| 6 | `preview` | 可选预览消息 |

loader 使用 protobuf 的 required-field 检查。未知字段被忽略，字段顺序无关，重复
标量按 protobuf 规则采用最后一个值。writer 使用 deterministic encoding，使同一
模拟状态产生稳定字节序列。

system type 编号一旦发布不得复用。content identity 用于索引和发现误配，CRC32 用于
发现意外损坏；两者都不是防篡改的密码学认证。

## Preview

Preview 是独立的 protobuf 子消息：

```proto
message Preview {
  optional uint32 version = 1;
  optional Format format = 2;
  optional uint32 width = 3;
  optional uint32 height = 4;
  optional bytes image_data = 5;
}
```

当前有效组合是 `version = 1`、`format = FORMAT_RGBA8888`，且 `image_data` 必须恰好
包含 `width * height * 4` 字节。它与公共 framebuffer 契约一致。

Preview 只供宿主显示缩略图，不参与机器恢复。消息缺字段、版本未知、格式未知或尺寸
错误时，loader 忽略 preview，仍可加载 system state。这样新增 preview 编码不会迫使
旧版本 Ear6 放弃可兼容的机器快照。

## System State

通用容器不读取 `system_state` 的内部布局。当前 payload 概念上如下：

```text
NES system state:
    u32 nes_state_version
    u32 mapper_number
    palette, CPU, PPU, APU, mapper, RAM, ...

Flash system state:
    u32 flash_state_version
    Flash-specific machine dump, ...
```

某个系统是否读取自己的旧 payload 由该系统决定；当前策略是不兼容读取未知 system
state version。

## 加载事务

`ear6_load_state_from_memory()` 按以下顺序执行：

1. 校验 preamble、body 长度和 CRC。
2. 解码 protobuf，检查 required 字段并重新计算 content identity。
3. 创建与目标 context 同类型的临时 system。
4. 在临时 system 中加载内嵌 content。
5. 把 opaque system state 交给该 system 的 loader。
6. 全部成功后替换当前 system。

任何一步失败都不会改变调用者现有模拟状态。optional metadata 不参与机器恢复。

## 实现与依赖

Ear6 使用 protobuf 36.0 的 C `upb` runtime。仓库只保留当前生成代码实际依赖的
arena、message、mini-table、wire encode/decode 和 utf8_range 文件，约 0.8 MB；不包含
C++ protobuf runtime、reflection、JSON、text format、compiler 或 Abseil。

生成的 `.upb.c/.h` 文件提交在 `src/core/state/`，正常 native/WASM 构建不要求本机安装
`protoc`。上游版本、许可证和裁剪范围记录在
`third_party/protobuf-36.0/README.ear6.md`。

## 宿主持久化

核心只提供内存 buffer，不选择路径或槽位。Desktop、Web 或第三方宿主负责：

- 使用 `.e6s` 扩展名；
- 原子写入或事务式更新；
- 每个 content 的槽位策略；
- 保存时间、排序和当地时区显示；
- 导入、导出、配额和错误提示。

当前官方宿主采用单槽策略，key 都是 `system_type + content_identity`：

- Desktop 写入 `QStandardPaths::AppDataLocation/states/`，文件名为
  `<system>-<identity>.e6s`，通过 `QSaveFile` 原子覆盖；Load Save 菜单扫描并验证
  目录内 state。
- Web 写入 `localStorage` 的 `ear6.save.v1.<system>.<identity>` key；JSON record
  保存 `.e6s` 的 base64、ROM 名、保存时间和菜单 PNG 缩略图。Load 菜单按当地时间
  展示，`.e6s` 导入/导出仍作为显式操作保留。

因此同一 content 的新 state 覆盖旧 state，不同 content 独立保存。这里的单槽策略是
宿主产品约定，不进入 public API 或 protobuf container。

state 内含原始 content，没有压缩或加密。它具有与原始 ROM/SWF 相同的版权、隐私
和分发属性。
