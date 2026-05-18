#include "mapper_013.h"

namespace ear6::nes {

void Mapper013::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    select_prg_page(0, 0);
    select_chr_page(0, 0);
    set_mirroring_type(MirroringType::VERTICAL);
}

void Mapper013::write_register(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000) {
        select_chr_page(1, value & 0x03);
    }
}

} // namespace ear6::nes
