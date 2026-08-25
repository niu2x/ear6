#include "mapper_075.h"

namespace ear6::nes {

void Mapper075::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    chr_banks_[0] = 0;
    chr_banks_[1] = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void Mapper075::update_chr_banks() {
    select_chr_page(0, chr_banks_[0]);
    select_chr_page(1, chr_banks_[1]);
}

void Mapper075::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000) {
        case 0x8000:
            select_prg_page(0, value);
            break;
        case 0x9000:
            if (mirroring_type_ != MirroringType::FOUR_SCREENS) {
                set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL
                                                   : MirroringType::VERTICAL);
            }
            chr_banks_[0] = (chr_banks_[0] & 0x0F) | ((value & 0x02) << 3);
            chr_banks_[1] = (chr_banks_[1] & 0x0F) | ((value & 0x04) << 2);
            update_chr_banks();
            break;
        case 0xA000:
            select_prg_page(1, value);
            break;
        case 0xC000:
            select_prg_page(2, value);
            break;
        case 0xE000:
            chr_banks_[0] = (chr_banks_[0] & 0x10) | (value & 0x0F);
            update_chr_banks();
            break;
        case 0xF000:
            chr_banks_[1] = (chr_banks_[1] & 0x10) | (value & 0x0F);
            update_chr_banks();
            break;
        default:
            break;
    }
}

} // namespace ear6::nes
