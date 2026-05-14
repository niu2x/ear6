#pragma once

#include "ines_memory_handler.h"
#include "apu_frame_counter.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace ear6::nes {

class NesConsole;
class NesSoundMixer;
class SquareChannel;
class TriangleChannel;
class NoiseChannel;
class DeltaModulationChannel;

class NesApu : public INesMemoryHandler {
public:
    NesApu(NesConsole* console, NesSoundMixer* mixer);
    ~NesApu() override;

    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;

    void reset(bool soft_reset);
    void process_cpu_clock();
    void run();
    void end_frame();

    void frame_counter_tick(FrameType type);
    void set_need_to_run() { need_to_run_ = true; }

    void set_apu_status(bool active) { apu_enabled_ = active; }
    bool is_apu_enabled() { return apu_enabled_; }

    // Single-frame audio buffer (overwritten each frame)
    const int16_t* get_buffer() const;
    int get_samples() const;
    void consume_audio(size_t stereo_samples);

    SquareChannel* get_square1() { return square1_.get(); }
    SquareChannel* get_square2() { return square2_.get(); }
    TriangleChannel* get_triangle() { return triangle_.get(); }
    NoiseChannel* get_noise() { return noise_.get(); }
    DeltaModulationChannel* get_dmc() { return dmc_.get(); }
    ApuFrameCounter* get_frame_counter() { return frame_counter_.get(); }

    uint16_t get_dmc_read_address();
    void set_dmc_read_buffer(uint8_t value);

private:
    bool need_to_run(uint32_t current_cycle);

    bool apu_enabled_ = true;
    bool need_to_run_ = false;
    uint32_t previous_cycle_ = 0;
    uint32_t current_cycle_ = 0;

    std::unique_ptr<SquareChannel> square1_;
    std::unique_ptr<SquareChannel> square2_;
    std::unique_ptr<TriangleChannel> triangle_;
    std::unique_ptr<NoiseChannel> noise_;
    std::unique_ptr<DeltaModulationChannel> dmc_;
    std::unique_ptr<ApuFrameCounter> frame_counter_;

    NesConsole* console_ = nullptr;
    NesSoundMixer* mixer_ = nullptr;

    // Single frame's audio: stereo interleaved int16
    std::vector<int16_t> audio_buffer_;
};

} // namespace ear6::nes
