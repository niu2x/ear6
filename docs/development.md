# 开发指南

本章说明 Ear6 的通用开发流程。NES 与 Mesen2 的专项对比方法放在
[NES 迁移与对比指南](migration_guide.md)。

## 仓库结构

| 路径 | 职责 |
|---|---|
| `include/ear6/` | 安装给使用者的纯 C Public API |
| `src/` | 私有 C/C++ 核心和系统抽象 |
| `src/nes/` | NES CPU、PPU、APU、输入和加载器 |
| `src/nes/mappers/` | NES mapper 基类、工厂、硬件实现和 mapper 专用辅助件 |
| `app/cli/` | 无窗口工具、截图、ROM 信息和音频录制 |
| `app/desktop/` | Qt 桌面宿主 |
| `app/web/` | Emscripten 导出桥接 |
| `app/web-ui/` | 浏览器宿主界面 |
| `tests/` | API 和 ROM 帧回归测试 |
| `assets/nes/` | NES DB 与本地测试 ROM 目录 |
| `docs/` | 用户、API 和开发文档 |

## 常用命令

```bash
make ear6
make ear6-web
make test
make clean
```

只检查核心编译：

```bash
cmake --build build --target ear6
```

运行完整或聚焦测试：

```bash
./build/ear6-test
./build/ear6-test --gtest_filter=Mapper69Regression.*
```

Mesen2 CLI 必须使用它自己的项目入口构建：

```bash
make cli -C ../mesen2/DesktopApp
```

不要从仓库根目录为 Mesen2 猜测另一套 CMake 命令。

## Public API 变更

修改 Public API 时同时检查：

1. 通用行为是否适用于每个 `Ear6SystemType`。
2. 系统专有配置是否放在 `<ear6/<system>.h>`。
3. 头文件是否仍能被 C 编译器使用。
4. C++ 异常是否在每个 `extern "C"` 入口内被转换为错误结果。
5. 新头文件是否同时加入 build include、install include 和 CMake 安装清单。
6. [API 手册](api-reference.md) 的参数、所有权、错误和示例是否同步。
7. 原生、CLI、Desktop 和 Web 桥接是否需要暴露新能力。

## 命名

- 类和类型别名：PascalCase，例如 `NesCpu`、`MapperType`
- 枚举值：UPPER_SNAKE_CASE，例如 `EAR6_SYSTEM_NES`
- 文件或命名空间常量：UPPER_SNAKE_CASE，例如 `DEFAULT_NES_PALETTE`
- 函数和变量：snake_case，例如 `parse_value`
- 非 public 成员：snake_case 加尾部下划线，例如 `data_`
- getter：`get_xxx()`；布尔 getter：`is_xxx()`；setter：`set_xxx()`

## 测试层级

### API 测试

至少覆盖成功路径、无效参数、错误系统类型、重复创建销毁和 `nullptr` 安全行为。
公开契约的改变必须先有对应测试。

### 核心单元测试

对 CPU、PPU、APU 或 mapper 的局部规则使用最小输入。不要仅靠某个游戏“看起来
正常”证明时序正确。

### ROM 回归

固定 ROM、帧号和 framebuffer MD5。ROM 不进入 Git；测试缺失时可以跳过，但提交
说明必须区分“测试通过”和“本机没有执行 ROM 用例”。

### 参考实现对比

NES 当前以 Mesen2 为参考目标。按截图、raw palette index、CPU sequence、PPU 事件
逐层定位首个差异，详见 [迁移指南](migration_guide.md)。任何 100% 结果和已知不同
都写入 [nes-issue.md](../nes-issue.md)。

## NES DB

`assets/nes/nes_db.txt` 是数据源。构建时 `cmake/embed_nes_db.cmake` 生成
`build/generated/nes_db_embedded.h`；运行时只能读取嵌入文本，不能依赖工作目录下
的数据库文件。这样原生和 WASM 使用同一份元数据。

## 调试崩溃和卡死

先构建 Debug：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
gdb --args ./build/app/cli/ear6-cli screenshot -f 1 path/to/game.nes -o /tmp/frame.ppm
```

卡死时中断并执行 `bt`；崩溃后直接查看 `bt`。优先取得调用链，再决定增加日志或
修改代码。

## 提交纪律

- 保留工作区内与当前任务无关的修改。
- 一个行为变化及其测试、文档组成一个可回滚提交。
- mapper 的每个独立实现或修复完成后及时提交。
- 未经用户明确要求不要自动提交；用户要求持续提交时，按该要求执行。
- commit message 不使用 shell 会解释的反引号。
