#include "mapper_035.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper035::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    irq_counter_ = 0;
    irq_enabled_ = false;
    a12_low_clock_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void Mapper035::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF007) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            select_prg_page(addr & 0x03, value);
            break;
        case 0x9000: case 0x9001: case 0x9002: case 0x9003:
        case 0x9004: case 0x9005: case 0x9006: case 0x9007:
            select_chr_page(addr & 0x07, value);
            break;
        case 0xC002:
            irq_enabled_ = false;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xC003:
            irq_enabled_ = true;
            break;
        case 0xC005:
            irq_counter_ = value;
            break;
        case 0xD001:
            set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL
                                               : MirroringType::VERTICAL);
            break;
        default:
            break;
    }
}

bool Mapper035::is_a12_rising_edge(uint16_t addr) {
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

void Mapper035::notify_vram_address_change(uint16_t addr) {
    if (!is_a12_rising_edge(addr) || !irq_enabled_) {
        return;
    }
    irq_counter_--;
    if (irq_counter_ == 0) {
        irq_enabled_ = false;
        console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
    }
}

} // namespace ear6::nes
