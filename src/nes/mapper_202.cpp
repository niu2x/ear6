#include "mapper_202.h"

namespace ear6::nes {

void Mapper202::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    prg_mode1_ = false;
    select_prg_page(0, 0); select_prg_page(1, 0); select_chr_page(0, 0);
}

void Mapper202::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    prg_mode1_ = (addr & 0x09) == 0x09;
    select_chr_page(0, (addr >> 1) & 0x07);
    if (prg_mode1_) {
        select_prg_page(0, (addr >> 1) & 0x07);
        select_prg_page(1, ((addr >> 1) & 0x07) + 1);
    } else {
        select_prg_page(0, (addr >> 1) & 0x07);
        select_prg_page(1, (addr >> 1) & 0x07);
    }
    set_mirroring_type((addr & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
}

} // namespace ear6::nes
