#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include "mapper_004.h"

namespace ear6::nes {

void Mapper004::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    registers_[0] = 0;
    registers_[1] = 2;
    registers_[2] = 4;
    registers_[3] = 5;
    registers_[4] = 6;
    registers_[5] = 7;
    registers_[6] = 0;
    registers_[7] = 1;

    work_ram_.resize(0x2000, 0);

    irq_reload_value_ = 0;
    irq_counter_ = 0;
    irq_reload_ = false;
    irq_enabled_ = false;
    wram_enabled_ = false;
    wram_write_protected_ = false;
    reg_a000_ = 0;

    update_state();
    update_mirroring();
}

bool Mapper004::is_a12_rising_edge(uint16_t addr) {
    if (addr & 0x1000) {
        bool rising_edge = a12_low_counter_ > 0;
        a12_low_counter_ = 0;
        return rising_edge;
    }
    a12_low_counter_++;
    return false;
}

void Mapper004::notify_vram_address_change(uint16_t addr) {
    if (is_a12_rising_edge(addr)) {
        if (irq_counter_ == 0 || irq_reload_) {
            irq_counter_ = irq_reload_value_;
        } else {
            irq_counter_--;
        }

        if (irq_counter_ == 0 && irq_enabled_) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
        irq_reload_ = false;
    }
}

void Mapper004::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE001) {
        case 0x8000:
            current_register_ = value & 0x07;
            chr_mode_ = (value >> 7) & 1;
            prg_mode_ = (value >> 6) & 1;
            break;

        case 0x8001:
            if (current_register_ <= 1) {
                value &= ~0x01;
            }
            registers_[current_register_] = value;
            update_state();
            break;

        case 0xA000:
            reg_a000_ = value;
            update_mirroring();
            break;

        case 0xA001:
            wram_enabled_ = (value & 0x80) != 0;
            wram_write_protected_ = (value & 0x40) != 0;
            update_state();
            break;

        case 0xC000:
            irq_reload_value_ = value;
            break;

        case 0xC001:
            irq_counter_ = 0;
            irq_reload_ = true;
            break;

        case 0xE000:
            irq_enabled_ = false;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;

        case 0xE001:
            irq_enabled_ = true;
            break;
    }
}

void Mapper004::update_state() {
    bool has_wram = (rom_info_.has_battery) || true;

    if (has_wram) {
        MemoryAccessType access;
        if (wram_enabled_) {
            access = (wram_enabled_ && !wram_write_protected_) ? READ_WRITE : READ;
        } else {
            access = NO_ACCESS;
        }
        set_cpu_memory_mapping(0x6000, 0x7FFF, work_ram_.data(), 0, 0x2000, access);
    } else {
        for (uint16_t i = 0x60; i <= 0x7F; i++) {
            prg_pages_[i] = nullptr;
            prg_memory_access_[i] = NO_ACCESS;
        }
    }

    update_prg_mapping();
    update_chr_mapping();
}

void Mapper004::update_mirroring() {
    if (mirroring_type_ != MirroringType::FOUR_SCREENS) {
        set_mirroring_type((reg_a000_ & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
    }
}

void Mapper004::update_prg_mapping() {
    if (prg_mode_ == 0) {
        select_prg_page(0, registers_[6]);
        select_prg_page(1, registers_[7]);
        select_prg_page(2, (uint16_t)-2);
        select_prg_page(3, (uint16_t)-1);
    } else {
        select_prg_page(0, (uint16_t)-2);
        select_prg_page(1, registers_[7]);
        select_prg_page(2, registers_[6]);
        select_prg_page(3, (uint16_t)-1);
    }
}

void Mapper004::update_chr_mapping() {
    if (chr_mode_ == 0) {
        select_chr_page(0, registers_[0] & 0xFE);
        select_chr_page(1, registers_[0] | 0x01);
        select_chr_page(2, registers_[1] & 0xFE);
        select_chr_page(3, registers_[1] | 0x01);
        select_chr_page(4, registers_[2]);
        select_chr_page(5, registers_[3]);
        select_chr_page(6, registers_[4]);
        select_chr_page(7, registers_[5]);
    } else {
        select_chr_page(0, registers_[2]);
        select_chr_page(1, registers_[3]);
        select_chr_page(2, registers_[4]);
        select_chr_page(3, registers_[5]);
        select_chr_page(4, registers_[0] & 0xFE);
        select_chr_page(5, registers_[0] | 0x01);
        select_chr_page(6, registers_[1] & 0xFE);
        select_chr_page(7, registers_[1] | 0x01);
    }
}

} // namespace ear6::nes
