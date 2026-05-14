#include "noise_channel.h"
#include "nes_apu.h"
#include "nes_console.h"

namespace ear6::nes {

void NoiseChannel::run(uint32_t target_cycle) {
    while (timer_.run(target_cycle)) {
        uint16_t feedback = (shift_register_ & 0x01) ^
            ((shift_register_ >> (mode_flag_ ? 6 : 1)) & 0x01);
        shift_register_ >>= 1;
        shift_register_ |= (feedback << 14);

        if (is_muted()) {
            timer_.add_output(0);
        } else {
            timer_.add_output((int8_t)envelope_.get_volume());
        }
    }
}

void NoiseChannel::reset(bool soft_reset) {
    envelope_.reset(soft_reset);
    timer_.reset(soft_reset);
    timer_.set_period(NOISE_PERIOD_TABLE_NTSC[0] - 1);
    shift_register_ = 1;
    mode_flag_ = false;
}

void NoiseChannel::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::Write, 0x400C, 0x400F);
}

void NoiseChannel::write_ram(uint16_t addr, uint8_t value) {
    console_->get_apu()->run();

    switch (addr & 0x03) {
        case 0:
            envelope_.initialize_envelope(value);
            break;

        case 2:
            timer_.set_period(NOISE_PERIOD_TABLE_NTSC[value & 0x0F] - 1);
            mode_flag_ = (value & 0x80) == 0x80;
            break;

        case 3:
            envelope_.length_counter.load_length_counter(value >> 3);
            envelope_.reset_envelope();
            break;
    }
}

} // namespace ear6::nes
