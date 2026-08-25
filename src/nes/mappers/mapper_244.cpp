#include "mapper_244.h"

namespace ear6::nes {

static constexpr uint8_t LUT_PRG[4][4] = {
    {0, 1, 2, 3}, {3, 2, 1, 0}, {0, 2, 1, 3}, {3, 1, 2, 0}
};

static constexpr uint8_t LUT_CHR[8][8] = {
    {0, 1, 2, 3, 4, 5, 6, 7}, {0, 2, 1, 3, 4, 6, 5, 7},
    {0, 1, 4, 5, 2, 3, 6, 7}, {0, 4, 1, 5, 2, 6, 3, 7},
    {0, 4, 2, 6, 1, 5, 3, 7}, {0, 2, 4, 6, 1, 3, 5, 7},
    {7, 6, 5, 4, 3, 2, 1, 0}, {7, 6, 5, 4, 3, 2, 1, 0}
};

void Mapper244::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0); select_chr_page(0, 0);
}

void Mapper244::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    if (value & 0x08) {
        select_chr_page(0, LUT_CHR[(value >> 4) & 0x07][value & 0x07]);
    } else {
        select_prg_page(0, LUT_PRG[(value >> 4) & 0x03][value & 0x03]);
    }
}

} // namespace ear6::nes
