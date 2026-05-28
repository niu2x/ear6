#include "mapper_032.h"

namespace ear6::nes {

void Mapper032::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    prg_regs_[0] = prg_regs_[1] = 0;
    prg_mode_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(2, -2);
    select_prg_page(3, -1);
    if (info.submapper_id == 1) {
        set_mirroring_type(MirroringType::SCREEN_A_ONLY);
    }
}

void Mapper032::update_prg_mode() {
    if (prg_mode_ == 0) {
        select_prg_page(0, prg_regs_[0]);
        select_prg_page(1, prg_regs_[1]);
        select_prg_page(2, -2);
        select_prg_page(3, -1);
    } else {
        select_prg_page(0, -2);
        select_prg_page(1, prg_regs_[1]);
        select_prg_page(2, prg_regs_[0]);
        select_prg_page(3, -1);
    }
}

void Mapper032::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000) {
        case 0x8000:
            prg_regs_[0] = value & 0x1F;
            select_prg_page(prg_mode_ == 0 ? 0 : 2, prg_regs_[0]);
            break;
        case 0x9000:
            prg_mode_ = (value & 0x02) >> 1;
            if (rom_info_.submapper_id == 1) prg_mode_ = 0;
            update_prg_mode();
            set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            break;
        case 0xA000:
            prg_regs_[1] = value & 0x1F;
            select_prg_page(1, prg_regs_[1]);
            break;
        case 0xB000:
            select_chr_page(addr & 0x07, value);
            break;
    }
}

} // namespace ear6::nes
