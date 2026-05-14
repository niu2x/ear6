#include "nes_apu.h"
#include <cstring>
#include "square_channel.h"
#include "triangle_channel.h"
#include "noise_channel.h"
#include "delta_modulation_channel.h"
#include "nes_sound_mixer.h"
#include "nes_console.h"
#include "nes_cpu.h"

namespace ear6::nes {

NesApu::NesApu(NesConsole* console, NesSoundMixer* mixer) {
    console_ = console;
    mixer_ = mixer;
    apu_enabled_ = true;
    need_to_run_ = false;

    square1_.reset(new SquareChannel(AudioChannel::SQUARE1, console, true));
    square2_.reset(new SquareChannel(AudioChannel::SQUARE2, console, false));
    triangle_.reset(new TriangleChannel(console));
    noise_.reset(new NoiseChannel(console));
    dmc_.reset(new DeltaModulationChannel(console));
    frame_counter_.reset(new ApuFrameCounter(console));

    square1_->set_mixer(mixer_);
    square2_->set_mixer(mixer_);
    triangle_->set_mixer(mixer_);
    noise_->set_mixer(mixer_);
    dmc_->set_mixer(mixer_);

    square1_->set_apu(this);
    square2_->set_apu(this);
    triangle_->set_apu(this);
    noise_->set_apu(this);
    frame_counter_->set_apu(this);

    reset(false);
}

NesApu::~NesApu() = default;

void NesApu::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::WRITE, 0x4000, 0x4013);
    ranges.add_handler(MemoryOperation::READ, 0x4015);
    ranges.add_handler(MemoryOperation::WRITE, 0x4015);
    ranges.add_handler(MemoryOperation::WRITE, 0x4017);
    ranges.add_handler(MemoryOperation::READ, 0x4018, 0x401A);
}

void NesApu::frame_counter_tick(FrameType type) {
    square1_->tick_envelope();
    square2_->tick_envelope();
    triangle_->tick_linear_counter();
    noise_->tick_envelope();

    if (type == FrameType::HALF_FRAME) {
        square1_->tick_length_counter();
        square2_->tick_length_counter();
        triangle_->tick_length_counter();
        noise_->tick_length_counter();

        square1_->tick_sweep();
        square2_->tick_sweep();
    }
}

uint8_t NesApu::read_ram(uint16_t addr) {
    run();

    switch (addr) {
        case 0x4015: {
            uint8_t status = 0;
            status |= square1_->is_active() ? 0x01 : 0x00;
            status |= square2_->is_active() ? 0x02 : 0x00;
            status |= triangle_->is_active() ? 0x04 : 0x00;
            status |= noise_->is_active() ? 0x08 : 0x00;
            status |= dmc_->is_active() ? 0x10 : 0x00;
            status |= frame_counter_->get_irq_flag() ? 0x40 : 0x00;
            status |= console_->get_cpu()->has_irq_source(IRQSource::DMC) ? 0x80 : 0x00;
            status |= 0x20;

            console_->get_cpu()->clear_irq_source(IRQSource::FRAME_COUNTER);

            return status;
        }

        case 0x4018: return square1_->get_output() | (square2_->get_output() << 4);
        case 0x4019: return triangle_->get_output() | (noise_->get_output() << 4);
        case 0x401A: return dmc_->get_output();

        default:
            return 0;
    }
}

void NesApu::write_ram(uint16_t addr, uint8_t value) {
    if (addr == 0x4015) {
        run();

        console_->get_cpu()->clear_irq_source(IRQSource::DMC);

        square1_->set_enabled((value & 0x01) == 0x01);
        square2_->set_enabled((value & 0x02) == 0x02);
        triangle_->set_enabled((value & 0x04) == 0x04);
        noise_->set_enabled((value & 0x08) == 0x08);
        dmc_->set_enabled((value & 0x10) == 0x10);
    } else if (addr == 0x4017) {
        frame_counter_->write_ram(addr, value);
    } else if (addr >= 0x4000 && addr <= 0x4003) {
        square1_->write_ram(addr, value);
    } else if (addr >= 0x4004 && addr <= 0x4007) {
        square2_->write_ram(addr, value);
    } else if (addr >= 0x4008 && addr <= 0x400B) {
        triangle_->write_ram(addr, value);
    } else if (addr >= 0x400C && addr <= 0x400F) {
        noise_->write_ram(addr, value);
    } else if (addr >= 0x4010 && addr <= 0x4013) {
        dmc_->write_ram(addr, value);
    }
}

void NesApu::run() {
    int32_t cycles_to_run = current_cycle_ - previous_cycle_;

    while (cycles_to_run > 0) {
        previous_cycle_ += frame_counter_->run(cycles_to_run);

        square1_->reload_length_counter();
        square2_->reload_length_counter();
        noise_->reload_length_counter();
        triangle_->reload_length_counter();

        square1_->run(previous_cycle_);
        square2_->run(previous_cycle_);
        noise_->run(previous_cycle_);
        triangle_->run(previous_cycle_);
        dmc_->run(previous_cycle_);
    }
}

bool NesApu::need_to_run(uint32_t current_cycle) {
    if (dmc_->need_to_run() || need_to_run_) {
        need_to_run_ = false;
        return true;
    }

    uint32_t cycles_to_run = current_cycle - previous_cycle_;
    return frame_counter_->need_to_run(cycles_to_run) || dmc_->irq_pending(cycles_to_run);
}

void NesApu::process_cpu_clock() {
    if (apu_enabled_) {
        current_cycle_++;
        if (current_cycle_ == NesSoundMixer::CYCLE_LENGTH - 1) {
            end_frame();
        } else if (need_to_run(current_cycle_)) {
            run();
        }
    }
}

void NesApu::end_frame() {
    dmc_->process_clock();
    run();
    square1_->end_frame();
    square2_->end_frame();
    triangle_->end_frame();
    noise_->end_frame();
    dmc_->end_frame();

    mixer_->play_audio_buffer(current_cycle_);

    current_cycle_ = 0;
    previous_cycle_ = 0;

    // Accumulate samples (pushed as one frame by push_frame)
    size_t count = mixer_->get_sample_count();
    if (count > 0) {
        size_t old_size = frame_accumulator_.size();
        frame_accumulator_.resize(old_size + count * 2);
        memcpy(frame_accumulator_.data() + old_size, mixer_->get_output_buffer(), count * 2 * sizeof(int16_t));
        mixer_->consume_samples(count);
    }
}

void NesApu::push_frame() {
    if (frame_accumulator_.empty()) {
        return;
    }

    AudioFrame& frame = audio_ring_[write_index_];
    frame.data.swap(frame_accumulator_);
    frame_accumulator_.clear();
    frame.samples = frame.data.size() / 2;

    write_index_ = (write_index_ + 1) % MAX_AUDIO_FRAMES;
    if (available_frames_ < MAX_AUDIO_FRAMES) {
        available_frames_++;
    } else {
        read_index_ = (read_index_ + 1) % MAX_AUDIO_FRAMES;
    }
}

const int16_t* NesApu::get_buffer() const {
    if (available_frames_ > 0) {
        return audio_ring_[read_index_].data.data();
    }
    return nullptr;
}

int NesApu::get_samples() const {
    if (available_frames_ > 0) {
        return (int)audio_ring_[read_index_].samples;
    }
    return 0;
}

void NesApu::consume_audio() {
    if (available_frames_ > 0) {
        read_index_ = (read_index_ + 1) % MAX_AUDIO_FRAMES;
        available_frames_--;
    }
}

void NesApu::reset(bool soft_reset) {
    apu_enabled_ = true;
    current_cycle_ = 0;
    previous_cycle_ = 0;
    square1_->reset(soft_reset);
    square2_->reset(soft_reset);
    triangle_->reset(soft_reset);
    noise_->reset(soft_reset);
    dmc_->reset(soft_reset);
    frame_counter_->reset(soft_reset);

    for (auto& frame : audio_ring_) {
        frame.data.clear();
        frame.samples = 0;
    }
    frame_accumulator_.clear();
    write_index_ = 0;
    read_index_ = 0;
    available_frames_ = 0;
}

uint16_t NesApu::get_dmc_read_address() {
    return dmc_->get_dmc_read_address();
}

void NesApu::set_dmc_read_buffer(uint8_t value) {
    dmc_->set_dmc_read_buffer(value);
}

} // namespace ear6::nes
