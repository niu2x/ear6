#include "mapper_namco108.h"

namespace ear6::nes {

uint16_t MapperNamco108::get_chr_page_size() {
    return rom_info_.mapper_number == 76 ? 0x0800 : 0x0400;
}

void MapperNamco108::init(const RomInfo& info,
                          const std::vector<uint8_t>& prg_rom,
                          const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    current_register_ = 0;
    registers_[0] = 0;
    registers_[1] = 2;
    registers_[2] = 4;
    registers_[3] = 5;
    registers_[4] = 6;
    registers_[5] = 7;
    registers_[6] = 0;
    registers_[7] = 1;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    update_state();
}

void MapperNamco108::update_prg_mapping() {
    select_prg_page(0, registers_[6]);
    select_prg_page(1, registers_[7]);
    select_prg_page(2, static_cast<uint16_t>(-2));
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void MapperNamco108::update_chr_mapping() {
    if (rom_info_.mapper_number == 76) {
        select_chr_page(0, registers_[2]);
        select_chr_page(1, registers_[3]);
        select_chr_page(2, registers_[4]);
        select_chr_page(3, registers_[5]);
        return;
    }
    if (rom_info_.mapper_number == 88 || rom_info_.mapper_number == 154) {
        registers_[0] &= 0x3F;
        registers_[1] &= 0x3F;
        for (int i = 2; i <= 5; i++) {
            registers_[i] |= 0x40;
        }
    }
    select_chr_page(0, registers_[0] & 0xFE);
    select_chr_page(1, registers_[0] | 0x01);
    select_chr_page(2, registers_[1] & 0xFE);
    select_chr_page(3, registers_[1] | 0x01);
    select_chr_page(4, registers_[2]);
    select_chr_page(5, registers_[3]);
    select_chr_page(6, registers_[4]);
    select_chr_page(7, registers_[5]);
}

void MapperNamco108::update_state() {
    update_prg_mapping();
    update_chr_mapping();
}

void MapperNamco108::write_register(uint16_t addr, uint8_t value) {
    if (rom_info_.mapper_number == 154) {
        set_mirroring_type((value & 0x40) ? MirroringType::SCREEN_B_ONLY
                                          : MirroringType::SCREEN_A_ONLY);
    }
    addr &= 0x8001;
    if (addr == 0x8000) {
        current_register_ = (value & 0x3F) & 0x07;
        return;
    }
    if (current_register_ <= 1) {
        value &= static_cast<uint8_t>(~0x01);
    }
    registers_[current_register_] = value;
    update_state();

    if (rom_info_.mapper_number == 95) {
        uint8_t nametable_1 = (registers_[0] >> 5) & 0x01;
        uint8_t nametable_2 = (registers_[1] >> 5) & 0x01;
        set_nametable(0, nametable_1);
        set_nametable(1, nametable_1);
        set_nametable(2, nametable_2);
        set_nametable(3, nametable_2);
    }
}

} // namespace ear6::nes
