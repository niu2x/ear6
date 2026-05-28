#include "mapper_226.h"

namespace ear6::nes {

void Mapper226::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    registers_[0] = 0; registers_[1] = 0;
    select_prg_page(0, 0); select_prg_page(1, 1); select_chr_page(0, 0);
}

void Mapper226::reset(bool soft_reset) {
    if (soft_reset) {
        registers_[0] = 0; registers_[1] = 0;
        select_prg_page(0, 0); select_prg_page(1, 1); select_chr_page(0, 0);
    }
}

uint8_t Mapper226::get_prg_page_inner() {
    return (registers_[0] & 0x1F) | ((registers_[0] & 0x80) >> 2) | ((registers_[1] & 0x01) << 6);
}

void Mapper226::update_prg() {
    uint8_t prg_page = get_prg_page_inner();
    if (registers_[0] & 0x20) {
        select_prg_page(0, prg_page);
        select_prg_page(1, prg_page);
    } else {
        select_prg_page(0, prg_page & 0xFE);
        select_prg_page(1, (prg_page & 0xFE) + 1);
    }
}

void Mapper226::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0x8001) {
        case 0x8000: registers_[0] = value; break;
        case 0x8001: registers_[1] = value; break;
    }
    update_prg();
    set_mirroring_type(registers_[0] & 0x40 ? MirroringType::VERTICAL : MirroringType::HORIZONTAL);
}

} // namespace ear6::nes
