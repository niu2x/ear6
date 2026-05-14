#pragma once

#include "../system.h"
#include "nes_console.h"
#include <cstdint>
#include <vector>

namespace ear6 {

class NesSystem : public System {
public:
    NesSystem();

    int load(const void* data, int size) override {
        return console_->load_rom(data, size);
    }

    int step() override;

    const uint8_t* get_framebuffer() const override;
    int get_frame_width() const override { return 256; }
    int get_frame_height() const override { return 240; }

    const int16_t* get_audiobuffer() const override {
        return console_->get_audiobuffer();
    }

    int get_audio_num_samples() const override {
        return console_->get_audio_num_samples();
    }

    void consume_audio() override {
        console_->consume_audio();
    }

    void set_palette(const uint32_t* palette);

    nes::NesConsole* get_console() { return console_.get(); }

private:
    void convert_frame();

    std::unique_ptr<nes::NesConsole> console_;
    uint32_t palette_[64] = {};
    std::vector<uint8_t> rgba_framebuffer_;
};

} // namespace ear6
