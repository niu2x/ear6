#include "delta_modulation_channel.h"
#include "nes_apu.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

DeltaModulationChannel::DeltaModulationChannel(NesConsole* console)
    : timer_() {
    console_ = console;
}

void DeltaModulationChannel::reset(bool soft_reset) {
    timer_.reset(soft_reset);

    if (!soft_reset) {
        sample_addr_ = 0xC000;
        sample_length_ = 1;
    }

    output_level_ = 0;
    irq_enabled_ = false;
    loop_flag_ = false;
    current_addr_ = 0;
    bytes_remaining_ = 0;
    read_buffer_ = 0;
    buffer_empty_ = true;
    shift_register_ = 0;
    bits_remaining_ = 8;
    silence_flag_ = true;
    need_to_run_flag_ = false;
    transfer_start_delay_ = 0;
    disable_delay_ = 0;

    timer_.set_period(DMC_PERIOD_TABLE_NTSC[0] - 1);
    timer_.set_timer(timer_.get_period());
}

void DeltaModulationChannel::init_sample() {
    current_addr_ = sample_addr_;
    bytes_remaining_ = sample_length_;
    need_to_run_flag_ |= bytes_remaining_ > 0;
}

void DeltaModulationChannel::start_dmc_transfer() {
    if (buffer_empty_ && bytes_remaining_ > 0) {
        console_->get_cpu()->start_dmc_transfer();
    }
}

uint16_t DeltaModulationChannel::get_dmc_read_address() {
    return current_addr_;
}

void DeltaModulationChannel::set_dmc_read_buffer(uint8_t value) {
    if (bytes_remaining_ > 0) {
        read_buffer_ = value;
        buffer_empty_ = false;
        current_addr_++;
        if (current_addr_ == 0) current_addr_ = 0x8000;
        bytes_remaining_--;

        if (bytes_remaining_ == 0) {
            if (loop_flag_) {
                init_sample();
            } else if (irq_enabled_) {
                console_->get_cpu()->set_irq_source(IRQSource::DMC);
            }
        }
    }
}

void DeltaModulationChannel::run(uint32_t target_cycle) {
    while (timer_.run(target_cycle)) {
        if (!silence_flag_) {
            uint8_t bit = shift_register_ & 0x01;
            shift_register_ >>= 1;
            if (bit) {
                if (output_level_ <= 125) output_level_ += 2;
            } else {
                if (output_level_ >= 2) output_level_ -= 2;
            }
        }

        bits_remaining_--;
        if (bits_remaining_ == 0) {
            bits_remaining_ = 8;
            if (buffer_empty_) {
                silence_flag_ = true;
            } else {
                silence_flag_ = false;
                shift_register_ = read_buffer_;
                buffer_empty_ = true;
                need_to_run_flag_ = true;
                if (transfer_start_delay_ == 0) {
                    start_dmc_transfer();
                }
            }
        }

        timer_.add_output(output_level_);
    }
}

bool DeltaModulationChannel::irq_pending(uint32_t cycles_to_run) {
    if (irq_enabled_ && bytes_remaining_ > 0) {
        uint32_t cycles_to_empty = (bits_remaining_ + (bytes_remaining_ - 1) * 8) * timer_.get_period();
        if (cycles_to_run >= cycles_to_empty) return true;
    }
    return false;
}

bool DeltaModulationChannel::is_active() {
    return bytes_remaining_ > 0;
}

void DeltaModulationChannel::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::WRITE, 0x4010, 0x4013);
}

void DeltaModulationChannel::write_ram(uint16_t addr, uint8_t value) {
    console_->get_apu()->run();

    switch (addr & 0x03) {
        case 0:
            irq_enabled_ = (value & 0x80) == 0x80;
            loop_flag_ = (value & 0x40) == 0x40;
            timer_.set_period(DMC_PERIOD_TABLE_NTSC[value & 0x0F] - 1);
            if (!irq_enabled_) {
                console_->get_cpu()->clear_irq_source(IRQSource::DMC);
            }
            break;

        case 1:
            output_level_ = value & 0x7F;
            timer_.add_output(output_level_);
            break;

        case 2:
            sample_addr_ = 0xC000 | ((uint32_t)value << 6);
            break;

        case 3:
            sample_length_ = (value << 4) | 0x0001;
            break;
    }
}

void DeltaModulationChannel::set_enabled(bool enabled) {
    if (!enabled) {
        if (disable_delay_ == 0) {
            if ((console_->get_cpu()->get_cycle_count() & 0x01) == 0)
                disable_delay_ = 2;
            else
                disable_delay_ = 3;
        }
        need_to_run_flag_ = true;
    } else if (bytes_remaining_ == 0) {
        init_sample();
        if ((console_->get_cpu()->get_cycle_count() & 0x01) == 0)
            transfer_start_delay_ = 2;
        else
            transfer_start_delay_ = 3;
        need_to_run_flag_ = true;
    }
}

void DeltaModulationChannel::process_clock() {
    if (disable_delay_ && --disable_delay_ == 0) {
        disable_delay_ = 0;
        bytes_remaining_ = 0;
        console_->get_cpu()->stop_dmc_transfer();
    }

    if (transfer_start_delay_ && --transfer_start_delay_ == 0) {
        start_dmc_transfer();
    }

    need_to_run_flag_ = disable_delay_ || transfer_start_delay_ || bytes_remaining_;
}

bool DeltaModulationChannel::need_to_run() {
    if (need_to_run_flag_) process_clock();
    return need_to_run_flag_;
}

} // namespace ear6::nes
