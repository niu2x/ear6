#include "apu_frame_counter.h"
#include "nes_apu.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

ApuFrameCounter::ApuFrameCounter(NesConsole* console) {
    console_ = console;
    reset(false);
}

void ApuFrameCounter::reset(bool soft_reset) {
    previous_cycle_ = 0;
    irq_flag_ = false;
    irq_flag_clear_clock_ = 0;

    if (!soft_reset) step_mode_ = 0;

    current_step_ = 0;
    new_value_ = step_mode_ ? 0x80 : 0x00;
    write_delay_counter_ = 3;
    inhibit_irq_ = false;
    block_frame_counter_tick_ = 0;

    set_region(CPU_CLOCK_RATE_NTSC);
}

void ApuFrameCounter::set_region(uint32_t) {
    memcpy(step_cycles_, STEP_CYCLES_NTSC, sizeof(step_cycles_));
}

void ApuFrameCounter::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::WRITE, 0x4017);
}

void ApuFrameCounter::write_ram(uint16_t addr, uint8_t value) {
    (void)addr;
    if (apu_) apu_->run();
    new_value_ = value;

    if (console_->get_cpu()->get_cycle_count() & 0x01)
        write_delay_counter_ = 4;
    else
        write_delay_counter_ = 3;

    inhibit_irq_ = (value & 0x40) == 0x40;
    if (inhibit_irq_) {
        console_->get_cpu()->clear_irq_source(IRQSource::FRAME_COUNTER);
        irq_flag_ = false;
        irq_flag_clear_clock_ = 0;
    }
}

uint32_t ApuFrameCounter::run(int32_t& cycles_to_run) {
    uint32_t cycles_ran;

    if (previous_cycle_ + cycles_to_run >= step_cycles_[step_mode_][current_step_]) {
        if (step_mode_ == 0 && current_step_ >= 3) {
            irq_flag_ = true;
            irq_flag_clear_clock_ = 0;
            if (!inhibit_irq_) {
                console_->get_cpu()->set_irq_source(IRQSource::FRAME_COUNTER);
            } else if (current_step_ == 5) {
                irq_flag_ = false;
                irq_flag_clear_clock_ = 0;
            }
        }

        FrameType type = FRAME_TYPE[step_mode_][current_step_];
        if (type != FrameType::NONE && !block_frame_counter_tick_) {
            if (apu_) apu_->frame_counter_tick(type);
            block_frame_counter_tick_ = 2;
        }

        if (step_cycles_[step_mode_][current_step_] < previous_cycle_)
            cycles_ran = 0;
        else
            cycles_ran = step_cycles_[step_mode_][current_step_] - previous_cycle_;

        cycles_to_run -= cycles_ran;
        current_step_++;
        if (current_step_ == 6) {
            current_step_ = 0;
            previous_cycle_ = 0;
        } else {
            previous_cycle_ += cycles_ran;
        }
    } else {
        cycles_ran = cycles_to_run;
        cycles_to_run = 0;
        previous_cycle_ += cycles_ran;
    }

    if (new_value_ >= 0) {
        write_delay_counter_--;
        if (write_delay_counter_ == 0) {
            step_mode_ = ((new_value_ & 0x80) == 0x80) ? 1 : 0;
            write_delay_counter_ = -1;
            current_step_ = 0;
            previous_cycle_ = 0;
            new_value_ = -1;

            if (step_mode_ && !block_frame_counter_tick_) {
                if (apu_) apu_->frame_counter_tick(FrameType::HALF_FRAME);
                block_frame_counter_tick_ = 2;
            }
        }
    }

    if (block_frame_counter_tick_ > 0) block_frame_counter_tick_--;

    return cycles_ran;
}

bool ApuFrameCounter::need_to_run(uint32_t cycles_to_run) {
    return new_value_ >= 0 || block_frame_counter_tick_ > 0 ||
        (previous_cycle_ + (int32_t)cycles_to_run >= step_cycles_[step_mode_][current_step_] - 1);
}

bool ApuFrameCounter::get_irq_flag() {
    if (irq_flag_) {
        return true;
    }
    return false;
}

} // namespace ear6::nes
