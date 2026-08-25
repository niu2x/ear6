#include "mapper_71.h"

namespace ear6::nes {

void Mapper71::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    bf9097_mode_ = (info.submapper_id == 1);
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, -1); select_chr_page(0, 0);
}

void Mapper71::write_register(uint16_t addr, uint8_t value) {
    if (addr == 0x9000) {
        bf9097_mode_ = true;
    }
    if (addr >= 0xC000 || !bf9097_mode_) {
        select_prg_page(0, value);
    } else if (addr < 0xC000) {
        set_mirroring_type((value & 0x10) ? MirroringType::SCREEN_A_ONLY : MirroringType::SCREEN_B_ONLY);
    }
}

} // namespace ear6::nes
