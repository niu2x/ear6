#include "square_channel.h"
#include "nes_apu.h"
#include "nes_console.h"

namespace ear6::nes {

void SquareChannel::get_memory_ranges(MemoryRanges& ranges) {
    if (is_channel1_)
        ranges.add_handler(MemoryOperation::Write, 0x4000, 0x4003);
    else
        ranges.add_handler(MemoryOperation::Write, 0x4004, 0x4007);
}

void SquareChannel::write_ram(uint16_t addr, uint8_t value) {
    console_->get_apu()->run();

    switch (addr & 0x03) {
        case 0:
            envelope_.initialize_envelope(value);
            duty_ = (value & 0xC0) >> 6;
            break;

        case 1:
            initialize_sweep(value);
            break;

        case 2:
            set_period((real_period_ & 0x0700) | value);
            break;

        case 3:
            envelope_.length_counter.load_length_counter(value >> 3);
            set_period((real_period_ & 0xFF) | ((value & 0x07) << 8));
            duty_pos_ = 0;
            envelope_.reset_envelope();
            break;
    }

    update_output();
}

void SquareChannel::tick_sweep() {
    sweep_divider_--;
    if (sweep_divider_ == 0) {
        if (sweep_shift_ > 0 && sweep_enabled_ && real_period_ >= 8 && sweep_target_period_ <= 0x7FF) {
            set_period(sweep_target_period_);
        }
        sweep_divider_ = sweep_period_;
    }
    if (reload_sweep_) {
        sweep_divider_ = sweep_period_;
        reload_sweep_ = false;
    }
}

} // namespace ear6::nes
