#include "mapper_112.h"
#include <cstring>

namespace ear6::nes {

void Mapper112::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    current_reg_ = 0; outer_chr_bank_ = 0;
    memset(registers_, 0, sizeof(registers_));
    set_mirroring_type(MirroringType::VERTICAL);
    add_register_range(0x4020, 0x5FFF, MemoryOperation::WRITE);
    select_prg_page(2, -2); select_prg_page(3, -1);
    update_state();
}

void Mapper112::update_state() {
    select_prg_page(0, registers_[0]);
    select_prg_page(1, registers_[1]);
    select_chr_page_2x(0, registers_[2]);
    select_chr_page_2x(1, registers_[3]);
    select_chr_page(4, registers_[4] | ((outer_chr_bank_ & 0x10) << 4));
    select_chr_page(5, registers_[5] | ((outer_chr_bank_ & 0x20) << 3));
    select_chr_page(6, registers_[6] | ((outer_chr_bank_ & 0x40) << 2));
    select_chr_page(7, registers_[7] | ((outer_chr_bank_ & 0x80) << 1));
}

void Mapper112::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE001) {
        case 0x8000: current_reg_ = value & 0x07; break;
        case 0xA000: registers_[current_reg_] = value; break;
        case 0xC000: outer_chr_bank_ = value; break;
        case 0xE000: set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL); break;
    }
    update_state();
}

} // namespace ear6::nes
