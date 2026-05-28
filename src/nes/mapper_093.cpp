#include "mapper_093.h"

namespace ear6::nes {

void Mapper093::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    uint16_t last_bank = (prg_size_ / 0x4000) - 1;
    select_prg_page(1, last_bank);
    select_chr_page(0, 0);
}

void Mapper093::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    select_prg_page(0, (value >> 4) & 0x07);
    if ((value & 0x01) == 0x01) {
        select_chr_page(0, 0);
    } else {
        // CHR disabled - remove PPU memory mapping
        set_ppu_memory_mapping(0x0000, 0x1FFF, ChrMemoryType::CHR_RAM, 0, 0);
    }
}

} // namespace ear6::nes
