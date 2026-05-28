#include "mapper_078.h"

namespace ear6::nes {

void Mapper078::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    set_has_bus_conflicts(true);
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    uint16_t last_bank = (prg_size_ / 0x4000) - 1;
    select_prg_page(0, 0);
    select_prg_page(1, last_bank);
    select_chr_page(0, 0);
}

void Mapper078::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    select_prg_page(0, value & 0x07);
    select_chr_page(0, (value >> 4) & 0x0F);
    if (rom_info_.submapper_id == 3) {
        set_mirroring_type((value & 0x08) ? MirroringType::VERTICAL : MirroringType::HORIZONTAL);
    } else {
        set_mirroring_type((value & 0x08) ? MirroringType::SCREEN_B_ONLY : MirroringType::SCREEN_A_ONLY);
    }
}

} // namespace ear6::nes
