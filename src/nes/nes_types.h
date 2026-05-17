#pragma once

#include <cstdint>
#include <cstring>

namespace ear6::nes {

constexpr uint32_t CPU_CLOCK_RATE_NTSC = 1789773;
constexpr uint32_t APU_SAMPLE_RATE = 96000;

enum class AudioChannel {
    SQUARE1 = 0,
    SQUARE2 = 1,
    TRIANGLE = 2,
    NOISE = 3,
    DMC = 4,
    FDS = 5,
    MMC5 = 6,
    VRC6 = 7,
    VRC7 = 8,
    NAMCO163 = 9,
    SUNSOFT5B = 10,
    MAX_CHANNELS = 11
};

namespace IRQSource {
    constexpr uint8_t EXTERNAL = 0x01;
    constexpr uint8_t FRAME_COUNTER = 0x02;
    constexpr uint8_t DMC = 0x04;
    constexpr uint8_t FDS_DISK = 0x08;
    constexpr uint8_t EPSM = 0x10;
}

enum class MemoryOperation {
    READ = 1,
    WRITE = 2,
    ANY = 3
};

enum class MirroringType {
    HORIZONTAL,
    VERTICAL,
    SCREEN_A_ONLY,
    SCREEN_B_ONLY,
    FOUR_SCREENS
};

enum MemoryAccessType {
    NO_ACCESS = 0x00,
    READ = 0x01,
    WRITE = 0x02,
    READ_WRITE = 0x03
};

enum class ChrMemoryType {
    DEFAULT,
    CHR_ROM,
    CHR_RAM,
    NAMETABLE_RAM,
    MAPPER_RAM
};

enum class PrgMemoryType {
    PRG_ROM,
    SAVE_RAM,
    WORK_RAM,
    MAPPER_RAM
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
    enum class BusConflictType {
        DEFAULT = 0,
        YES = 1,
        NO = 2,
    } bus_conflicts = BusConflictType::DEFAULT;

    enum class GameInputType {
        UNSPECIFIED = 0,
        STANDARD_CONTROLLERS = 1,
        VS_ZAPPER = 7,
        ZAPPER = 8,
    } input_type = GameInputType::UNSPECIFIED;

    int mapper_number = 0;
    int prg_banks = 0;
    int chr_banks = 0;
    bool has_battery = false;
    MirroringType mirroring = MirroringType::HORIZONTAL;
    bool has_trainer = false;
    int submapper_id = 0;
    bool is_vs_system = false;
    bool use_vs_palette = false;
    enum class VsPpuModel {
        PPU_2C02 = 0,
        PPU_2C03 = 1,
        PPU_2C04A = 2,
        PPU_2C04B = 3,
        PPU_2C04C = 4,
        PPU_2C04D = 5,
        PPU_2C05A = 6,
        PPU_2C05B = 7,
        PPU_2C05C = 8,
        PPU_2C05D = 9,
        PPU_2C05E = 10,
    } vs_ppu_model = VsPpuModel::PPU_2C02;
};

} // namespace ear6::nes
