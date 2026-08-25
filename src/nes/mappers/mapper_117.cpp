#include "mapper_117.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper117::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    irq_counter_ = 0;
    irq_reload_value_ = 0;
    irq_enabled_ = false;
    irq_enabled_alt_ = false;
    a12_low_clock_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, static_cast<uint16_t>(-4));
    select_prg_page(1, static_cast<uint16_t>(-3));
    select_prg_page(2, static_cast<uint16_t>(-2));
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void Mapper117::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            select_prg_page(addr & 0x03, value);
            break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
        case 0xA004: case 0xA005: case 0xA006: case 0xA007:
            select_chr_page(addr & 0x07, value);
            break;
        case 0xC001:
            irq_reload_value_ = value;
            break;
        case 0xC002:
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xC003:
            irq_counter_ = irq_reload_value_;
            irq_enabled_alt_ = true;
            break;
        case 0xD000:
            set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL
                                               : MirroringType::VERTICAL);
            break;
        case 0xE000:
            irq_enabled_ = (value & 0x01) != 0;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        default:
            break;
    }
}

bool Mapper117::is_a12_rising_edge(uint16_t addr) {
    constexpr uint64_t A12_FILTER_CPU_CYCLES = 3;
    uint64_t clock = console_->get_cpu()->get_cycle_count();
    if (addr & 0x1000) {
        bool rising = a12_low_clock_ > 0 &&
            (clock - a12_low_clock_) >= A12_FILTER_CPU_CYCLES;
        a12_low_clock_ = 0;
        return rising;
    }
    if (a12_low_clock_ == 0) {
        a12_low_clock_ = clock;
    }
    return false;
}

void Mapper117::notify_vram_address_change(uint16_t addr) {
    if (!is_a12_rising_edge(addr) || !irq_enabled_ || !irq_enabled_alt_ || irq_counter_ == 0) {
        return;
    }
    irq_counter_--;
    if (irq_counter_ == 0) {
        console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        irq_enabled_alt_ = false;
    }
}

} // namespace ear6::nes
