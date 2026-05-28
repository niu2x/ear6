#include "mapper_46.h"

namespace ear6::nes {

void Mapper46::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x6000, 0xFFFF, MemoryOperation::WRITE);
    write_register(0x6000, 0); write_register(0x8000, 0);
}

void Mapper46::reset(bool soft_reset) {
    (void)soft_reset;
    regs_[0] = 0; regs_[1] = 0;
    update_state();
}

void Mapper46::update_state() {
    select_prg_page(0, ((regs_[0] & 0x0F) << 1) | (regs_[1] & 0x01));
    select_chr_page(0, ((regs_[0] & 0xF0) >> 1) | ((regs_[1] & 0x70) >> 4));
}

void Mapper46::write_register(uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        regs_[0] = value;
    } else {
        regs_[1] = value;
    }
    update_state();
}

} // namespace ear6::nes
