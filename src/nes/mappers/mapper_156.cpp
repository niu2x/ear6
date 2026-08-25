#include "mapper_156.h"
#include <cstring>

namespace ear6::nes {

void Mapper156::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    memset(chr_low_, 0, sizeof(chr_low_));
    memset(chr_high_, 0, sizeof(chr_high_));
    set_mirroring_type(MirroringType::SCREEN_A_ONLY);
    add_register_range(0xC000, 0xC014, MemoryOperation::WRITE);
    uint16_t last_bank = (prg_size_ / 0x4000) - 1;
    select_prg_page(1, last_bank);
}

void Mapper156::update_chr_banks() {
    for (int i = 0; i < 8; i++) {
        select_chr_page(i, ((uint16_t)chr_high_[i] << 8) | chr_low_[i]);
    }
}

void Mapper156::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0xC000: case 0xC001: case 0xC002: case 0xC003:
        case 0xC004: case 0xC005: case 0xC006: case 0xC007:
        case 0xC008: case 0xC009: case 0xC00A: case 0xC00B:
        case 0xC00C: case 0xC00D: case 0xC00E: case 0xC00F: {
            uint8_t bank = (addr & 0x03) + ((addr >= 0xC008) ? 4 : 0);
            uint8_t* arr = (addr & 0x04) ? chr_high_ : chr_low_;
            arr[bank] = value;
            update_chr_banks();
            break;
        }
        case 0xC010:
            select_prg_page(0, value);
            break;
        case 0xC014:
            set_mirroring_type((value & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
            break;
    }
}

} // namespace ear6::nes
