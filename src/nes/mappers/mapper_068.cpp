#include "mapper_068.h"

namespace ear6::nes {

void Mapper068::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    nametable_registers_.fill(0);
    use_chr_nametables_ = false;
    prg_ram_enabled_ = false;
    licensing_timer_ = 0;
    using_external_rom_ = false;
    external_page_ = 0;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0);
    select_prg_page(1, 7);
    update_state();
}

void Mapper068::setup_default_work_ram() {
    BaseMapper::setup_default_work_ram();
    update_state();
}

void Mapper068::update_nametables() {
    if (!use_chr_nametables_) {
        set_mirroring_type(mirroring_type_);
        return;
    }

    ChrMemoryType type = chr_rom_size_ > 0
                         ? ChrMemoryType::CHR_ROM
                         : ChrMemoryType::CHR_RAM;
    int8_t access = chr_rom_size_ > 0 ? READ : READ_WRITE;
    for (uint8_t table = 0; table < 4; table++) {
        uint8_t reg = 0;
        switch (mirroring_type_) {
            case MirroringType::VERTICAL: reg = table & 0x01; break;
            case MirroringType::HORIZONTAL: reg = (table & 0x02) >> 1; break;
            case MirroringType::SCREEN_A_ONLY: reg = 0; break;
            case MirroringType::SCREEN_B_ONLY: reg = 1; break;
            case MirroringType::FOUR_SCREENS: break;
        }

        uint32_t offset = static_cast<uint32_t>(nametable_registers_[reg]) * 0x0400;
        uint16_t start = 0x2000 + table * 0x0400;
        set_ppu_memory_mapping(start, start + 0x03FF, type, offset, access);
        set_ppu_memory_mapping(start + 0x1000, start + 0x13FF, type, offset, access);
    }
}

void Mapper068::update_state() {
    std::vector<uint8_t>& ram = rom_info_.has_battery ? save_ram_ : work_ram_;
    int8_t access = prg_ram_enabled_ ? READ_WRITE : NO_ACCESS;
    set_cpu_memory_mapping(0x6000, 0x7FFF, ram.data(), 0,
                           static_cast<uint32_t>(ram.size()), access);

    if (using_external_rom_) {
        if (licensing_timer_ == 0) {
            set_cpu_memory_mapping(0x8000, 0xBFFF, prg_rom_.data(), 0,
                                   prg_size_, NO_ACCESS);
        } else {
            select_prg_page(0, external_page_);
        }
    }
}

void Mapper068::process_cpu_clock() {
    if (licensing_timer_ > 0) {
        licensing_timer_--;
        if (licensing_timer_ == 0) {
            update_state();
        }
    }
}

void Mapper068::write_ram(uint16_t addr, uint8_t value) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        licensing_timer_ = 1024 * 105;
        update_state();
    }
    BaseMapper::write_ram(addr, value);
}

void Mapper068::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000) {
        case 0x8000: select_chr_page(0, value); break;
        case 0x9000: select_chr_page(1, value); break;
        case 0xA000: select_chr_page(2, value); break;
        case 0xB000: select_chr_page(3, value); break;
        case 0xC000:
            nametable_registers_[0] = value | 0x80;
            update_nametables();
            break;
        case 0xD000:
            nametable_registers_[1] = value | 0x80;
            update_nametables();
            break;
        case 0xE000:
            switch (value & 0x03) {
                case 0: set_mirroring_type(MirroringType::VERTICAL); break;
                case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
                case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
            }
            use_chr_nametables_ = (value & 0x10) != 0;
            update_nametables();
            break;
        case 0xF000: {
            uint16_t page_count = static_cast<uint16_t>(prg_size_ / get_prg_page_size());
            bool external_prg = (value & 0x08) == 0;
            if (external_prg && page_count > 8) {
                using_external_rom_ = true;
                external_page_ = 0x08 | ((value & 0x07) % (page_count - 0x08));
                select_prg_page(0, external_page_);
            } else {
                using_external_rom_ = false;
                select_prg_page(0, value & 0x07);
            }
            prg_ram_enabled_ = (value & 0x10) != 0;
            update_state();
            break;
        }
    }
}

} // namespace ear6::nes
