#include "mapper_213.h"

namespace ear6::nes {

void Mapper213::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    write_register(0x8000, 0);
}

void Mapper213::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    select_chr_page(0, (addr >> 3) & 0x07);
    select_prg_page(0, (addr >> 1) & 0x03);
}

} // namespace ear6::nes
