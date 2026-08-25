#include "mapper_065.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper065::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    irq_enabled_ = false; irq_counter_ = 0; irq_reload_value_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_prg_page(1, 1);
    select_prg_page(2, 0xFE); select_prg_page(3, -1);
}

void Mapper065::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_--;
        if (irq_counter_ == 0) {
            irq_enabled_ = false;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper065::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x8000: select_prg_page(0, value); break;
        case 0x9001: set_mirroring_type((value & 0x80) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL); break;
        case 0x9003:
            irq_enabled_ = (value & 0x80) != 0;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0x9004:
            irq_counter_ = irq_reload_value_;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0x9005: irq_reload_value_ = (irq_reload_value_ & 0x00FF) | ((uint16_t)value << 8); break;
        case 0x9006: irq_reload_value_ = (irq_reload_value_ & 0xFF00) | value; break;
        case 0xA000: select_prg_page(1, value); break;
        case 0xB000: select_chr_page(0, value); break;
        case 0xB001: select_chr_page(1, value); break;
        case 0xB002: select_chr_page(2, value); break;
        case 0xB003: select_chr_page(3, value); break;
        case 0xB004: select_chr_page(4, value); break;
        case 0xB005: select_chr_page(5, value); break;
        case 0xB006: select_chr_page(6, value); break;
        case 0xB007: select_chr_page(7, value); break;
        case 0xC000: select_prg_page(2, value); break;
    }
}

} // namespace ear6::nes
