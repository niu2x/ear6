#include "mapper_018.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper018::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    for (uint8_t& bank : prg_banks_) bank = 0;
    for (uint8_t& bank : chr_banks_) bank = 0;
    for (uint8_t& value : irq_reload_value_) value = 0;
    irq_counter_ = 0;
    irq_counter_size_ = 0;
    irq_enabled_ = false;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void Mapper018::update_prg_bank(uint8_t bank, uint8_t value, bool upper_bits) {
    if (upper_bits) {
        prg_banks_[bank] = (prg_banks_[bank] & 0x0F) | (value << 4);
    } else {
        prg_banks_[bank] = (prg_banks_[bank] & 0xF0) | value;
    }
    select_prg_page(bank, prg_banks_[bank]);
}

void Mapper018::update_chr_bank(uint8_t bank, uint8_t value, bool upper_bits) {
    if (upper_bits) {
        chr_banks_[bank] = (chr_banks_[bank] & 0x0F) | (value << 4);
    } else {
        chr_banks_[bank] = (chr_banks_[bank] & 0xF0) | value;
    }
    select_chr_page(bank, chr_banks_[bank]);
}

void Mapper018::reload_irq_counter() {
    irq_counter_ = irq_reload_value_[0] |
                   (irq_reload_value_[1] << 4) |
                   (irq_reload_value_[2] << 8) |
                   (irq_reload_value_[3] << 12);
}

void Mapper018::process_cpu_clock() {
    if (!irq_enabled_) {
        return;
    }

    uint16_t mask = IRQ_MASKS[irq_counter_size_];
    uint16_t counter = irq_counter_ & mask;
    counter--;
    if (counter == 0) {
        console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
    }
    irq_counter_ = (irq_counter_ & static_cast<uint16_t>(~mask)) | (counter & mask);
}

void Mapper018::write_register(uint16_t addr, uint8_t value) {
    bool upper_bits = (addr & 0x01) != 0;
    value &= 0x0F;

    switch (addr & 0xF003) {
        case 0x8000: case 0x8001: update_prg_bank(0, value, upper_bits); break;
        case 0x8002: case 0x8003: update_prg_bank(1, value, upper_bits); break;
        case 0x9000: case 0x9001: update_prg_bank(2, value, upper_bits); break;

        case 0xA000: case 0xA001: update_chr_bank(0, value, upper_bits); break;
        case 0xA002: case 0xA003: update_chr_bank(1, value, upper_bits); break;
        case 0xB000: case 0xB001: update_chr_bank(2, value, upper_bits); break;
        case 0xB002: case 0xB003: update_chr_bank(3, value, upper_bits); break;
        case 0xC000: case 0xC001: update_chr_bank(4, value, upper_bits); break;
        case 0xC002: case 0xC003: update_chr_bank(5, value, upper_bits); break;
        case 0xD000: case 0xD001: update_chr_bank(6, value, upper_bits); break;
        case 0xD002: case 0xD003: update_chr_bank(7, value, upper_bits); break;

        case 0xE000: case 0xE001: case 0xE002: case 0xE003:
            irq_reload_value_[addr & 0x03] = value;
            break;
        case 0xF000:
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            reload_irq_counter();
            break;
        case 0xF001:
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            irq_enabled_ = (value & 0x01) != 0;
            if (value & 0x08) irq_counter_size_ = 3;
            else if (value & 0x04) irq_counter_size_ = 2;
            else if (value & 0x02) irq_counter_size_ = 1;
            else irq_counter_size_ = 0;
            break;
        case 0xF002:
            switch (value & 0x03) {
                case 0: set_mirroring_type(MirroringType::HORIZONTAL); break;
                case 1: set_mirroring_type(MirroringType::VERTICAL); break;
                case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
            }
            break;
        case 0xF003:
            break;
        default:
            break;
    }
}

} // namespace ear6::nes
