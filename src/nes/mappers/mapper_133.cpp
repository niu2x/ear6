#include "mapper_133.h"

namespace ear6::nes {

void Mapper133::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x4100, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_chr_page(0, 0);
}

void Mapper133::write_register(uint16_t addr, uint8_t value) {
    if ((addr & 0x6100) != 0x4100) return;
    select_prg_page(0, (value >> 2) & 0x01);
    select_chr_page(0, value & 0x03);
}

} // namespace ear6::nes
