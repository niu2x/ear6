#include "nes_system.h"
#include "nes_ppu.h"

namespace ear6 {

static const uint32_t DEFAULT_NES_PALETTE[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

// PPU 2C03 palette (VS UniSystem) — RGB PPU, different color mapping from 2C02
static const uint32_t VS_PALETTE_2C03[64] = {
    0x6D6D6D, 0x002491, 0x0000DA, 0x6D48DA, 0x91006D, 0xB6006D, 0xB62400, 0x914800,
    0x6D4800, 0x244800, 0x006D24, 0x009100, 0x004848, 0x000000, 0x000000, 0x000000,
    0xB6B6B6, 0x006DDA, 0x0048FF, 0x9100FF, 0xB600FF, 0xFF0091, 0xFF0000, 0xDA6D00,
    0x916D00, 0x249100, 0x009100, 0x00B66D, 0x009191, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0x6DB6FF, 0x9191FF, 0xDA6DFF, 0xFF00FF, 0xFF6DFF, 0xFF9100, 0xFFB600,
    0xDADA00, 0x6DDA00, 0x00FF00, 0x48FFDA, 0x00FFFF, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0xB6DAFF, 0xDAB6FF, 0xFFB6FF, 0xFF91FF, 0xFFB6B6, 0xFFDA91, 0xFFFF48,
    0xFFFF6D, 0xB6FF48, 0x91FF6D, 0x48FFDA, 0x91DAFF, 0x000000, 0x000000, 0x000000
};

NesSystem::NesSystem()
    : console_(std::make_unique<nes::NesConsole>())
    , rgba_framebuffer_(256 * 240 * 4, 0) {
    set_palette(DEFAULT_NES_PALETTE);
}

int NesSystem::load(const void* data, int size) {
    int ret = console_->load_rom(data, size);
    if (ret == 0 && console_->get_rom_info().use_vs_palette) {
        set_palette(VS_PALETTE_2C03);
    }
    return ret;
}

void NesSystem::set_palette(const uint32_t* palette) {
    for (int i = 0; i < 64; ++i) {
        palette_[i] = palette[i];
    }
    convert_frame();
}

const uint8_t* NesSystem::get_framebuffer() const {
    return rgba_framebuffer_.data();
}

void NesSystem::convert_frame() {
    const uint16_t* src = console_->get_framebuffer();
    if (!src) return;
    int num_pixels = 256 * 240;
    for (int i = 0; i < num_pixels; ++i) {
        uint8_t pal_idx = src[i] & 0x3F;
        uint32_t rgb = palette_[pal_idx];
        rgba_framebuffer_[i * 4 + 0] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
        rgba_framebuffer_[i * 4 + 1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
        rgba_framebuffer_[i * 4 + 2] = static_cast<uint8_t>(rgb & 0xFF);
        rgba_framebuffer_[i * 4 + 3] = 0xFF;
    }
}

int NesSystem::step() {
    console_->run_frame();
    {
        auto* ppu = console_->get_ppu();
        if (ppu && ppu->get_frame_count() <= 8) {
            const uint16_t* fb = console_->get_framebuffer();
            uint16_t fb0 = fb ? fb[0] : 0;
            fprintf(stderr, "[EAR6_STEP] f=%u pal0=%02X fb0=%02X\n",
                ppu->get_frame_count(), ppu->get_palette_ram0(), fb0 & 0x3F);
        }
    }
    convert_frame();
    return 0;
}

} // namespace ear6
