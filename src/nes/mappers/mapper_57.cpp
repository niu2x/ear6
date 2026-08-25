#include "mapper_57.h"

namespace ear6::nes {

void Mapper57::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    registers_[0] = 0; registers_[1] = 0;
    update_state();
}

void Mapper57::update_state() {
    set_mirroring_type(registers_[1] & 0x08 ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
    select_chr_page(0, ((registers_[0] & 0x40) >> 3) | ((registers_[0] | registers_[1]) & 0x07));
    if (registers_[1] & 0x10) {
        select_prg_page(0, (registers_[1] >> 5) & 0x06);
        select_prg_page(1, ((registers_[1] >> 5) & 0x06) + 1);
    } else {
        select_prg_page(0, (registers_[1] >> 5) & 0x07);
        select_prg_page(1, (registers_[1] >> 5) & 0x07);
    }
}

void Mapper57::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0x8800) {
        case 0x8000: registers_[0] = value; break;
        case 0x8800: registers_[1] = value; break;
    }
    update_state();
}

} // namespace ear6::nes
