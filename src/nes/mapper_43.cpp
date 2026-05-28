#include "mapper_43.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper43::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    irq_counter_ = 0; irq_enabled_ = false; swap_ = false; reg_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x4020, 0xFFFF, MemoryOperation::WRITE);
    update_state();
    set_cpu_memory_mapping(0x5000, 0x5FFF, 8, PrgMemoryType::PRG_ROM);
    select_prg_page(0, 1); select_prg_page(1, 0); select_chr_page(0, 0);
}

void Mapper43::update_state() {
    set_cpu_memory_mapping(0x6000, 0x7FFF, swap_ ? 0 : 2, PrgMemoryType::PRG_ROM);
    select_prg_page(2, reg_);
    select_prg_page(3, swap_ ? 8 : 9);
}

void Mapper43::process_cpu_clock() {
    if (irq_enabled_) {
        irq_counter_++;
        if (irq_counter_ >= 4096) {
            irq_enabled_ = false;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper43::write_register(uint16_t addr, uint8_t value) {
    static const int lut[8] = {4, 3, 5, 3, 6, 3, 7, 3};
    switch (addr & 0xF1FF) {
        case 0x4022: reg_ = (uint8_t)lut[value & 0x07]; update_state(); break;
        case 0x4120: swap_ = (value & 0x01) != 0; update_state(); break;
        case 0x4122:
        case 0x8122:
            irq_enabled_ = (value & 0x01) != 0;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            irq_counter_ = 0;
            break;
    }
}

} // namespace ear6::nes
