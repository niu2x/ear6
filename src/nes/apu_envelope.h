#pragma once

#include "apu_length_counter.h"

namespace ear6::nes {

class NesConsole;

class ApuEnvelope {
public:
    ApuLengthCounter length_counter;

    ApuEnvelope(AudioChannel channel, NesConsole* console)
        : length_counter(channel, console) {}

    void set_apu(NesApu* apu) { length_counter.set_apu(apu); }

    void initialize_envelope(uint8_t reg_value) {
        length_counter.initialize_length_counter((reg_value & 0x20) == 0x20);
        constant_volume_ = (reg_value & 0x10) == 0x10;
        volume_ = reg_value & 0x0F;
    }

    void reset_envelope() { start_ = true; }

    uint32_t get_volume() {
        if (length_counter.is_active()) {
            if (constant_volume_) return volume_;
            return counter_;
        }
        return 0;
    }

    void reset(bool soft_reset) {
        length_counter.reset(soft_reset);
        constant_volume_ = false;
        volume_ = 0;
        start_ = false;
        divider_ = 0;
        counter_ = 0;
    }

    void tick_envelope() {
        if (!start_) {
            divider_--;
            if (divider_ < 0) {
                divider_ = volume_;
                if (counter_ > 0) {
                    counter_--;
                } else if (length_counter.is_halted()) {
                    counter_ = 15;
                }
            }
        } else {
            start_ = false;
            counter_ = 15;
            divider_ = volume_;
        }
    }

private:
    bool constant_volume_ = false;
    uint8_t volume_ = 0;
    bool start_ = false;
    int8_t divider_ = 0;
    uint8_t counter_ = 0;
};

} // namespace ear6::nes
