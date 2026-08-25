#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace ear6::desktop {

class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacity_samples);

    void clear();
    void push(const int16_t* samples, size_t count);
    size_t pop(int16_t* output, size_t count);
    size_t get_size() const;

private:
    mutable std::mutex mutex_;
    std::vector<int16_t> samples_;
    size_t read_position_ = 0;
    size_t size_ = 0;
};

} // namespace ear6::desktop
