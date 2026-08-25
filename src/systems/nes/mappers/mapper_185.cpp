#include "mapper_185.h"

namespace ear6::nes {

void Mapper185::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    set_has_bus_conflicts(true);
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    select_prg_page(0, 0);
    select_chr_page(0, 0);
}

void Mapper185::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    bool chr_enabled;
    switch (rom_info_.submapper_id) {
        case 0:
            chr_enabled = ((value & 0x0F) != 0 && value != 0x13);
            break;
        case 4:
            chr_enabled = ((value & 0x03) == 0);
            break;
        case 5:
            chr_enabled = ((value & 0x03) == 1);
            break;
        case 6:
            chr_enabled = ((value & 0x03) == 2);
            break;
        case 7:
            chr_enabled = ((value & 0x03) == 3);
            break;
        default:
            chr_enabled = ((value & 0x0F) != 0 && value != 0x13);
            break;
    }

    if (chr_enabled) {
        select_chr_page(0, 0);
    } else {
        set_ppu_memory_mapping(0x0000, 0x1FFF, ChrMemoryType::CHR_RAM, 0, 0);
    }
}

uint8_t Mapper185::read_vram_custom(uint16_t addr) {
    uint8_t value = read_vram(addr);
    if (addr < 0x2000) {
        value |= 0x01;
    }
    return value;
}

} // namespace ear6::nes
