#include "mapper_033.h"

namespace ear6::nes {

void Mapper033::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(2, -2);
    select_prg_page(3, -1);
}

void Mapper033::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xA003) {
        case 0x8000:
            select_prg_page(0, value & 0x3F);
            set_mirroring_type((value & 0x40) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            break;
        case 0x8001:
            select_prg_page(1, value & 0x3F);
            break;
        case 0x8002:
            select_chr_page(0, value * 2);
            select_chr_page(1, value * 2 + 1);
            break;
        case 0x8003:
            select_chr_page(2, value * 2);
            select_chr_page(3, value * 2 + 1);
            break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            select_chr_page(4 + (addr & 0x03), value);
            break;
    }
}

} // namespace ear6::nes
