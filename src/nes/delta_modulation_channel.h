#pragma once

#include "apu_timer.h"
#include "ines_memory_handler.h"

namespace ear6::nes {

class NesConsole;
class NesSoundMixer;

class DeltaModulationChannel : public INesMemoryHandler {
public:
    static constexpr uint16_t DMC_PERIOD_TABLE_NTSC[16] = {
        428, 380, 340, 320, 286, 254, 226, 214,
        190, 160, 142, 128, 106, 84, 72, 54
    };
    static constexpr uint16_t DMC_PERIOD_TABLE_PAL[16] = {
        398, 354, 316, 298, 276, 236, 210, 198,
        176, 148, 132, 118, 98, 78, 66, 50
    };

    DeltaModulationChannel(NesConsole* console);

    void set_mixer(NesSoundMixer* mixer) { timer_.set_mixer(AudioChannel::DMC, mixer); }

    void run(uint32_t target_cycle);
    void reset(bool soft_reset);
    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override { (void)addr; return 0; }
    void write_ram(uint16_t addr, uint8_t value) override;
    void end_frame() { timer_.end_frame(); }

    bool irq_pending(uint32_t cycles_to_run);
    bool need_to_run();
    bool is_active();
    void set_enabled(bool enabled);
    void process_clock();
    void start_dmc_transfer();
    uint16_t get_dmc_read_address();
    void set_dmc_read_buffer(uint8_t value);
    uint8_t get_output() { return (uint8_t)timer_.get_last_output(); }

private:
    void init_sample();

    NesConsole* console_ = nullptr;
    ApuTimer timer_;
    uint16_t sample_addr_ = 0;
    uint16_t sample_length_ = 0;
    uint8_t output_level_ = 0;
    bool irq_enabled_ = false;
    bool loop_flag_ = false;
    uint16_t current_addr_ = 0;
    uint16_t bytes_remaining_ = 0;
    uint8_t read_buffer_ = 0;
    bool buffer_empty_ = true;
    uint8_t shift_register_ = 0;
    uint8_t bits_remaining_ = 8;
    bool silence_flag_ = true;
    bool need_to_run_flag_ = false;
    uint8_t disable_delay_ = 0;
    uint8_t transfer_start_delay_ = 0;
};

} // namespace ear6::nes
