#include "mapper_216.h"

namespace ear6::nes {

void Mapper216::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x5000, 0x5000, MemoryOperation::READ);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    write_register(0x8000, 0);
}

uint8_t Mapper216::read_register(uint16_t addr) {
    (void)addr;
    return 0;
}

void Mapper216::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    select_prg_page(0, addr & 0x01);
    select_chr_page(0, (addr & 0x0E) >> 1);
}

} // namespace ear6::nes
