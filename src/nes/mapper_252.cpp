#include "mapper_252.h"

namespace ear6::nes {

void Mapper252::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    chr_registers_.fill(0);
    irq_.initialize(console_);
    irq_.reset();

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(2, static_cast<uint16_t>(-2));
    select_prg_page(3, static_cast<uint16_t>(-1));
    update_state();
}

void Mapper252::update_state() {
    for (uint8_t i = 0; i < chr_registers_.size(); i++) {
        set_ppu_memory_mapping(i * 0x0400, i * 0x0400 + 0x03FF,
                               chr_registers_[i], ChrMemoryType::DEFAULT,
                               READ_WRITE);
    }
}

void Mapper252::process_cpu_clock() {
    irq_.process_cpu_clock();
}

void Mapper252::write_register(uint16_t addr, uint8_t value) {
    if (addr <= 0x8FFF) {
        select_prg_page(0, value);
    } else if (addr >= 0xA000 && addr <= 0xAFFF) {
        select_prg_page(1, value);
    } else if (addr >= 0xB000 && addr <= 0xEFFF) {
        uint8_t shift = addr & 0x04;
        uint16_t bank_bits = ((addr - 0xB000) >> 1) & 0x1800;
        bank_bits |= (addr << 7) & 0x0400;
        uint8_t bank = static_cast<uint8_t>(bank_bits / 0x0400);
        chr_registers_[bank] = (chr_registers_[bank] & (0xF0 >> shift))
                               | ((value & 0x0F) << shift);
        update_state();
    } else {
        switch (addr & 0xF00C) {
            case 0xF000: irq_.set_reload_value_nibble(value, false); break;
            case 0xF004: irq_.set_reload_value_nibble(value, true); break;
            case 0xF008: irq_.set_control_value(value); break;
            case 0xF00C: irq_.acknowledge_irq(); break;
        }
    }
}

} // namespace ear6::nes
