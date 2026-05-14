#include "mapper_007.h"

namespace ear6::nes {

void Mapper007::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    select_chr_page(0, 0);
    write_register(0, 0);
}

void Mapper007::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    select_prg_page(0, value & 0x0F);
    set_mirroring_type((value & 0x10) ? MirroringType::SCREEN_B_ONLY : MirroringType::SCREEN_A_ONLY);
}

} // namespace ear6::nes
