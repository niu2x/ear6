#include "mapper_143.h"

namespace ear6::nes {

void Mapper143::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x4100, 0x5FFF, MemoryOperation::READ);
    select_prg_page(0, 0); select_prg_page(1, 1); select_chr_page(0, 0);
}

uint8_t Mapper143::read_register(uint16_t addr) {
    (void)addr;
    return 0x40;
}

void Mapper143::write_register(uint16_t, uint8_t) {}

} // namespace ear6::nes
