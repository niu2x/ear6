#include "mapper_079.h"

namespace ear6::nes {

void Mapper079::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    multicart_mode_ = (info.mapper_number == 113);

    set_mirroring_type(info.mirroring);
    add_register_range(0x4100, 0x5FFF, MemoryOperation::WRITE);

    select_prg_page(0, 0);
    select_chr_page(0, 0);
}

void Mapper079::write_register(uint16_t addr, uint8_t value) {
    if ((addr & 0xE100) != 0x4100) return;

    if (multicart_mode_) {
        select_prg_page(0, (value >> 3) & 0x07);
        select_chr_page(0, (value & 0x07) | ((value >> 3) & 0x08));
        set_mirroring_type((value & 0x80) ? MirroringType::VERTICAL : MirroringType::HORIZONTAL);
    } else {
        select_prg_page(0, (value >> 3) & 0x01);
        select_chr_page(0, value & 0x07);
    }
}

} // namespace ear6::nes
