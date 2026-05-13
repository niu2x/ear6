#include "system_test.h"

#include <cmath>

namespace ear6 {

static constexpr double PI = 3.14159265358979323846;

TestSystem::TestSystem()
    : framebuffer_(WIDTH * HEIGHT * 4, 0) {
    generate_wave();
}

int TestSystem::load(const void* data, int size) {
    (void)data;
    (void)size;
    return 0;
}

int TestSystem::step() {
    ++tick_;
    generate_wave();
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

void TestSystem::generate_wave() {
    double t = tick_ * 0.05;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            double fx = x / static_cast<double>(WIDTH);
            double fy = y / static_cast<double>(HEIGHT);

            double wave1 = std::sin(fx * 6.0 * PI + t * 1.3);
            double wave2 = std::sin(fy * 4.0 * PI + t * 0.9);
            double wave3 = std::sin((fx + fy) * 5.0 * PI - t * 1.1);

            double r = 0.5 + 0.5 * wave1;
            double g = 0.5 + 0.5 * wave2;
            double b = 0.5 + 0.5 * wave3;

            int idx = (y * WIDTH + x) * 4;
            framebuffer_[idx + 0] = static_cast<uint8_t>(r * 255.0);
            framebuffer_[idx + 1] = static_cast<uint8_t>(g * 255.0);
            framebuffer_[idx + 2] = static_cast<uint8_t>(b * 255.0);
            framebuffer_[idx + 3] = 255;
        }
    }
}

} // namespace ear6
