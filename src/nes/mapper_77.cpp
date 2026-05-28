#include "mapper_77.h"

namespace ear6::nes {

void Mapper77::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(MirroringType::FOUR_SCREENS);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(0, 0);
    select_chr_page(0, 0);
    if (!chr_rom_.empty()) {
        select_chr_page(1, 0, ChrMemoryType::CHR_RAM);
        select_chr_page(2, 0, ChrMemoryType::CHR_RAM);
        select_chr_page(3, 0, ChrMemoryType::CHR_RAM);
    }
}

void Mapper77::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    select_prg_page(0, value & 0x0F);
    select_chr_page(0, (value >> 4) & 0x0F);
}

} // namespace ear6::nes
