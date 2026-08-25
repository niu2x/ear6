#include "mapper_064.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper064::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    registers_.fill(0);
    current_register_ = 0;
    irq_enabled_ = false;
    irq_cycle_mode_ = false;
    need_reload_ = false;
    irq_counter_ = 0;
    irq_reload_value_ = 0;
    cpu_clock_counter_ = 0;
    irq_delay_ = 0;
    force_clock_ = false;
    a12_low_clock_ = 0;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void Mapper064::update_state() {
    if ((current_register_ & 0x40) != 0) {
        select_prg_page(0, registers_[15]);
        select_prg_page(1, registers_[6]);
        select_prg_page(2, registers_[7]);
    } else {
        select_prg_page(0, registers_[6]);
        select_prg_page(1, registers_[7]);
        select_prg_page(2, registers_[15]);
    }

    uint8_t a12_inversion = (current_register_ & 0x80) != 0 ? 0x04 : 0x00;
    select_chr_page(0 ^ a12_inversion, registers_[0]);
    select_chr_page(2 ^ a12_inversion, registers_[1]);
    select_chr_page(4 ^ a12_inversion, registers_[2]);
    select_chr_page(5 ^ a12_inversion, registers_[3]);
    select_chr_page(6 ^ a12_inversion, registers_[4]);
    select_chr_page(7 ^ a12_inversion, registers_[5]);

    if ((current_register_ & 0x20) != 0) {
        select_chr_page(1 ^ a12_inversion, registers_[8]);
        select_chr_page(3 ^ a12_inversion, registers_[9]);
    } else {
        select_chr_page(1 ^ a12_inversion, registers_[0] + 1);
        select_chr_page(3 ^ a12_inversion, registers_[1] + 1);
    }
}

void Mapper064::clock_irq_counter(uint8_t delay) {
    if (need_reload_) {
        irq_counter_ = irq_reload_value_ <= 1
                       ? irq_reload_value_ + 1
                       : irq_reload_value_ + 2;
        need_reload_ = false;
    } else if (irq_counter_ == 0) {
        irq_counter_ = irq_reload_value_ + 1;
    }

    irq_counter_--;
    if (irq_counter_ == 0 && irq_enabled_) {
        irq_delay_ = delay;
    }
}

void Mapper064::process_cpu_clock() {
    if (irq_delay_ > 0) {
        irq_delay_--;
        if (irq_delay_ == 0) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }

    if (irq_cycle_mode_ || force_clock_) {
        cpu_clock_counter_ = (cpu_clock_counter_ + 1) & 0x03;
        if (cpu_clock_counter_ == 0) {
            clock_irq_counter(1);
            force_clock_ = false;
        }
    }
}

bool Mapper064::is_a12_rising_edge(uint16_t addr) {
    constexpr uint64_t A12_FILTER_CPU_CYCLES = 10;
    uint64_t clock = console_->get_cpu()->get_cycle_count();
    if ((addr & 0x1000) != 0) {
        bool rising = a12_low_clock_ > 0
                      && clock - a12_low_clock_ >= A12_FILTER_CPU_CYCLES;
        a12_low_clock_ = 0;
        return rising;
    }
    if (a12_low_clock_ == 0) {
        a12_low_clock_ = clock;
    }
    return false;
}

void Mapper064::notify_vram_address_change(uint16_t addr) {
    if (!irq_cycle_mode_ && is_a12_rising_edge(addr)) {
        clock_irq_counter(2);
    }
}

void Mapper064::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE001) {
        case 0x8000:
            current_register_ = value;
            break;
        case 0x8001:
            registers_[current_register_ & 0x0F] = value;
            update_state();
            break;
        case 0xA000:
            set_mirroring_type((value & 0x01) != 0
                               ? MirroringType::HORIZONTAL
                               : MirroringType::VERTICAL);
            break;
        case 0xC000:
            irq_reload_value_ = value;
            break;
        case 0xC001:
            if (irq_cycle_mode_ && (value & 0x01) == 0) {
                force_clock_ = true;
            }
            irq_cycle_mode_ = (value & 0x01) != 0;
            if (irq_cycle_mode_) {
                cpu_clock_counter_ = 0;
            }
            need_reload_ = true;
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

} // namespace ear6::nes
