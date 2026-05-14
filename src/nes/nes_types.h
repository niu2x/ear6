#pragma once

#include <cstdint>
#include <cstring>

namespace ear6::nes {

constexpr uint32_t CPU_CLOCK_RATE_NTSC = 1789773;
constexpr uint32_t APU_SAMPLE_RATE = 96000;

enum class AudioChannel {
    Square1 = 0,
    Square2 = 1,
    Triangle = 2,
    Noise = 3,
    DMC = 4,
    FDS = 5,
    MMC5 = 6,
    VRC6 = 7,
    VRC7 = 8,
    Namco163 = 9,
    Sunsoft5B = 10,
    MAX_CHANNELS = 11
};

namespace IRQSource {
    constexpr uint8_t External = 0x01;
    constexpr uint8_t FrameCounter = 0x02;
    constexpr uint8_t DMC = 0x04;
    constexpr uint8_t FdsDisk = 0x08;
    constexpr uint8_t Epsm = 0x10;
}

enum class MemoryOperation {
    Read = 1,
    Write = 2,
    Any = 3
};

enum class MirroringType {
    Horizontal,
    Vertical,
    ScreenAOnly,
    ScreenBOnly,
    FourScreens
};

enum MemoryAccessType {
    NoAccess = 0x00,
    Read = 0x01,
    Write = 0x02,
    ReadWrite = 0x03
};

enum class ChrMemoryType {
    Default,
    ChrRom,
    ChrRam,
    NametableRam,
    MapperRam
};

enum class PrgMemoryType {
    PrgRom,
    SaveRam,
    WorkRam,
    MapperRam
};

struct PPUStatusFlags {
    bool sprite_overflow = false;
    bool sprite_zero_hit = false;
    bool vertical_blank = false;
};

struct PpuControlFlags {
    uint16_t background_pattern_addr = 0;
    uint16_t sprite_pattern_addr = 0;
    bool vertical_write = false;
    bool large_sprites = false;
    bool nmi_on_vertical_blank = false;
};

struct PpuMaskFlags {
    bool grayscale = false;
    bool background_mask = false;
    bool sprite_mask = false;
    bool background_enabled = false;
    bool sprites_enabled = false;
    bool intensify_red = false;
    bool intensify_green = false;
    bool intensify_blue = false;
};

struct TileInfo {
    uint16_t tile_addr = 0;
    uint8_t low_byte = 0;
    uint8_t high_byte = 0;
    uint8_t palette_offset = 0;
};

struct NesSpriteInfo {
    uint8_t sprite_x = 0;
    uint8_t low_byte = 0;
    uint8_t high_byte = 0;
    uint8_t palette_offset = 0;
    bool horizontal_mirror = false;
    bool background_priority = false;
};

namespace PSFlags {
    constexpr uint8_t CARRY     = 0x01;
    constexpr uint8_t ZERO      = 0x02;
    constexpr uint8_t INTERRUPT = 0x04;
    constexpr uint8_t DECIMAL   = 0x08;
    constexpr uint8_t BREAK     = 0x10;
    constexpr uint8_t RESERVED  = 0x20;
    constexpr uint8_t OVERFLOW_FLAG = 0x40;
    constexpr uint8_t NEGATIVE  = 0x80;
}

struct NesCpuState {
    uint64_t cycle_count = 0;
    uint16_t pc = 0;
    uint8_t sp = 0;
    uint8_t a = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t ps = 0;
    uint8_t irq_flag = 0;
    bool nmi_flag = false;
};

struct NesPpuState {
    PPUStatusFlags status_flags;
    PpuMaskFlags mask;
    PpuControlFlags control;
    int32_t scanline = 0;
    uint32_t cycle = 0;
    uint32_t frame_count = 0;
    uint16_t bus_address = 0;
    uint8_t memory_read_buffer = 0;
    uint16_t video_ram_addr = 0;
    uint16_t tmp_video_ram_addr = 0;
    uint8_t fine_x_scroll = 0;
    bool write_toggle = false;
    uint8_t sprite_ram_addr = 0;
};

struct RomInfo {
    int mapper_number = 0;
    int prg_banks = 0;
    int chr_banks = 0;
    bool has_battery = false;
    MirroringType mirroring = MirroringType::Horizontal;
    bool has_trainer = false;
};

} // namespace ear6::nes
