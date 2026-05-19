#include "nes_console.h"
#include "nes_memory_manager.h"
#include <cstring>
#include "mapper_000.h"

namespace ear6::nes {

void Mapper000::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();
    chr_is_ram_ = (chr_rom_size_ == 0);

    set_mirroring_type(info.mirroring);

    if (prg_size_ >= 0x8000) {
        // 32KB PRG: page 0 at $8000, page 1 at $C000
        set_cpu_memory_mapping(0x8000, 0xBFFF, 0, PrgMemoryType::PRG_ROM);
        set_cpu_memory_mapping(0xC000, 0xFFFF, 1, PrgMemoryType::PRG_ROM);
    } else {
        // 16KB PRG: page 0 mirrored at $8000 and $C000
        set_cpu_memory_mapping(0x8000, 0xBFFF, 0, PrgMemoryType::PRG_ROM);
        set_cpu_memory_mapping(0xC000, 0xFFFF, 0, PrgMemoryType::PRG_ROM);
    }

    if (chr_is_ram_) {
        initialize_chr_ram(0x2000);
        set_ppu_memory_mapping(0x0000, 0x1FFF, 0, ChrMemoryType::CHR_RAM, READ_WRITE);
    } else {
        // NROM CHR ROM is a single fixed 8KB window.
        set_ppu_memory_mapping(0x0000, 0x1FFF, 0, ChrMemoryType::CHR_ROM, READ);
    }
}

void Mapper000::reset(bool soft_reset) {
    (void)soft_reset;
    if (chr_is_ram_) {
        memset(chr_ram_.data(), 0, chr_ram_.size());
    }
}

void Mapper000::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::READ, 0x4020, 0xFFFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x4020, 0xFFFF);
    if (chr_is_ram_) {
        // No PRG RAM by default
    }
}

uint8_t Mapper000::read_ram(uint16_t addr) {
    if (prg_memory_access_[addr >> 8] & READ) {
        return prg_pages_[addr >> 8][(uint8_t)addr];
    }
    return console_->get_memory_manager()->get_open_bus();
}

void Mapper000::write_ram(uint16_t addr, uint8_t value) {
    // NROM doesn't have writable registers
    (void)addr;
    (void)value;
}

} // namespace ear6::nes
