#include "mapper_230.h"

namespace ear6::nes {

void Mapper230::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_chr_page(0, 0);
    reset(true);
}

void Mapper230::reset(bool soft_reset) {
    if (soft_reset) {
        contra_mode_ = !contra_mode_;
        if (contra_mode_) {
            select_prg_page(0, 0);
            select_prg_page(1, 7);
            set_mirroring_type(MirroringType::VERTICAL);
        } else {
            select_prg_page(0, 8);
            select_prg_page(1, 9);
            set_mirroring_type(MirroringType::HORIZONTAL);
        }
    }
}

void Mapper230::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    if (contra_mode_) {
        select_prg_page(0, value & 0x07);
    } else {
        if (value & 0x20) {
            select_prg_page(0, (value & 0x1F) + 8);
            select_prg_page(1, (value & 0x1F) + 8);
        } else {
            select_prg_page(0, (value & 0x1E) + 8);
            select_prg_page(1, (value & 0x1E) + 9);
        }
        set_mirroring_type(value & 0x40 ? MirroringType::VERTICAL : MirroringType::HORIZONTAL);
    }
}

} // namespace ear6::nes
