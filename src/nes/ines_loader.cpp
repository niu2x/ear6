#include "ines_loader.h"
#include <cstring>

namespace ear6::nes {

bool INesLoader::is_valid(const uint8_t* data, int size) {
    if (size < 16) return false;
    return data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

RomInfo INesLoader::parse_header(const uint8_t* data) {
    RomInfo info;
    info.prg_banks = data[4];
    info.chr_banks = data[5];
    info.mapper_number = (data[6] >> 4) | (data[7] & 0xF0);
    info.has_battery = (data[6] & 0x02) != 0;
    info.has_trainer = (data[6] & 0x04) != 0;
    info.mirroring = (data[6] & 0x01) ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    if (data[6] & 0x08) {
        info.mirroring = MirroringType::FOUR_SCREENS;
    }
    // byte 7 bit 0 = VS flag → mesen2 uses 2C03 RGB palette even when system is not VsSystem
    info.is_vs_system = false;  // mesen2 detects VS from filename, not from header
    info.use_vs_palette = (data[7] & 0x01) != 0;
    return info;
}

} // namespace ear6::nes
