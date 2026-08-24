#include "vrc_irq.h"

#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

void VrcIrq::reset() {
    reload_value_ = 0;
    counter_ = 0;
    prescaler_counter_ = 0;
    enabled_ = false;
    enabled_after_ack_ = false;
    cycle_mode_ = false;
}

void VrcIrq::process_cpu_clock() {
    if (!enabled_) {
        return;
    }

    prescaler_counter_ -= 3;
    if (cycle_mode_ || prescaler_counter_ <= 0) {
        if (counter_ == 0xFF) {
            counter_ = reload_value_;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        } else {
            counter_++;
        }
        prescaler_counter_ += 341;
    }
}

void VrcIrq::set_reload_value_nibble(uint8_t value, bool high_bits) {
    if (high_bits) {
        reload_value_ = (reload_value_ & 0x0F) | ((value & 0x0F) << 4);
    } else {
        reload_value_ = (reload_value_ & 0xF0) | (value & 0x0F);
    }
}

void VrcIrq::set_control_value(uint8_t value) {
    enabled_after_ack_ = (value & 0x01) != 0;
    enabled_ = (value & 0x02) != 0;
    cycle_mode_ = (value & 0x04) != 0;
    if (enabled_) {
        counter_ = reload_value_;
        prescaler_counter_ = 341;
    }
    console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
}

void VrcIrq::acknowledge_irq() {
    enabled_ = enabled_after_ack_;
    console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
}

} // namespace ear6::nes
