#include "mapper_231.h"

namespace ear6::nes {

void Mapper231::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, 0); select_chr_page(0, 0);
}

void Mapper231::reset(bool soft_reset) {
    if (soft_reset) {
        select_prg_page(0, 0); select_prg_page(1, 0);
    }
}

void Mapper231::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    uint8_t prg_bank = ((addr >> 5) & 0x01) | (addr & 0x1E);
    select_prg_page(0, prg_bank & 0x1E);
    select_prg_page(1, prg_bank);
    set_mirroring_type((addr & 0x80) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
}

} // namespace ear6::nes
