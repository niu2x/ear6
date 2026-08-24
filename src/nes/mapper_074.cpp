#include "mapper_074.h"

namespace ear6::nes {

void Mapper074::select_chr_page(uint16_t slot, uint16_t page, ChrMemoryType type) {
    if (page >= 0x08 && page <= 0x09) {
        type = ChrMemoryType::CHR_RAM;
        page -= 0x08;
    }
    BaseMapper::select_chr_page(slot, page, type);
}

} // namespace ear6::nes
