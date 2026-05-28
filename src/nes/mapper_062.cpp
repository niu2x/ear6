#include "mapper_062.h"

namespace ear6::nes {

void Mapper062::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, 1); select_chr_page(0, 0);
}

void Mapper062::reset(bool soft_reset) {
    if (soft_reset) {
        select_prg_page(0, 0); select_prg_page(1, 1); select_chr_page(0, 0);
    }
}

void Mapper062::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    uint8_t prg_page = ((addr & 0x3F00) >> 8) | (addr & 0x40);
    uint8_t chr_page = ((addr & 0x1F) << 2) | (value & 0x03);
    if (addr & 0x20) {
        select_prg_page(0, prg_page);
        select_prg_page(1, prg_page);
    } else {
        select_prg_page(0, prg_page & 0xFE);
        select_prg_page(1, (prg_page & 0xFE) + 1);
    }
    select_chr_page(0, chr_page);
    set_mirroring_type((addr & 0x80) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
}

} // namespace ear6::nes
