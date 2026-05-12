#pragma once

#include <cstdint>

namespace ear6 {

class System {
public:
    virtual ~System() = default;

    virtual int load(const void* data, int size) = 0;
    virtual int step() = 0;

    virtual const uint8_t* framebuffer() const = 0;
    virtual int frame_width() const = 0;
    virtual int frame_height() const = 0;

    virtual const int16_t* audiobuffer() const = 0;
    virtual int audio_num_samples() const = 0;
};

} // namespace ear6
