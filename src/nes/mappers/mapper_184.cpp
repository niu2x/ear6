#include "mapper_184.h"

namespace ear6::nes {

void Mapper184::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    set_mirroring_type(info.mirroring);
    add_register_range(0x6000, 0x7FFF, MemoryOperation::WRITE);

    select_prg_page(0, 0);
}

void Mapper184::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    select_chr_page(0, value & 0x07);
    select_chr_page(1, 0x80 | ((value >> 4) & 0x07));
}

} // namespace ear6::nes
