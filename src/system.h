#pragma once

#include <cstdint>
#include <cstddef>

namespace ear6 {

class System {
public:
    virtual ~System() = default;

    virtual int load(const void* data, int size) = 0;

    virtual int load_data(const void* data, int size, const char* name_hint) {
        (void)name_hint;
        return load(data, size);
    }

    virtual int step() = 0;

    virtual const uint8_t* get_framebuffer() const = 0;
    virtual int get_frame_width() const = 0;
    virtual int get_frame_height() const = 0;

    virtual const int16_t* get_audiobuffer() const = 0;
    virtual int get_audio_num_samples() const = 0;
    virtual void consume_audio() = 0;
};

} // namespace ear6
