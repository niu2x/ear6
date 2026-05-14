#pragma once

#include "apu_envelope.h"
#include "apu_timer.h"
#include "ines_memory_handler.h"

namespace ear6::nes {

class NesConsole;
class NesSoundMixer;

class NoiseChannel : public INesMemoryHandler {
public:
    static constexpr uint16_t NOISE_PERIOD_TABLE_NTSC[16] = {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
    };
    static constexpr uint16_t NOISE_PERIOD_TABLE_PAL[16] = {
        4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
    };

    NoiseChannel(NesConsole* console)
        : envelope_(AudioChannel::NOISE, console), timer_() {
        console_ = console;
    }

    void set_mixer(NesSoundMixer* mixer) { timer_.set_mixer(AudioChannel::NOISE, mixer); }
    void set_apu(NesApu* apu) { envelope_.set_apu(apu); }

    void run(uint32_t target_cycle);
    void reset(bool soft_reset);
    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override { (void)addr; return 0; }
    void write_ram(uint16_t addr, uint8_t value) override;

    void tick_envelope() { envelope_.tick_envelope(); }
    void tick_length_counter() { envelope_.length_counter.tick_length_counter(); }
    void reload_length_counter() { envelope_.length_counter.reload_counter(); }
    void end_frame() { timer_.end_frame(); }
    void set_enabled(bool enabled) { envelope_.length_counter.set_enabled(enabled); }
    bool is_active() { return envelope_.length_counter.is_active(); }
    uint8_t get_output() { return (uint8_t)timer_.get_last_output(); }

private:
    bool is_muted() { return (shift_register_ & 0x01) == 0x01; }

    NesConsole* console_ = nullptr;
    ApuEnvelope envelope_;
    ApuTimer timer_;
    uint16_t shift_register_ = 1;
    bool mode_flag_ = false;
};

} // namespace ear6::nes
