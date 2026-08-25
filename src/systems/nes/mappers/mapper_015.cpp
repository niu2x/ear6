#include "mapper_015.h"

namespace ear6::nes {

void Mapper015::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_chr_page(0, 0);
    write_register(0x8000, 0);
}

void Mapper015::reset(bool) {
    write_register(0x8000, 0);
}

void Mapper015::write_register(uint16_t addr, uint8_t value) {
    set_mirroring_type((value & 0x40) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);

    uint8_t sub_bank = value >> 7;
    uint8_t bank = (value & 0x7F) << 1;
    uint8_t mode = addr & 0x03;

    MemoryAccessType access = READ_WRITE;
    if (enable_chr_ram_write_protection_ && (mode == 0 || mode == 3)) {
        access = READ;
    }
    set_ppu_memory_mapping(0x0000, 0x1FFF, 0, ChrMemoryType::DEFAULT, access);

    switch (mode) {
        case 0:
            select_prg_page(0, bank ^ sub_bank);
            select_prg_page(1, (uint16_t)((bank + 1) ^ sub_bank));
            select_prg_page(2, (uint16_t)((bank + 2) ^ sub_bank));
            select_prg_page(3, (uint16_t)((bank + 3) ^ sub_bank));
            break;
        case 1:
        case 3:
            bank |= sub_bank;
            select_prg_page(0, bank);
            select_prg_page(1, bank + 1);
            bank = (uint8_t)(((mode == 3) ? bank : (bank | 0x0E)) | sub_bank);
            select_prg_page(2, bank + 0);
            select_prg_page(3, bank + 1);
            break;
        case 2:
            bank |= sub_bank;
            select_prg_page(0, bank);
            select_prg_page(1, bank);
            select_prg_page(2, bank);
            select_prg_page(3, bank);
            break;
    }
}

} // namespace ear6::nes
