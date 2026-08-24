#include "mapper_099.h"

#include "nes_console.h"
#include "vs_control_manager.h"

namespace ear6::nes {

void Mapper099::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());
    prg_chr_select_bit_ = 0;

    set_mirroring_type(info.mirroring);
    select_prg_page(0, 0);
    select_prg_page(1, 1);
    select_prg_page(2, 2);
    select_prg_page(3, 3);
    select_chr_page(0, 0);
}

void Mapper099::setup_default_work_ram() {
    std::vector<uint8_t>& ram = rom_info_.has_battery ? save_ram_ : work_ram_;
    if (ram.size() < 0x0800) {
        return;
    }
    for (uint16_t start = 0x6000; start <= 0x7800; start += 0x0800) {
        set_cpu_memory_mapping(start, start + 0x07FF, ram.data(), 0,
                               static_cast<uint32_t>(ram.size()), READ_WRITE);
    }
}

void Mapper099::process_cpu_clock() {
    auto* control_manager = dynamic_cast<VsControlManager*>(
        console_->get_control_manager());
    if (!control_manager) {
        return;
    }
    uint8_t select_bit = control_manager->get_prg_chr_select_bit();
    if (select_bit == prg_chr_select_bit_) {
        return;
    }
    prg_chr_select_bit_ = select_bit;
    if (prg_size_ > 0x8000) {
        select_prg_page(0, prg_chr_select_bit_ << 2);
    }
    select_chr_page(0, prg_chr_select_bit_);
}

} // namespace ear6::nes
