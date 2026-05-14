#pragma once

#include "apu_timer.h"
#include "apu_length_counter.h"
#include "ines_memory_handler.h"

namespace ear6::nes {

class NesConsole;
class NesSoundMixer;

class TriangleChannel : public INesMemoryHandler {
public:
    static constexpr uint8_t SEQUENCE[32] = {
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    TriangleChannel(NesConsole* console)
        : length_counter_(AudioChannel::Triangle, console), timer_() {
        console_ = console;
    }

    void set_mixer(NesSoundMixer* mixer) { timer_.set_mixer(AudioChannel::Triangle, mixer); }
    void set_apu(NesApu* apu) { length_counter_.set_apu(apu); }

    void run(uint32_t target_cycle);
    void reset(bool soft_reset);
    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override { (void)addr; return 0; }
    void write_ram(uint16_t addr, uint8_t value) override;

    void tick_linear_counter();
    void tick_length_counter() { length_counter_.tick_length_counter(); }
    void reload_length_counter() { length_counter_.reload_counter(); }
    void end_frame() { timer_.end_frame(); }
    void set_enabled(bool enabled) { length_counter_.set_enabled(enabled); }
    bool get_status() { return length_counter_.get_status(); }
    uint8_t get_output() { return (uint8_t)timer_.get_last_output(); }

private:
    NesConsole* console_ = nullptr;
    ApuLengthCounter length_counter_;
    ApuTimer timer_;
    uint8_t linear_counter_ = 0;
    uint8_t linear_counter_reload_ = 0;
    bool linear_reload_flag_ = false;
    bool linear_control_flag_ = false;
    uint8_t sequence_position_ = 0;
};

} // namespace ear6::nes
