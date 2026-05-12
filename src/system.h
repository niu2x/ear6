#pragma once

#include <cstdint>

namespace ear6 {

class System {
public:
    virtual ~System() = default;

    virtual int load(const void* data, int size) = 0;
    virtual int step() = 0;

    virtual const uint8_t* get_framebuffer() const = 0;
    virtual int get_frame_width() const = 0;
    virtual int get_frame_height() const = 0;

    virtual const int16_t* get_audiobuffer() const = 0;
    virtual int get_audio_num_samples() const = 0;
};

} // namespace ear6
