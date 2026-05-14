#pragma once

#include "nes_types.h"
#include "nes_sound_mixer.h"
#include <cstdint>

namespace ear6::nes {

class ApuTimer {
public:
    ApuTimer() { reset(false); }

    void set_mixer(AudioChannel channel, NesSoundMixer* mixer) {
        channel_ = channel;
        mixer_ = mixer;
    }

    void reset(bool) {
        timer_ = 0;
        period_ = 0;
        previous_cycle_ = 0;
        last_output_ = 0;
    }

    void add_output(int8_t output) {
        if (output != last_output_ && mixer_) {
            mixer_->add_delta(channel_, previous_cycle_, output - last_output_);
            last_output_ = output;
        }
    }

    int8_t get_last_output() const { return last_output_; }

    bool run(uint32_t target_cycle) {
        int32_t cycles_to_run = target_cycle - previous_cycle_;
        if (cycles_to_run > timer_) {
            previous_cycle_ += timer_ + 1;
            timer_ = period_;
            return true;
        }
        timer_ -= cycles_to_run;
        previous_cycle_ = target_cycle;
        return false;
    }

    void set_period(uint16_t period) { period_ = period; }
    uint16_t get_period() const { return period_; }
    uint16_t get_timer() const { return timer_; }
    void set_timer(uint16_t t) { timer_ = t; }
    void end_frame() { previous_cycle_ = 0; }

private:
    uint32_t previous_cycle_ = 0;
    uint16_t timer_ = 0;
    uint16_t period_ = 0;
    int8_t last_output_ = 0;
    AudioChannel channel_ = AudioChannel::SQUARE1;
    NesSoundMixer* mixer_ = nullptr;
};

} // namespace ear6::nes
