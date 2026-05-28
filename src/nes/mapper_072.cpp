#include "mapper_072.h"

namespace ear6::nes {

void Mapper072::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    prg_flag_ = false; chr_flag_ = false;
    set_has_bus_conflicts(true);
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, -1); select_chr_page(0, 0);
}

void Mapper072::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    if (!prg_flag_ && (value & 0x80)) {
        select_prg_page(0, value & 0x07);
    }
    if (!chr_flag_ && (value & 0x40)) {
        select_chr_page(0, value & 0x0F);
    }
    prg_flag_ = (value & 0x80) != 0;
    chr_flag_ = (value & 0x40) != 0;
}

} // namespace ear6::nes
