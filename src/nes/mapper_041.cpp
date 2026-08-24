#include "mapper_041.h"

namespace ear6::nes {

void Mapper041::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    set_mirroring_type(info.mirroring);
    add_register_range(0x6000, 0x67FF, MemoryOperation::WRITE);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    reset(false);
}

void Mapper041::reset(bool) {
    prg_bank_ = 0;
    chr_bank_ = 0;
    write_register(0x6000, 0);
    write_register(0x8000, 0);
}

void Mapper041::write_register(uint16_t addr, uint8_t value) {
    if (addr <= 0x67FF) {
        prg_bank_ = addr & 0x07;
        chr_bank_ = (chr_bank_ & 0x03) | ((addr >> 1) & 0x0C);
        select_prg_page(0, prg_bank_);
        select_chr_page(0, chr_bank_);
        set_mirroring_type((addr & 0x20) ? MirroringType::HORIZONTAL
                                         : MirroringType::VERTICAL);
    } else if (prg_bank_ >= 4) {
        chr_bank_ = (chr_bank_ & 0x0C) | (value & 0x03);
        select_chr_page(0, chr_bank_);
    }
}

} // namespace ear6::nes
