#include "mapper_40.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void Mapper40::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    irq_counter_ = 0;
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    set_cpu_memory_mapping(0x6000, 0x7FFF, 6, PrgMemoryType::PRG_ROM);
    select_prg_page(0, 4); select_prg_page(1, 5);
    select_prg_page(3, 7); select_chr_page(0, 0);
}

void Mapper40::process_cpu_clock() {
    if (irq_counter_ > 0) {
        irq_counter_--;
        if (irq_counter_ == 0) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper40::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE000) {
        case 0x8000:
            irq_counter_ = 0;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xA000:
            irq_counter_ = 4096;
            break;
        case 0xE000:
            select_prg_page(2, value);
            break;
    }
}

} // namespace ear6::nes
