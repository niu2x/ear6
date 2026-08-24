#include "mapper_118.h"

namespace ear6::nes {

void Mapper118::write_register(uint16_t addr, uint8_t value) {
    if ((addr & 0xE001) == 0x8001) {
        uint8_t nametable = value >> 7;
        if (chr_mode_ == 0) {
            if (current_register_ == 0) {
                set_nametable(0, nametable);
                set_nametable(1, nametable);
            } else if (current_register_ == 1) {
                set_nametable(2, nametable);
                set_nametable(3, nametable);
            }
        } else {
            switch (current_register_) {
                case 2: set_nametable(0, nametable); break;
                case 3: set_nametable(1, nametable); break;
                case 4: set_nametable(2, nametable); break;
                case 5: set_nametable(3, nametable); break;
            }
        }
    }
    Mapper004::write_register(addr, value);
}

} // namespace ear6::nes
