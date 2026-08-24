#include "mapper_045.h"

namespace ear6::nes {

void Mapper045::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    outer_registers_.fill(0);
    outer_registers_[2] = 0x0F;
    outer_register_index_ = 0;

    Mapper004::init(info, prg_rom, chr_rom);
    add_register_range(0x6000, 0x7FFF, MemoryOperation::WRITE);
    update_state();
}

void Mapper045::select_chr_page(uint16_t slot, uint16_t page, ChrMemoryType type) {
    if (!has_chr_ram()) {
        page &= 0xFF >> (0x0F - (outer_registers_[2] & 0x0F));
        page |= outer_registers_[0]
                | (static_cast<uint16_t>(outer_registers_[2] & 0xF0) << 4);
    }
    BaseMapper::select_chr_page(slot, page, type);
}

void Mapper045::select_prg_page(uint16_t slot, uint16_t page, PrgMemoryType type) {
    page &= 0x3F ^ (outer_registers_[3] & 0x3F);
    page |= outer_registers_[1];
    BaseMapper::select_prg_page(slot, page, type);
}

void Mapper045::write_register(uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        if ((outer_registers_[3] & 0x40) == 0) {
            outer_registers_[outer_register_index_] = value;
            outer_register_index_ = (outer_register_index_ + 1) & 0x03;
        }
        if ((outer_registers_[3] & 0x40) != 0) {
            remove_register_range(0x6000, 0x7FFF, MemoryOperation::WRITE);
        }
        update_state();
        return;
    }
    Mapper004::write_register(addr, value);
}

} // namespace ear6::nes
