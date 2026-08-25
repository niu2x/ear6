#include "mapper_011.h"

namespace ear6::nes {

void Mapper011::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    set_has_bus_conflicts(true);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    select_prg_page(0, 0);
    select_chr_page(0, 0);
}

void Mapper011::write_register(uint16_t addr, uint8_t value) {
    if (rom_info_.mapper_number == 144) {
        value |= (read_ram(addr) & 0x01);
    }

    select_prg_page(0, value & 0x0F);
    select_chr_page(0, (value >> 4) & 0x0F);
}

} // namespace ear6::nes
