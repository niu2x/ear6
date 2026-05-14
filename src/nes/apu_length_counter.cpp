#include "apu_length_counter.h"
#include "nes_apu.h"
#include "nes_console.h"

namespace ear6::nes {

void ApuLengthCounter::initialize_length_counter(bool halt_flag) {
    if (apu_) apu_->set_need_to_run();
    new_halt_value_ = halt_flag;
}

void ApuLengthCounter::load_length_counter(uint8_t value) {
    if (enabled_) {
        reload_value_ = LOOKUP_TABLE[value];
        previous_value_ = counter_;
        if (apu_) apu_->set_need_to_run();
    }
}

} // namespace ear6::nes
