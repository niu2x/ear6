#include "nes_sound_mixer.h"
#include "../blip_buf.h"
#include <algorithm>
#include <climits>

namespace ear6::nes {

NesSoundMixer::NesSoundMixer() {
    output_buffer_size_ = MAX_SAMPLES_PER_FRAME * 2;
    output_buffer_ = new int16_t[output_buffer_size_];
    blip_buf_left_ = blip_new(MAX_SAMPLES_PER_FRAME);
    reset();
}

NesSoundMixer::~NesSoundMixer() {
    delete[] output_buffer_;
    blip_delete(blip_buf_left_);
}

void NesSoundMixer::reset() {
    sample_count_ = 0;
    previous_output_left_ = 0;
    initialized_ = false;
    blip_clear(blip_buf_left_);
    timestamps_.clear();
    memset(channel_output_, 0, sizeof(channel_output_));
    memset(current_output_, 0, sizeof(current_output_));

    clock_rate_ = CPU_CLOCK_RATE_NTSC;
    blip_set_rates(blip_buf_left_, (double)clock_rate_, (double)APU_SAMPLE_RATE);
}

void NesSoundMixer::add_delta(AudioChannel channel, uint32_t time, int16_t delta) {
    if (delta != 0) {
        timestamps_.push_back(time);
        channel_output_[(int)channel][time] += delta;
    }
}

int16_t NesSoundMixer::get_output_volume() {
    double square_output = (double)current_output_[(int)AudioChannel::SQUARE1] +
                           (double)current_output_[(int)AudioChannel::SQUARE2];
    double tnd_output = (double)current_output_[(int)AudioChannel::DMC] +
                        2.7516713261 * (double)current_output_[(int)AudioChannel::TRIANGLE] +
                        1.8493587125 * (double)current_output_[(int)AudioChannel::NOISE];

    double square_volume = (95.88 * 5000.0) / (8128.0 / std::max(square_output, 0.0001) + 100.0);
    double tnd_volume = (159.79 * 5000.0) / (22638.0 / std::max(tnd_output, 0.0001) + 100.0);

    int result = (int)(square_volume + tnd_volume);
    if (result > SHRT_MAX) result = SHRT_MAX;
    if (result < SHRT_MIN) result = SHRT_MIN;
    return (int16_t)result;
}

void NesSoundMixer::end_frame(uint32_t time) {
    if (timestamps_.empty()) {
        blip_end_frame(blip_buf_left_, time);
        return;
    }

    std::sort(timestamps_.begin(), timestamps_.end());
    timestamps_.erase(
        std::unique(timestamps_.begin(), timestamps_.end()),
        timestamps_.end()
    );

    // On first frame with output, initialize previous_output_left_ to prevent pop
    if (!initialized_) {
        for (size_t i = 0; i < timestamps_.size(); i++) {
            uint32_t stamp = timestamps_[i];
            for (uint32_t j = 0; j < MAX_CHANNEL_COUNT; j++) {
                current_output_[j] += channel_output_[j][stamp];
            }
        }
        previous_output_left_ = get_output_volume() * 4;
        initialized_ = true;
        timestamps_.clear();
        memset(channel_output_, 0, sizeof(channel_output_));
        blip_end_frame(blip_buf_left_, time);
        return;
    }

    for (size_t i = 0; i < timestamps_.size(); i++) {
        uint32_t stamp = timestamps_[i];
        for (uint32_t j = 0; j < MAX_CHANNEL_COUNT; j++) {
            current_output_[j] += channel_output_[j][stamp];
        }

        int current_output = get_output_volume() * 4;
        int delta = current_output - previous_output_left_;
        if (delta != 0) {
            blip_add_delta(blip_buf_left_, stamp, delta);
        }
        previous_output_left_ = current_output;
    }

    blip_end_frame(blip_buf_left_, time);
    timestamps_.clear();
    memset(channel_output_, 0, sizeof(channel_output_));
}

void NesSoundMixer::play_audio_buffer(uint32_t time) {
    end_frame(time);

    size_t remaining = (output_buffer_size_ - sample_count_ * 2) / 2;
    if (remaining > MAX_SAMPLES_PER_FRAME) remaining = MAX_SAMPLES_PER_FRAME;

    int16_t* out = output_buffer_ + (sample_count_ * 2);
    size_t actual = blip_read_samples(blip_buf_left_, out, (int)remaining, 1);

    for (size_t i = 0; i < actual * 2; i += 2) {
        out[i + 1] = out[i];
    }

    sample_count_ += actual;
}

void NesSoundMixer::consume_samples(size_t count) {
    if (count >= sample_count_) {
        sample_count_ = 0;
        return;
    }
    size_t remaining = sample_count_ - count;
    memmove(output_buffer_, output_buffer_ + count * 2, remaining * 2 * sizeof(int16_t));
    sample_count_ = remaining;
}

} // namespace ear6::nes
