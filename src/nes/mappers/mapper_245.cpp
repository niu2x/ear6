#include "mapper_245.h"

namespace ear6::nes {

void Mapper245::update_state() {
    Mapper004::update_state();
    if (has_chr_ram()) {
        if (chr_mode_ != 0) {
            select_chr_page_4x(0, 4, ChrMemoryType::CHR_RAM);
            select_chr_page_4x(4, 0, ChrMemoryType::CHR_RAM);
        } else {
            select_chr_page_4x(0, 0, ChrMemoryType::CHR_RAM);
            select_chr_page_4x(4, 4, ChrMemoryType::CHR_RAM);
        }
    }
}

void Mapper245::update_prg_mapping() {
    uint8_t block = (registers_[0] & 0x02) != 0 ? 0x40 : 0x00;
    registers_[6] = (registers_[6] & 0x3F) | block;
    registers_[7] = (registers_[7] & 0x3F) | block;

    uint16_t page_count = static_cast<uint16_t>(prg_size_ / get_prg_page_size());
    uint16_t last_page = page_count >= 0x40
                         ? static_cast<uint16_t>(0x3F | block)
                         : static_cast<uint16_t>(-1);
    if (prg_mode_ == 0) {
        select_prg_page(0, registers_[6]);
        select_prg_page(1, registers_[7]);
        select_prg_page(2, last_page - 1);
        select_prg_page(3, last_page);
    } else {
        select_prg_page(0, last_page - 1);
        select_prg_page(1, registers_[7]);
        select_prg_page(2, registers_[6]);
        select_prg_page(3, last_page);
    }
}

} // namespace ear6::nes
