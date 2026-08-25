# Ear6 文档

Ear6 是一个跨平台、多系统的游戏模拟器库。NES 是当前最成熟的系统实现，
但项目边界不是“NES 模拟器”：应用只依赖统一的 C API，系统专有能力通过独立
扩展头文件提供。

本文档按读者和任务组织。第一次阅读时不需要从头读到尾。

## 阅读路线

### 使用 Ear6

1. [快速开始](getting-started.md)：构建、运行 CLI 和浏览器版本
2. [Public API 手册](api-reference.md)：在 C/C++ 应用中创建、驱动和销毁模拟器
3. [Save State 格式](state-format.md)：理解 `.e6s` 容器、预览和系统 payload
4. [架构与系统模型](architecture.md)：理解通用 API 与系统扩展的边界

### 开发 Ear6

1. [开发指南](development.md)：目录、构建、测试和提交要求
2. [项目路线图](TODO.md)：当前能力、近期优先级和完成标准
3. [NES 迁移与对比指南](migration_guide.md)：与 Mesen2 做逐帧、逐指令和逐周期比较
4. [Mapper 实现清单](mapper_checklist.md)：新增或修正 NES mapper 时的检查项
5. [NES 兼容性结果](../nes-issue.md)：已经验证的 ROM、mapper 和已知差异
6. [Save State 格式](state-format.md)：修改 state、系统 dump 或宿主持久化前必读

### 前端

- [Web UI 设计约束](web_ui_design.md)：浏览器界面的布局、性能指标和构建信息

## 当前系统状态

| 系统 | `Ear6SystemType` | 状态 | 专用头文件 |
|---|---|---|---|
| Test | `EAR6_SYSTEM_TEST` | 已实现，用于 API/渲染管线自检 | 无 |
| NES | `EAR6_SYSTEM_NES` | 已实现，持续做 Mesen2 精确性验证 | `<ear6/nes.h>` |
| Flash | `EAR6_SYSTEM_FLASH` | API 占位，核心尚未实现 | `<ear6/flash.h>` |

状态含义以代码和 [项目路线图](TODO.md) 为准。NES mapper 的“存在实现”、
“有回归测试”和“与 Mesen2 完全一致”是三个不同层级，不能互相替代。

## 文档维护规则

- `<ear6/ear6.h>` 是跨系统公开契约的事实来源。
- `<ear6/<system>.h>` 只描述对应系统的扩展能力。
- `docs/TODO.md` 记录项目级能力和下一步，不重复维护逐 ROM 结果。
- `nes-issue.md` 记录 NES 实测结果，包括 100% 一致和已知差异。
- 行为、命令或公开 API 改动必须在同一个提交中更新相应文档。
