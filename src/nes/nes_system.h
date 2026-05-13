#pragma once

#include "../system.h"
#include "nes_console.h"
#include <cstdint>
#include <vector>

namespace ear6 {

class NesSystem : public System {
public:
    NesSystem() : console_(std::make_unique<nes::NesConsole>()) {}

    int load(const void* data, int size) override {
        return console_->load_rom(data, size);
    }

    int step() override {
        console_->run_frame();
        return 0; // Frame completed
    }

    const uint8_t* get_framebuffer() const override {
        return reinterpret_cast<const uint8_t*>(console_->get_framebuffer());
    }

    int get_frame_width() const override { return 256; }
    int get_frame_height() const override { return 240; }

    const int16_t* get_audiobuffer() const override {
        return console_->get_audiobuffer();
    }

    int get_audio_num_samples() const override {
        return console_->get_audio_num_samples();
    }

    nes::NesConsole* get_console() { return console_.get(); }

private:
    std::unique_ptr<nes::NesConsole> console_;
};

} // namespace ear6
