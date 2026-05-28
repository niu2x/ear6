#include "mapper_060.h"

namespace ear6::nes {

void Mapper060::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    reset_counter_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, 0); select_chr_page(0, 0);
}

void Mapper060::reset(bool soft_reset) {
    if (soft_reset) {
        reset_counter_ = (reset_counter_ + 1) % 4;
        select_prg_page(0, reset_counter_);
        select_prg_page(1, reset_counter_);
        select_chr_page(0, reset_counter_);
    }
}

} // namespace ear6::nes
