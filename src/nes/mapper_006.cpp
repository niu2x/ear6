#include "mapper_006.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper006::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    irq_counter_ = 0;
    irq_enabled_ = false;
    ffe_alt_mode_ = true;

    add_register_range(0x42FE, 0x42FF, MemoryOperation::WRITE);
    add_register_range(0x4501, 0x4503, MemoryOperation::WRITE);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    select_prg_page_2x(0, 0);
    select_prg_page_2x(1, 14);
}

void Mapper006::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_++;
        if (irq_counter_ == 0) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
            irq_enabled_ = false;
        }
    }
}

void Mapper006::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x42FE:
            ffe_alt_mode_ = (value & 0x80) == 0;
            set_mirroring_type(((value >> 4) & 0x01) ? MirroringType::SCREEN_B_ONLY : MirroringType::SCREEN_A_ONLY);
            return;

        case 0x42FF:
            set_mirroring_type(((value >> 4) & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            return;

        case 0x4501:
            irq_enabled_ = false;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            return;

        case 0x4502:
            irq_counter_ = (uint16_t)((irq_counter_ & 0xFF00) | value);
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            return;

        case 0x4503:
            irq_counter_ = (uint16_t)((irq_counter_ & 0x00FF) | (value << 8));
            irq_enabled_ = true;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            return;

        default:
            break;
    }

    if (addr >= 0x8000) {
        uint8_t chr_bank = value;
        if (has_chr_ram() || ffe_alt_mode_) {
            select_prg_page_2x(0, (value & 0xFC) >> 1);
            chr_bank = value & 0x03;
        }
        select_chr_page_8x(0, chr_bank << 3);
    }
}

} // namespace ear6::nes
