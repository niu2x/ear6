#include "mapper_42.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper42::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    irq_counter_ = 0; irq_enabled_ = false; prg_reg_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, -4); select_prg_page(1, -3);
    select_prg_page(2, -2); select_prg_page(3, -1);
    select_chr_page(0, 0);
    update_state();
}

void Mapper42::update_state() {
    set_cpu_memory_mapping(0x6000, 0x7FFF, prg_reg_ & 0x0F, PrgMemoryType::PRG_ROM);
}

void Mapper42::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_++;
        if (irq_counter_ >= 0x8000) irq_counter_ -= 0x8000;
        if (irq_counter_ >= 0x6000) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        } else {
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper42::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE003) {
        case 0x8000:
            if (chr_rom_size_ > 0) select_chr_page(0, value & 0x0F);
            break;
        case 0xE000:
            prg_reg_ = value & 0x0F;
            update_state();
            break;
        case 0xE001:
            set_mirroring_type((value & 0x08) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            break;
        case 0xE002:
            irq_enabled_ = (value == 0x02);
            if (!irq_enabled_) {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
                irq_counter_ = 0;
            }
            break;
    }
}

} // namespace ear6::nes
