# Ear6 Public API 手册

本章描述 Ear6 0.1.x 的公开 C API。它既适用于 C，也适用于 C++、FFI 和 WASM
桥接。头文件声明是编译接口；本章同时说明当前实现状态、所有权和调用顺序。

## 1. 头文件与链接

通用功能只需要：

```c
#include <ear6/ear6.h>
```

使用 NES 专有配置时再包含：

```c
#include <ear6/nes.h>
```

Flash 扩展头 `<ear6/flash.h>` 目前仅为占位。所有头文件都可由 C 编译器直接
包含；C++ 下声明自动放入 `extern "C"`。

CMake 安装包导出目标 `Ear6::ear6`：

```cmake
find_package(Ear6 CONFIG REQUIRED)

add_executable(player main.c)
target_link_libraries(player PRIVATE Ear6::ear6)
```

版本字符串由 `<ear6/version.h>` 中的 `EAR6_VERSION` 提供。

## 2. 核心概念

### `Ear6`

```c
typedef struct Ear6 Ear6;
```

`Ear6` 是不透明上下文。它拥有系统核心、framebuffer、音频队列和回调注册。
调用者只能保存 `Ear6*`，不能访问结构字段。

一个上下文创建后不能切换系统。要更换系统，先 `ear6_destroy()`，再用新的
`Ear6SystemType` 创建。

### `Ear6SystemType`

```c
typedef enum {
    EAR6_SYSTEM_TEST,
    EAR6_SYSTEM_NES,
    EAR6_SYSTEM_FLASH,
} Ear6SystemType;
```

| 值 | 当前行为 |
|---|---|
| `EAR6_SYSTEM_TEST` | 创建测试图案系统；无需 ROM，无音频 |
| `EAR6_SYSTEM_NES` | 创建 NES 系统；加载 iNES/NES 2.0 内容 |
| `EAR6_SYSTEM_FLASH` | 尚未实现；`ear6_create()` 返回 `nullptr` |

枚举值存在不等于核心已实现。宿主必须检查 `ear6_create()` 的返回值。

### 返回码

对返回 `int` 的操作，公共规则是：

- `0`：成功
- 非 `0`：失败

当前通用包装层还使用以下负值，应用可以记录它们，但不应把未声明的具体负值
当成长期稳定 ABI：

| 值 | 当前含义 |
|---:|---|
| `-1` | 无效上下文、无效参数、错误系统类型或系统拒绝输入 |
| `-2` | C++ 实现抛出异常，已在 C ABI 边界捕获 |
| `-3` | `ear6_load_from_file()` 打开、定位或读取文件失败 |
| `-4` | state buffer 容量不足，或 state 格式、版本、系统、内容身份不匹配 |

未来应增加公开错误枚举；见 [项目路线图](TODO.md)。

## 3. 最小调用流程

```text
ear6_create(system)
        |
        v
ear6_load_from_file(...) 或 ear6_load_from_memory(...)
        |
        +--> 设置系统专有配置和输入
        |
        v
重复调用 ear6_step(ctx)
        |
        +--> 读取 framebuffer
        +--> 读取并消费 audio packet
        |
        v
ear6_destroy(ctx)
```

所有函数都要求调用者保证同一个上下文没有被多个线程同时访问。当前 API 不提供
内部同步，也不保证回调中的重入安全。

## 4. 生命周期

### `ear6_create`

```c
Ear6* ear6_create(Ear6SystemType system);
```

创建指定系统的上下文。

- 成功：返回由 Ear6 拥有的新上下文
- 未知或未实现系统、分配失败、内部异常：返回 `nullptr`
- 创建成功后必须恰好调用一次 `ear6_destroy()`

### `ear6_destroy`

```c
void ear6_destroy(Ear6* ctx);
```

销毁上下文和所有内部缓冲区。`ctx == nullptr` 是安全的。返回过的 framebuffer、
音频指针和回调中的数据指针在销毁后全部失效。

## 5. 加载内容

### `ear6_load_from_file`

```c
int ear6_load_from_file(Ear6* ctx, const char* path);
```

从本地文件系统读取全部内容并交给所选系统。

- `ctx` 和 `path` 必须非空
- 路径只在函数调用期间使用，不被上下文保存
- 返回 `0` 表示系统接受并完成初始化
- 原生应用可以使用；浏览器宿主通常应使用 `ear6_load_from_memory()`

Test 系统接受任意可读文件。NES 系统要求有效的 iNES/NES 2.0 数据。

### `ear6_load_from_memory`

```c
int ear6_load_from_memory(
    Ear6* ctx,
    const void* data,
    int size,
    const char* name_hint
);
```

从内存加载内容。

| 参数 | 要求 |
|---|---|
| `ctx` | 有效上下文 |
| `data` | 指向至少 `size` 字节的可读内存 |
| `size` | 数据字节数，必须能用非负 `int` 表示 |
| `name_hint` | 可选名称/扩展名提示，可为 `nullptr` |

函数返回后，当前系统实现不保留调用者的输入指针；调用者可以释放或复用原始
缓冲区。NES 会把需要的 PRG、CHR 和 trainer 数据复制进核心。

不要把 `name_hint` 当成通用自动检测 API。调用者已经在创建上下文时选择了系统；
它只是供系统实现做格式或内容提示。

## 6. Save State

Ear6 只负责把整机状态写入内存或从内存恢复。文件名、槽位、压缩、数据库和云端
同步都由宿主负责；这与游戏自身的 battery-backed save RAM 是两种不同能力。

### `ear6_save_state_to_memory`

```c
int ear6_save_state_to_memory(
    Ear6* ctx,
    void* buffer,
    size_t capacity,
    size_t* state_size
);
```

采用两阶段调用。先传入 `buffer == NULL` 查询所需大小，再由调用者分配内存：

```c
size_t size = 0;
if (ear6_save_state_to_memory(ctx, NULL, 0, &size) == 0) {
    void* state = malloc(size);
    int rc = ear6_save_state_to_memory(ctx, state, size, &size);
    /* 宿主可以把 state 写入文件、数据库或其他存储。 */
    free(state);
}
```

当容量不足时函数返回非零，并通过 `state_size` 返回所需容量。成功后 buffer 中是
带 magic、格式版本、内容身份、原始内容、名称提示、当前画面预览、系统 payload
和整体校验值的 Ear6 state。该二进制格式不等同于 Mesen2 state，也不承诺其他
模拟器可以读取。

container wire version 由 `EAR6_STATE_CONTAINER_WIRE_VERSION` 定义，当前值为 `1`。
固定 preamble 为 32 字节，所有整数均为 little-endian：

| Offset | 字段 | 大小 |
|---:|---|---:|
| 0 | magic `EAR6STAT` | 8 |
| 8 | container wire version | 4 |
| 12 | preamble size | 4 |
| 16 | protobuf body size | 8 |
| 24 | protobuf body CRC32 | 4 |
| 28 | reserved | 4 |

preamble 后是 `StateContainer` 的 protobuf binary body。系统类型、content identity、
原始 content 和 opaque system state 是 required；名称和 preview 是 optional。未知
protobuf 字段被忽略，字段顺序不影响解析，缺少 required 字段时拒绝加载。完整字段
编号、校验规则和演进策略见 [Save State 格式](state-format.md)。宿主应优先使用
`ear6_get_state_info()`，不要自行拼接 state。

state 内嵌原始 ROM，因此大小至少约等于 ROM 大小加运行态 payload。它不是压缩包，
也没有加密。文件名提示只保留末尾名称，不保存本地目录、URL query 或 fragment。
state 与内嵌 ROM 具有相同的隐私、版权和分发风险；宿主不能把它当成不含游戏内容
的小型元数据文件。

当系统存在有效 framebuffer 时，保存操作会复制调用瞬间的当前可见帧作为预览。
NES 因而保存最近完成的 PPU frame；预览只用于 UI，不会在 load 时覆盖或参与模拟
状态恢复。当前 256x240 RGBA8888 NES 预览额外占用 245760 字节。

### `ear6_get_state_info`

```c
int ear6_get_state_info(
    const void* data,
    size_t size,
    Ear6StateInfo* info
);
```

在不创建或修改模拟器上下文的情况下验证 state，并返回菜单展示所需的元数据：
container wire version、系统类型、内容 identity、内容大小、名称提示和预览图。
成功返回 `0`；magic、wire version、protobuf、required 字段、长度或 CRC 无效时返回非零
并清空 `info`。未知或无效的 optional preview 不影响 state 验证，只返回无预览。

`content_name_hint` 和 `preview_data` 都直接指向调用者传入的 state buffer，不由 Ear6
分配，也不能释放；它们只在该 buffer 保持原地址且未被修改时有效。名称由
`content_name_hint_size` 定界，不保证以 NUL 结尾。`preview_data` 当前为
`preview_width * preview_height * 4` 字节的 RGBA8888；没有预览时 format 为
`EAR6_STATE_PREVIEW_NONE`、指针为 `NULL`、尺寸和大小均为零。

### `ear6_load_state_from_memory`

```c
int ear6_load_state_from_memory(Ear6* ctx, const void* data, size_t size);
```

无需预先加载 ROM。Ear6 先在临时系统中加载 state 内嵌的内容，再恢复系统 payload；
只有两步都成功才会原子替换 `ctx` 当前系统。格式版本、系统类型、内容 identity、
长度、checksum 或 payload 不匹配时返回非零，原有内容和模拟状态保持不变。加载
成功后，先前取得的 framebuffer 和音频指针均视为失效，宿主应重新查询。

公共 envelope 保存 `Ear6SystemType`，用于确认 state 能否交给当前 `ctx`。各系统的
payload 是 protobuf 容器中的 opaque `system_state` 字段，并在内部维护自己的版本；NES 和
Test 当前 payload version 都是 `1`。Flash 修改自己的 payload 不改变 NES 版本或
loader，外壳增加 optional 字段也不升级任何 system payload。

state 头保存由完整 ROM 数据计算出的 64-bit content identity，并对名称、ROM、
preview 和 payload 组成的 body 计算 CRC32。identity 和 CRC 用于发现误配与损坏，
不是防篡改的密码学验证。

当前 Test 系统和 MapperFactory 接受的全部 NES mapper 都支持完整 state 往返。
回归测试对全部受支持 mapper 做合成 ROM 连续运行验证，并对 `tests/local-roms/nes/` 中
现有的真实 ROM mapper 样本做恢复后重放验证；两类测试都同时覆盖原上下文恢复和
空白新上下文直接从 state 恢复内嵌 ROM。这里的 state 覆盖不表示这些 mapper 已经达到
Mesen2 的逐周期或逐像素精确性；兼容性证据仍以
[NES 兼容性结果](compatibility/nes.md) 为准。

Emscripten 宿主可通过 `ear6_web_save_state_to_memory()` 和
`ear6_web_load_state_from_memory()` 使用相同语义。WASM32 中 `size_t` 与 state 大小
输出均占 4 字节；JavaScript 宿主负责用 `_malloc()` 分配 buffer 和 `state_size`，并
在调用后释放。

## 7. 推进模拟

### `ear6_step`

```c
int ear6_step(Ear6* ctx);
```

把系统推进到下一帧边界。成功返回 `0`，随后 framebuffer 表示刚完成的帧。

一次成功调用的顺序是：

1. 系统完成一帧。
2. 若设置了帧回调，同步调用一次。
3. 若设置了音频回调且有音频，同步调用一次并自动消费该音频包。
4. 返回宿主。

`ear6_step()` 不负责按真实时间等待。宿主决定 60 Hz、PAL 速率、暂停、快进和
掉帧策略。不要直接把浏览器刷新率等同于模拟帧率。

## 8. 视频

### `Ear6FrameCallback`

```c
typedef void (*Ear6FrameCallback)(
    const void* data,
    int width,
    int height,
    void* user_data
);
```

`data` 指向紧密排列的 RGBA8888：每像素依次为 R、G、B、A 四个 8-bit 分量，
stride 为 `width * 4` 字节。指针只读且由上下文拥有。

### `ear6_set_frame_callback`

```c
void ear6_set_frame_callback(
    Ear6* ctx,
    Ear6FrameCallback cb,
    void* user_data
);
```

设置或替换帧回调。`cb == nullptr` 关闭回调；`user_data` 不由 Ear6 读取或释放。
回调在调用 `ear6_step()` 的同一线程内同步执行。

回调返回后不要长期保存 `data`。需要跨帧使用时复制 `width * height * 4` 字节。

### 拉取 framebuffer

```c
const uint8_t* ear6_get_framebuffer(Ear6* ctx);
int ear6_get_frame_width(Ear6* ctx);
int ear6_get_frame_height(Ear6* ctx);
```

这三个函数用于回调之外的拉取模式：

```c
if (ear6_step(ctx) == 0) {
    const uint8_t* rgba = ear6_get_framebuffer(ctx);
    int width = ear6_get_frame_width(ctx);
    int height = ear6_get_frame_height(ctx);
    if (rgba != NULL && width > 0 && height > 0) {
        upload_rgba(rgba, width, height);
    }
}
```

无效上下文分别返回 `nullptr`、`0`、`0`。指针至少在下一次会修改系统状态的调用
前有效；稳妥的宿主应在当前帧立即上传或复制。

当前尺寸：Test 为其内部测试画面尺寸，NES 为 `256 x 240`。宿主必须每次使用
查询值，不能把 NES 尺寸写成跨系统常量。

## 9. 音频

### `Ear6AudioCallback`

```c
typedef void (*Ear6AudioCallback)(
    const int16_t* data,
    int num_samples,
    void* user_data
);
```

音频是 signed 16-bit PCM。`data` 只读，`num_samples` 是当前音频包中每声道的
sample frame 数量，而不是 `int16_t` 元素总数。

当前 NES 输出为 96,000 Hz、双声道交错 PCM，因此一个包包含
`num_samples * 2` 个 `int16_t`。公共 API 尚未提供 sample rate 和 channel count
查询；这是 0.1.x 的已知接口缺口，不应假设未来所有系统都与 NES 相同。

### 回调模式

```c
void ear6_set_audio_callback(
    Ear6* ctx,
    Ear6AudioCallback cb,
    void* user_data
);
```

设置回调后，`ear6_step()` 在有音频时同步调用它，并在回调返回后自动消费该包。
回调中应立即复制、编码或提交音频，不能保留指针，也不要再调用
`ear6_consume_audio()`。

传入 `nullptr` 关闭回调并恢复拉取模式。

### 拉取模式

```c
const int16_t* ear6_get_audiobuffer(Ear6* ctx);
int ear6_get_audio_num_samples(Ear6* ctx);
void ear6_consume_audio(Ear6* ctx);
```

不设置音频回调时，宿主读取最旧的可用音频包：

```c
const int16_t* pcm = ear6_get_audiobuffer(ctx);
int frames = ear6_get_audio_num_samples(ctx);
if (pcm != NULL && frames > 0) {
    submit_stereo_s16(pcm, frames);
    ear6_consume_audio(ctx);
}
```

`ear6_consume_audio()` 消费一个完整包。它不接受部分 sample 数量；没有可用包或
上下文无效时是 no-op。消费后先前的音频指针失效。

## 10. NES 扩展

NES API 在 `<ear6/nes.h>` 中。所有操作都要求 `ctx` 是由
`EAR6_SYSTEM_NES` 创建的上下文。

### 区域

```c
typedef enum {
    EAR6_NES_REGION_NTSC,
    EAR6_NES_REGION_PAL,
} Ear6NesRegion;

int ear6_nes_set_region(Ear6* ctx, Ear6NesRegion region);
```

**当前状态：已声明但库中尚无实现符号，请勿调用。** NES 当前运行路径以 NTSC
为主，PAL 精确性仍在路线图中。

### Mapper 覆盖

```c
int ear6_nes_set_mapper(Ear6* ctx, int mapper_number);
```

**当前状态：已声明但库中尚无实现符号，请勿调用。** Mapper 由 ROM header 与
内嵌 NES DB 决定。兼容性工具如需覆盖 mapper，应先通过 CLI/加载器层设计明确的
调试接口，不能假设该声明已经可链接。

### 调色板

```c
int ear6_nes_set_palette(Ear6* ctx, const uint32_t palette[64]);
```

设置 64 色 RGB 调色板。每项格式为 `0x00RRGGBB`，高 8 位忽略。数组必须包含
64 项，并在调用期间有效。成功返回 `0`。

VS System ROM 可能根据 NES DB 和 PPU 型号使用硬件专有颜色表，此时普通 NES
调色板并不覆盖所有最终颜色路径。

### 标准手柄

```c
typedef enum {
    EAR6_NES_BUTTON_A      = 0,
    EAR6_NES_BUTTON_B      = 1,
    EAR6_NES_BUTTON_SELECT = 2,
    EAR6_NES_BUTTON_START  = 3,
    EAR6_NES_BUTTON_UP     = 4,
    EAR6_NES_BUTTON_DOWN   = 5,
    EAR6_NES_BUTTON_LEFT   = 6,
    EAR6_NES_BUTTON_RIGHT  = 7,
} Ear6NesButton;

int ear6_nes_set_button_state(
    Ear6* ctx,
    Ear6NesButton button,
    int pressed
);

void ear6_nes_clear_input(Ear6* ctx);
```

`ear6_nes_set_button_state()` 当前控制 player 1 标准手柄。`pressed == 0` 表示释放，
其他值表示按下。成功返回 `0`；无效或非 NES 上下文返回非零。

`ear6_nes_clear_input()` 释放当前 NES 输入管理器中的全部按键。无效或非 NES
上下文时是 no-op。窗口失焦、暂停或 ROM 切换时应调用它，避免粘键。

## 11. Flash 扩展

```c
#include <ear6/flash.h>

int ear6_flash_set_version(Ear6* ctx, int version);
```

**当前状态：Flash 核心和该函数实现都不存在。** 头文件只保留未来扩展边界，
0.1.x 应用不能创建或链接使用 Flash 运行路径。

## 12. 诊断函数

```c
int ear6_test(void);
```

当前固定返回 `42`，只用于早期链接/导出自检。它不是模拟器功能，也不应成为新
集成的健康检查协议。

## 13. 完整 C 示例

下面的程序加载 NES ROM，按 60 帧推进，并读取最后一帧。真正的宿主需要把
framebuffer 上传到图形 API，并按自己的时钟调度 `ear6_step()`。

```c
#include <ear6/ear6.h>
#include <ear6/nes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s game.nes\n", argv[0]);
        return 2;
    }

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    if (ctx == NULL) {
        fprintf(stderr, "failed to create NES system\n");
        return 1;
    }

    int rc = ear6_load_from_file(ctx, argv[1]);
    if (rc != 0) {
        fprintf(stderr, "failed to load ROM: %d\n", rc);
        ear6_destroy(ctx);
        return 1;
    }

    ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 1);
    rc = ear6_step(ctx);
    ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 0);

    for (int frame = 1; rc == 0 && frame < 60; ++frame) {
        rc = ear6_step(ctx);
    }

    if (rc == 0) {
        const uint8_t* rgba = ear6_get_framebuffer(ctx);
        int width = ear6_get_frame_width(ctx);
        int height = ear6_get_frame_height(ctx);
        printf("frame: %dx%d, first pixel: #%02x%02x%02x%02x\n",
               width, height, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    ear6_destroy(ctx);
    return rc == 0 ? 0 : 1;
}
```

## 14. 集成检查清单

- 每次创建后检查 `nullptr`，每次加载/推进后检查非零返回码。
- 系统配置函数只用于匹配类型的上下文。
- 不写入、不释放、不跨状态变化保存 Ear6 返回的指针。
- 音频选择回调模式或拉取模式之一；拉取后消费一次完整包。
- 由宿主控制真实时间节奏，不能依赖 `ear6_step()` 自己等待。
- 失焦或输入设备断开时清理系统输入。
- 不把 NES 的尺寸、帧率、声道数或采样率当成所有系统的常量。
- 销毁前先停止会访问该上下文的音频、渲染和工作线程。
