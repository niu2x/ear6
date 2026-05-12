#pragma once

#include "system.h"

#include <vector>

namespace ear6 {

class TestSystem : public System {
public:
    TestSystem();

    int load(const void* data, int size) override;
    int step() override;

    const uint8_t* framebuffer() const override;
    int frame_width() const override;
    int frame_height() const override;

    const int16_t* audiobuffer() const override;
    int audio_num_samples() const override;

private:
    void generate_mosaic();

    static constexpr int WIDTH = 256;
    static constexpr int HEIGHT = 240;

    std::vector<uint8_t> framebuffer_;
};

} // namespace ear6
