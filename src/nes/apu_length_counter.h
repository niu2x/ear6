#pragma once

#include "nes_types.h"
#include <cstdint>

namespace ear6::nes {

class NesConsole;
class NesApu;

class ApuLengthCounter {
public:
    static constexpr uint8_t LOOKUP_TABLE[32] = {
        10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
    };

    ApuLengthCounter(AudioChannel channel, NesConsole*)
        : channel_(channel) {}

    void initialize_length_counter(bool halt_flag);
    void load_length_counter(uint8_t value);

    bool get_status() const { return counter_ > 0; }
    bool is_halted() const { return halt_; }

    void reload_counter() {
        if (reload_value_) {
            if (counter_ == previous_value_) {
                counter_ = reload_value_;
            }
            reload_value_ = 0;
        }
        halt_ = new_halt_value_;
    }

    void tick_length_counter() {
        if (counter_ > 0 && !halt_) {
            counter_--;
        }
    }

    void set_enabled(bool enabled) {
        if (!enabled) counter_ = 0;
        enabled_ = enabled;
    }

    bool is_enabled() const { return enabled_; }

    void set_apu(NesApu* apu) { apu_ = apu; }

    void reset(bool soft_reset) {
        if (soft_reset) {
            enabled_ = false;
            if (channel_ != AudioChannel::Triangle) {
                halt_ = false;
                counter_ = 0;
                new_halt_value_ = false;
                reload_value_ = 0;
                previous_value_ = 0;
            }
        } else {
            enabled_ = false;
            halt_ = false;
            counter_ = 0;
            new_halt_value_ = false;
            reload_value_ = 0;
            previous_value_ = 0;
        }
    }

private:
    NesApu* apu_ = nullptr;
    AudioChannel channel_ = AudioChannel::Square1;
    bool new_halt_value_ = false;
    bool enabled_ = false;
    bool halt_ = false;
    uint8_t counter_ = 0;
    uint8_t reload_value_ = 0;
    uint8_t previous_value_ = 0;
};

} // namespace ear6::nes
