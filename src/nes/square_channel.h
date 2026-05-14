#pragma once

#include "apu_envelope.h"
#include "apu_timer.h"
#include "ines_memory_handler.h"

namespace ear6::nes {

class NesConsole;
class NesSoundMixer;

class SquareChannel : public INesMemoryHandler {
public:
    static constexpr uint8_t DUTY_SEQUENCES[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 1, 1},
        {0, 0, 0, 0, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 0, 0}
    };

    SquareChannel(AudioChannel channel, NesConsole* console, bool is_channel1)
        : envelope_(channel, console), timer_() {
        console_ = console;
        channel_ = channel;
        is_channel1_ = is_channel1;
    }

    void set_mixer(NesSoundMixer* mixer) { timer_.set_mixer(channel_, mixer); }
    void set_apu(NesApu* apu) { envelope_.set_apu(apu); }

    void run(uint32_t target_cycle) {
        while (timer_.run(target_cycle)) {
            duty_pos_ = (duty_pos_ - 1) & 0x07;
            update_output();
        }
    }

    void reset(bool soft_reset) {
        envelope_.reset(soft_reset);
        timer_.reset(soft_reset);
        duty_ = 0;
        duty_pos_ = 0;
        real_period_ = 0;
        sweep_enabled_ = false;
        sweep_period_ = 0;
        sweep_negate_ = false;
        sweep_shift_ = 0;
        reload_sweep_ = false;
        sweep_divider_ = 0;
        sweep_target_period_ = 0;
        update_target_period();
    }

    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override { (void)addr; return 0; }
    void write_ram(uint16_t addr, uint8_t value) override;

    void tick_sweep();
    void tick_envelope() { envelope_.tick_envelope(); }
    void tick_length_counter() { envelope_.length_counter.tick_length_counter(); }
    void reload_length_counter() { envelope_.length_counter.reload_counter(); }
    void end_frame() { timer_.end_frame(); }
    void set_enabled(bool enabled) { envelope_.length_counter.set_enabled(enabled); }
    bool get_status() { return envelope_.length_counter.get_status(); }
    uint8_t get_output() { return (uint8_t)timer_.get_last_output(); }

private:
    bool is_muted() {
        return real_period_ < 8 || (!sweep_negate_ && sweep_target_period_ > 0x7FF);
    }

    void initialize_sweep(uint8_t reg_value) {
        sweep_enabled_ = (reg_value & 0x80) == 0x80;
        sweep_negate_ = (reg_value & 0x08) == 0x08;
        sweep_period_ = ((reg_value & 0x70) >> 4) + 1;
        sweep_shift_ = (reg_value & 0x07);
        update_target_period();
        reload_sweep_ = true;
    }

    void update_target_period() {
        uint16_t shift_result = (real_period_ >> sweep_shift_);
        if (sweep_negate_) {
            sweep_target_period_ = real_period_ - shift_result;
            if (is_channel1_) sweep_target_period_--;
        } else {
            sweep_target_period_ = real_period_ + shift_result;
        }
    }

    void set_period(uint16_t new_period) {
        real_period_ = new_period;
        timer_.set_period((real_period_ * 2) + 1);
        update_target_period();
    }

    void update_output() {
        if (is_muted()) {
            timer_.add_output(0);
        } else {
            timer_.add_output(DUTY_SEQUENCES[duty_][duty_pos_] * (int8_t)envelope_.get_volume());
        }
    }

    NesConsole* console_ = nullptr;
    ApuEnvelope envelope_;
    ApuTimer timer_;
    AudioChannel channel_ = AudioChannel::Square1;
    bool is_channel1_ = false;
    uint8_t duty_ = 0;
    uint8_t duty_pos_ = 0;
    bool sweep_enabled_ = false;
    uint8_t sweep_period_ = 0;
    bool sweep_negate_ = false;
    uint8_t sweep_shift_ = 0;
    bool reload_sweep_ = false;
    uint8_t sweep_divider_ = 0;
    uint32_t sweep_target_period_ = 0;
    uint16_t real_period_ = 0;
};

} // namespace ear6::nes
