#include "mapper_sachen_74ls374n.h"

#include "nes_console.h"
#include "nes_memory_manager.h"

namespace ear6::nes {

void MapperSachen74LS374N::init(const RomInfo& info,
                                const std::vector<uint8_t>& prg_rom,
                                const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    current_register_ = 0;
    for (uint8_t& reg : registers_) {
        reg = 0;
    }
    set_mirroring_type(info.mirroring);
    add_register_range(0x4100, 0x7FFF, MemoryOperation::ANY);
    update_state();
}

void MapperSachen74LS374N::update_state() {
    uint8_t chr_page;
    if (rom_info_.mapper_number == 150) {
        chr_page = ((registers_[4] & 0x01) << 2) | (registers_[6] & 0x03);
    } else {
        chr_page = ((registers_[2] & 0x01) << 3) |
                   ((registers_[6] & 0x03) << 1) |
                   (registers_[4] & 0x01);
    }
    select_chr_page(0, chr_page);
    select_prg_page(0, registers_[5] & 0x03);
    switch ((registers_[7] >> 1) & 0x03) {
        case 0:
            set_nametable(0, 0);
            set_nametable(1, 0);
            set_nametable(2, 0);
            set_nametable(3, 1);
            break;
        case 1:
            set_mirroring_type(MirroringType::HORIZONTAL);
            break;
        case 2:
            set_mirroring_type(MirroringType::VERTICAL);
            break;
        case 3:
            set_mirroring_type(MirroringType::SCREEN_A_ONLY);
            break;
    }
}

uint8_t MapperSachen74LS374N::read_register(uint16_t addr) {
    uint8_t open_bus = console_->get_memory_manager()->get_open_bus();
    if ((addr & 0xC101) == 0x4101) {
        return (open_bus & 0xF8) | (registers_[current_register_] & 0x07);
    }
    return open_bus;
}

void MapperSachen74LS374N::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xC101) {
        case 0x4100:
            current_register_ = value & 0x07;
            break;
        case 0x4101:
            registers_[current_register_] = value & 0x07;
            update_state();
            break;
        default:
            break;
    }
}

} // namespace ear6::nes
