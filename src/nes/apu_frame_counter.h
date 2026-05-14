#pragma once

#include "ines_memory_handler.h"
#include "nes_types.h"
#include <cstdint>
#include <cstring>

namespace ear6::nes {

class NesConsole;
class NesApu;

enum class FrameType {
    None = 0,
    QuarterFrame = 1,
    HalfFrame = 2,
};

class ApuFrameCounter : public INesMemoryHandler {
public:
    static constexpr int32_t STEP_CYCLES_NTSC[2][6] = {
        {7457, 14913, 22371, 29828, 29829, 29830},
        {7457, 14913, 22371, 29829, 37281, 37282}
    };
    static constexpr int32_t STEP_CYCLES_PAL[2][6] = {
        {8313, 16627, 24939, 33252, 33253, 33254},
        {8313, 16627, 24939, 33253, 41565, 41566}
    };
    static constexpr FrameType FRAME_TYPE[2][6] = {
        {FrameType::QuarterFrame, FrameType::HalfFrame, FrameType::QuarterFrame, FrameType::None, FrameType::HalfFrame, FrameType::None},
        {FrameType::QuarterFrame, FrameType::HalfFrame, FrameType::QuarterFrame, FrameType::None, FrameType::HalfFrame, FrameType::None}
    };

    ApuFrameCounter(NesConsole* console);
    void set_apu(NesApu* apu) { apu_ = apu; }

    void reset(bool soft_reset);
    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override { (void)addr; return 0; }
    void write_ram(uint16_t addr, uint8_t value) override;

    uint32_t run(int32_t& cycles_to_run);
    bool need_to_run(uint32_t cycles_to_run);
    bool get_irq_flag();
    void set_region(uint32_t clock_rate);

private:
    NesConsole* console_ = nullptr;
    NesApu* apu_ = nullptr;
    int32_t step_cycles_[2][6] = {};
    int32_t previous_cycle_ = 0;
    uint32_t current_step_ = 0;
    uint32_t step_mode_ = 0;
    bool inhibit_irq_ = false;
    uint8_t block_frame_counter_tick_ = 0;
    int16_t new_value_ = 0;
    int8_t write_delay_counter_ = 0;
    bool irq_flag_ = false;
    uint64_t irq_flag_clear_clock_ = 0;
};

} // namespace ear6::nes
