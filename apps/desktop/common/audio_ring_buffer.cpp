#include "audio_ring_buffer.h"

#include <algorithm>

namespace ear6::desktop {

AudioRingBuffer::AudioRingBuffer(size_t capacity_samples)
    : samples_(capacity_samples) {}

void AudioRingBuffer::clear() {
    std::lock_guard lock(mutex_);
    read_position_ = 0;
    size_ = 0;
}

void AudioRingBuffer::push(const int16_t* samples, size_t count) {
    if (!samples || count == 0 || samples_.empty()) return;

    std::lock_guard lock(mutex_);
    if (count >= samples_.size()) {
        samples += count - samples_.size();
        count = samples_.size();
        read_position_ = 0;
        size_ = 0;
    } else if (count > samples_.size() - size_) {
        const size_t discard = count - (samples_.size() - size_);
        read_position_ = (read_position_ + discard) % samples_.size();
        size_ -= discard;
    }

    size_t write_position = (read_position_ + size_) % samples_.size();
    const size_t first = std::min(count, samples_.size() - write_position);
    std::copy_n(samples, first, samples_.begin() + static_cast<std::ptrdiff_t>(write_position));
    std::copy_n(samples + first, count - first, samples_.begin());
    size_ += count;
}

size_t AudioRingBuffer::pop(int16_t* output, size_t count) {
    if (!output || count == 0 || samples_.empty()) return 0;

    std::lock_guard lock(mutex_);
    count = std::min(count, size_);
    const size_t first = std::min(count, samples_.size() - read_position_);
    std::copy_n(samples_.begin() + static_cast<std::ptrdiff_t>(read_position_), first, output);
    std::copy_n(samples_.begin(), count - first, output + first);
    read_position_ = (read_position_ + count) % samples_.size();
    size_ -= count;
    return count;
}

size_t AudioRingBuffer::get_size() const {
    std::lock_guard lock(mutex_);
    return size_;
}

} // namespace ear6::desktop
