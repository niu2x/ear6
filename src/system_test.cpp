#include "system_test.h"

namespace ear6 {

TestSystem::TestSystem()
    : framebuffer_(WIDTH * HEIGHT * 4, 0) {
    generate_mosaic();
}

int TestSystem::load(const void* data, int size) {
    (void)data;
    (void)size;
    return 0;
}

int TestSystem::step() {
    generate_mosaic();
    return 0;
}

const uint8_t* TestSystem::get_framebuffer() const {
    return framebuffer_.data();
}

int TestSystem::get_frame_width() const {
    return WIDTH;
}

int TestSystem::get_frame_height() const {
    return HEIGHT;
}

const int16_t* TestSystem::get_audiobuffer() const {
    return nullptr;
}

int TestSystem::get_audio_num_samples() const {
    return 0;
}

void TestSystem::generate_mosaic() {
    static constexpr uint8_t colors[][4] = {
        {255,   0,   0, 255},
        {  0, 255,   0, 255},
        {  0,   0, 255, 255},
        {255, 255,   0, 255},
        {  0, 255, 255, 255},
        {255,   0, 255, 255},
        {255, 128,   0, 255},
        {128,   0, 255, 255},
    };
    static constexpr int BLOCK = 16;
    static constexpr int NUM_COLORS = 8;

    for (int by = 0; by < HEIGHT; by += BLOCK) {
        for (int bx = 0; bx < WIDTH; bx += BLOCK) {
            int ci = ((bx / BLOCK) + (by / BLOCK)) % NUM_COLORS;
            for (int y = by; y < by + BLOCK && y < HEIGHT; ++y) {
                for (int x = bx; x < bx + BLOCK && x < WIDTH; ++x) {
                    int idx = (y * WIDTH + x) * 4;
                    framebuffer_[idx + 0] = colors[ci][0];
                    framebuffer_[idx + 1] = colors[ci][1];
                    framebuffer_[idx + 2] = colors[ci][2];
                    framebuffer_[idx + 3] = colors[ci][3];
                }
            }
        }
    }
}

} // namespace ear6
