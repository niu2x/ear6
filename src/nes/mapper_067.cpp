#include "mapper_067.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper067::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    irq_counter_ = 0;
    irq_latch_ = false;
    irq_enabled_ = false;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    uint16_t last_bank = (prg_size_ / 0x4000) - 1;
    select_prg_page(1, last_bank);
}

void Mapper067::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_--;
        if (irq_counter_ == 0xFFFF) {
            irq_enabled_ = false;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper067::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF800) {
        case 0x8800: select_chr_page(0, value); break;
        case 0x9800: select_chr_page(1, value); break;
        case 0xA800: select_chr_page(2, value); break;
        case 0xB800: select_chr_page(3, value); break;
        case 0xC800:
            if (irq_latch_) {
                irq_counter_ = (irq_counter_ & 0xFF00) | value;
            } else {
                irq_counter_ = (irq_counter_ & 0x00FF) | ((uint16_t)value << 8);
            }
            irq_latch_ = !irq_latch_;
            break;
        case 0xD800:
            irq_enabled_ = (value & 0x10) != 0;
            irq_latch_ = false;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xE800:
            switch (value & 0x03) {
                case 0: set_mirroring_type(MirroringType::VERTICAL); break;
                case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
                case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
            }
            break;
        case 0xF800: select_prg_page(0, value); break;
    }
}

} // namespace ear6::nes
