#include "triangle_channel.h"
#include "nes_apu.h"
#include "nes_console.h"

namespace ear6::nes {

void TriangleChannel::run(uint32_t target_cycle) {
    while (timer_.run(target_cycle)) {
        if (length_counter_.is_active() && linear_counter_ > 0) {
            sequence_position_ = (sequence_position_ + 1) & 0x1F;
            timer_.add_output(SEQUENCE[sequence_position_]);
        }
    }
}

void TriangleChannel::reset(bool soft_reset) {
    timer_.reset(soft_reset);
    length_counter_.reset(soft_reset);
    linear_counter_ = 0;
    linear_counter_reload_ = 0;
    linear_reload_flag_ = false;
    linear_control_flag_ = false;
    sequence_position_ = 0;
}

void TriangleChannel::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::WRITE, 0x4008, 0x400B);
}

void TriangleChannel::write_ram(uint16_t addr, uint8_t value) {
    console_->get_apu()->run();

    switch (addr & 0x03) {
        case 0:
            linear_control_flag_ = (value & 0x80) == 0x80;
            linear_counter_reload_ = value & 0x7F;
            length_counter_.initialize_length_counter(linear_control_flag_);
            break;

        case 2:
            timer_.set_period((timer_.get_period() & 0xFF00) | value);
            break;

        case 3:
            length_counter_.load_length_counter(value >> 3);
            timer_.set_period((timer_.get_period() & 0xFF) | ((value & 0x07) << 8));
            linear_reload_flag_ = true;
            break;
    }
}

void TriangleChannel::tick_linear_counter() {
    if (linear_reload_flag_) {
        linear_counter_ = linear_counter_reload_;
    } else if (linear_counter_ > 0) {
        linear_counter_--;
    }
    if (!linear_control_flag_) {
        linear_reload_flag_ = false;
    }
}

} // namespace ear6::nes
