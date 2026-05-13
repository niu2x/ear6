#pragma once

#include "system.h"

#include <vector>

namespace ear6 {

class TestSystem : public System {
public:
    TestSystem();

    int load(const void* data, int size) override;
    int step() override;

    const uint8_t* get_framebuffer() const override;
    int get_frame_width() const override;
    int get_frame_height() const override;

    const int16_t* get_audiobuffer() const override;
    int get_audio_num_samples() const override;

private:
    void generate_wave();

    static constexpr int WIDTH = 256;
    static constexpr int HEIGHT = 240;

    int tick_ = 0;
    std::vector<uint8_t> framebuffer_;
};

} // namespace ear6
