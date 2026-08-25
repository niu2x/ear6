#include "mapper_009.h"

namespace ear6::nes {

uint16_t Mapper009::get_prg_page_size() {
    return (rom_info_.mapper_number == 10) ? 0x4000 : 0x2000;
}

void Mapper009::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    left_latch_ = 1;
    right_latch_ = 1;
    left_chr_page_[0] = 0;
    left_chr_page_[1] = 0;
    right_chr_page_[0] = 0;
    right_chr_page_[1] = 0;

    add_register_range(0xA000, 0xFFFF, MemoryOperation::WRITE);

    if (rom_info_.mapper_number == 10) {
        select_prg_page(1, (uint16_t)-1);
    } else {
        select_prg_page(1, (uint16_t)-3);
        select_prg_page(2, (uint16_t)-2);
        select_prg_page(3, (uint16_t)-1);
    }

    set_mirroring_type(info.mirroring);
    update_chr_mapping();
}

void Mapper009::update_chr_mapping() {
    select_chr_page(0, left_chr_page_[left_latch_]);
    select_chr_page(1, right_chr_page_[right_latch_]);
}

void Mapper009::write_register(uint16_t addr, uint8_t value) {
    switch (addr >> 12) {
        case 0xA:
            prg_page_ = value & 0x0F;
            select_prg_page(0, prg_page_);
            break;
        case 0xB:
            left_chr_page_[0] = value & 0x1F;
            update_chr_mapping();
            break;
        case 0xC:
            left_chr_page_[1] = value & 0x1F;
            update_chr_mapping();
            break;
        case 0xD:
            right_chr_page_[0] = value & 0x1F;
            update_chr_mapping();
            break;
        case 0xE:
            right_chr_page_[1] = value & 0x1F;
            update_chr_mapping();
            break;
        case 0xF:
            set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            break;
        default:
            break;
    }
}

void Mapper009::notify_vram_address_change(uint16_t addr) {
    if (rom_info_.mapper_number == 10) {
        if (addr >= 0x0FD8 && addr <= 0x0FDF) {
            left_latch_ = 0;
            update_chr_mapping();
        } else if (addr >= 0x0FE8 && addr <= 0x0FEF) {
            left_latch_ = 1;
            update_chr_mapping();
        } else if (addr >= 0x1FD8 && addr <= 0x1FDF) {
            right_latch_ = 0;
            update_chr_mapping();
        } else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
            right_latch_ = 1;
            update_chr_mapping();
        }
    } else {
        if (addr == 0x0FD8) {
            left_latch_ = 0;
            update_chr_mapping();
        } else if (addr == 0x0FE8) {
            left_latch_ = 1;
            update_chr_mapping();
        } else if (addr >= 0x1FD8 && addr <= 0x1FDF) {
            right_latch_ = 0;
            update_chr_mapping();
        } else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
            right_latch_ = 1;
            update_chr_mapping();
        }
    }
}

} // namespace ear6::nes
