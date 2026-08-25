# 快速开始

本章面向第一次构建 Ear6 的用户。Ear6 可以作为库嵌入应用，也提供 CLI、Qt
桌面程序和 WebAssembly 前端。

## 依赖

原生构建需要：

- CMake 3.17 或更高版本
- 支持 C++17 的编译器
- Threads、Boost `program_options` 和 OpenSSL
- Qt 6 Widgets（可选，仅用于 `ear6-desktop`）

Web 构建还需要 Emscripten；前端开发需要 Node.js 和 npm。

ROM 和 BIOS 不随 Ear6 分发。请只使用你有权使用的内容。

## 构建原生版本

```bash
make ear6
```

这会配置 Release 构建，并按本机依赖生成：

- `ear6` 动态库（Linux 为 `.so`，macOS 为 `.dylib`）
- `build/apps/cli/ear6-cli`
- `ear6-desktop`（找到 Qt 6 时）

不需要桌面程序时，可以直接配置核心、CLI 和测试：

```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DEAR6_BUILD_DESKTOP=OFF \
  -DEAR6_BUILD_TESTS=ON
cmake --build build
```

## 使用 CLI

查看 ROM 元数据：

```bash
./build/apps/cli/ear6-cli info path/to/game.nes
```

运行指定帧数并保存无损 PPM 截图：

```bash
./build/apps/cli/ear6-cli screenshot -f 60 path/to/game.nes -o frame.ppm
```

录制音频：

```bash
./build/apps/cli/ear6-cli record -f 600 path/to/game.nes -o audio.wav
```

`--system nes|test|flash` 可以覆盖自动检测。当前自动检测只识别 iNES/NES 2.0
文件，Flash 核心尚未实现。

## 运行测试

```bash
make test
```

或只运行一个测试集：

```bash
cmake -B build -S . -DEAR6_BUILD_TESTS=ON
cmake --build build --target ear6-test
./build/ear6-test --gtest_filter=ChoplifterRegression.*
```

部分 NES 回归测试依赖本地 ROM。测试代码从 `tests/local-roms/nes/mapper_N/`
查找它们；缺失的合法 ROM 会使对应测试跳过，而不是由仓库下载。

## 构建 Web 版本

在 `.env` 中设置 Emscripten toolchain 路径：

```dotenv
EMSCRIPTEN_CMAKE_TOOLCHAIN=/path/to/emscripten/cmake/Modules/Platform/Emscripten.cmake
```

然后运行：

```bash
make serve
```

该命令把 WASM 生成到 `build-web/web-public/`，安装前端依赖并启动开发服务器。
`apps/web/ui/public/` 保留给字体、图标、manifest 等前端静态资源；Vite 在开发和
生产构建中另外把生成的 WASM 暴露到 `ear6/`。构建可部署站点使用
`make ear6-web`，最终产物位于 `build-web/site/`。

## 安装并从 CMake 使用

```bash
cmake --install build --prefix /path/to/ear6-install
```

消费方的 `CMakeLists.txt`：

```cmake
find_package(Ear6 CONFIG REQUIRED)
target_link_libraries(my_emulator_host PRIVATE Ear6::ear6)
```

公开头文件统一从 `ear6/` 命名空间包含：

```c
#include <ear6/ear6.h>
#include <ear6/nes.h>
```

继续阅读 [Public API 手册](api-reference.md) 完成第一个宿主集成。
