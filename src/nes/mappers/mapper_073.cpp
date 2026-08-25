#include "mapper_073.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper073::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    irq_enable_on_ack_ = false;
    irq_enabled_ = false;
    small_counter_ = false;
    irq_reload_ = 0;
    irq_counter_ = 0;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(1, static_cast<uint16_t>(-1));
    select_chr_page(0, 0);
}

void Mapper073::process_cpu_clock() {
    if (!irq_enabled_) {
        return;
    }

    if (small_counter_) {
        uint8_t counter = static_cast<uint8_t>(irq_counter_);
        counter++;
        if (counter == 0) {
            counter = static_cast<uint8_t>(irq_reload_);
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
        irq_counter_ = (irq_counter_ & 0xFF00) | counter;
    } else {
        irq_counter_++;
        if (irq_counter_ == 0) {
            irq_counter_ = irq_reload_;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper073::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000) {
        case 0x8000:
            irq_reload_ = (irq_reload_ & 0xFFF0) | (value & 0x0F);
            break;
        case 0x9000:
            irq_reload_ = (irq_reload_ & 0xFF0F) | ((value & 0x0F) << 4);
            break;
        case 0xA000:
            irq_reload_ = (irq_reload_ & 0xF0FF) | ((value & 0x0F) << 8);
            break;
        case 0xB000:
            irq_reload_ = (irq_reload_ & 0x0FFF) | ((value & 0x0F) << 12);
            break;
        case 0xC000:
            irq_enabled_ = (value & 0x02) != 0;
            if (irq_enabled_) {
                irq_counter_ = irq_reload_;
            }
            small_counter_ = (value & 0x04) != 0;
            irq_enable_on_ack_ = (value & 0x01) != 0;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xD000:
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            irq_enabled_ = irq_enable_on_ack_;
            break;
        case 0xF000:
            select_prg_page(0, value & 0x07);
            break;
    }
}

} // namespace ear6::nes
