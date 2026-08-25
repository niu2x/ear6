#include "mapper_50.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper50::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    irq_counter_ = 0; irq_enabled_ = false;
    set_mirroring_type(info.mirroring);
    add_register_range(0x4020, 0x5FFF, MemoryOperation::WRITE);
    set_cpu_memory_mapping(0x6000, 0x7FFF, 0x0F, PrgMemoryType::PRG_ROM);
    select_prg_page(0, 0x08); select_prg_page(1, 0x09);
    select_prg_page(3, 0x0B); select_chr_page(0, 0);
}

void Mapper50::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_++;
        if (irq_counter_ == 0x1000) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
            irq_enabled_ = false;
        }
    }
}

void Mapper50::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0x4120) {
        case 0x4020:
            select_prg_page(2, (value & 0x08) | ((value & 0x01) << 2) | ((value & 0x06) >> 1));
            break;
        case 0x4120:
            if (value & 0x01) {
                irq_enabled_ = true;
            } else {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
                irq_counter_ = 0;
                irq_enabled_ = false;
            }
            break;
    }
}

} // namespace ear6::nes
