#pragma once

#include "nes_types.h"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>

struct blip_t;

namespace ear6::nes {

class NesSoundMixer {
public:
    static constexpr uint32_t CYCLE_LENGTH = 10000;
    static constexpr uint32_t MAX_SAMPLE_RATE = 96000;
    static constexpr uint32_t MAX_SAMPLES_PER_FRAME = MAX_SAMPLE_RATE / 60 * 4;
    static constexpr uint32_t MAX_CHANNEL_COUNT = 11;

    NesSoundMixer();
    ~NesSoundMixer();

    void reset();
    void play_audio_buffer(uint32_t time);
    void add_delta(AudioChannel channel, uint32_t time, int16_t delta);

    const int16_t* get_output_buffer() const { return output_buffer_; }
    size_t get_sample_count() const { return sample_count_; }
    void consume_samples(size_t count);

private:
    void end_frame(uint32_t time);
    int16_t get_output_volume();

    int previous_output_left_ = 0;
    std::vector<uint32_t> timestamps_;
    int16_t channel_output_[MAX_CHANNEL_COUNT][CYCLE_LENGTH] = {};
    int16_t current_output_[MAX_CHANNEL_COUNT] = {};
    bool initialized_ = false;

    blip_t* blip_buf_left_ = nullptr;
    int16_t* output_buffer_ = nullptr;
    size_t output_buffer_size_ = 0;
    size_t sample_count_ = 0;
    uint32_t clock_rate_ = 0;
};

} // namespace ear6::nes
