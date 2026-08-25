#include "mapper_233.h"

namespace ear6::nes {

void Mapper233::reset(bool soft_reset) {
    if (!soft_reset) {
        reset_flag_ = 0;
    }
    Mapper226::reset(soft_reset);
    if (soft_reset) {
        reset_flag_ ^= 0x01;
    }
    update_prg();
}

uint8_t Mapper233::get_prg_page_inner() {
    return (registers_[0] & 0x1F) | (reset_flag_ << 5) | ((registers_[1] & 0x01) << 6);
}

} // namespace ear6::nes
