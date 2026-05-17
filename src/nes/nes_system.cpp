#include "nes_system.h"
#include "nes_ppu.h"

#include <cstdlib>

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

static const uint8_t PALETTE_LUT_2C02[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
   16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
   32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
   48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
};

static const uint8_t PALETTE_LUT_2C03[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
   16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
   32,33,34,35,36,37,38,39,40,41,42,43,44,15,46,47,
   48,49,50,51,52,53,54,55,56,57,58,59,60,15,62,63
};

static const uint8_t PALETTE_LUT_2C04C[64] = {
    20,37,58,16,11,32,49,9,1,46,54,8,21,61,62,60,
    34,28,5,18,25,24,23,27,0,3,46,2,22,6,52,53,
    35,15,14,55,13,39,38,32,41,4,33,36,17,45,46,31,
    44,30,57,51,7,42,40,29,10,46,50,56,19,43,63,12
};

NesSystem::NesSystem()
    : console_(std::make_unique<nes::NesConsole>())
    , rgba_framebuffer_(256 * 240 * 4, 0) {
    set_palette(DEFAULT_NES_PALETTE);
}

int NesSystem::load(const void* data, int size) {
    int ret = console_->load_rom(data, size);
    if (ret == 0) {
        set_palette(DEFAULT_NES_PALETTE);
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
    const auto& rom_info = console_->get_rom_info();
    const uint8_t* lut = PALETTE_LUT_2C02;
    if (rom_info.use_vs_palette) {
        if (rom_info.vs_ppu_model == nes::RomInfo::VsPpuModel::PPU_2C04C) {
            lut = PALETTE_LUT_2C04C;
        } else {
            lut = PALETTE_LUT_2C03;
        }
    }

    for (int i = 0; i < num_pixels; ++i) {
        uint8_t raw_idx = src[i] & 0x3F;
        uint8_t pal_idx = lut[raw_idx];
        uint32_t rgb = palette_[pal_idx];
        rgba_framebuffer_[i * 4 + 0] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
        rgba_framebuffer_[i * 4 + 1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
        rgba_framebuffer_[i * 4 + 2] = static_cast<uint8_t>(rgb & 0xFF);
        rgba_framebuffer_[i * 4 + 3] = 0xFF;
    }
}

int NesSystem::step() {
    console_->run_frame();
    convert_frame();

    if (std::getenv("EAR6_PALETTE_LOG") != nullptr) {
        auto* ppu = console_->get_ppu();
        if (ppu && ppu->get_frame_count() <= 2) {
            const uint16_t* idx_fb = console_->get_framebuffer();
            const auto& rom_info = console_->get_rom_info();
            const uint8_t* lut = PALETTE_LUT_2C02;
            if (rom_info.use_vs_palette) {
                if (rom_info.vs_ppu_model == nes::RomInfo::VsPpuModel::PPU_2C04C) {
                    lut = PALETTE_LUT_2C04C;
                } else {
                    lut = PALETTE_LUT_2C03;
                }
            }
            if (idx_fb) {
                fprintf(stderr,
                    "[EAR6_IDX16_F1] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    idx_fb[0] & 0x3F, idx_fb[1] & 0x3F, idx_fb[2] & 0x3F, idx_fb[3] & 0x3F,
                    idx_fb[4] & 0x3F, idx_fb[5] & 0x3F, idx_fb[6] & 0x3F, idx_fb[7] & 0x3F,
                    idx_fb[8] & 0x3F, idx_fb[9] & 0x3F, idx_fb[10] & 0x3F, idx_fb[11] & 0x3F,
                    idx_fb[12] & 0x3F, idx_fb[13] & 0x3F, idx_fb[14] & 0x3F, idx_fb[15] & 0x3F);

                fprintf(stderr,
                    "[EAR6_IDX2RGB16_F1]"
                    " %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X"
                    " %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X"
                    " %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X"
                    " %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X %02X>%02X>%06X\n",
                    idx_fb[0] & 0x3F, lut[idx_fb[0] & 0x3F], palette_[lut[idx_fb[0] & 0x3F]],
                    idx_fb[1] & 0x3F, lut[idx_fb[1] & 0x3F], palette_[lut[idx_fb[1] & 0x3F]],
                    idx_fb[2] & 0x3F, lut[idx_fb[2] & 0x3F], palette_[lut[idx_fb[2] & 0x3F]],
                    idx_fb[3] & 0x3F, lut[idx_fb[3] & 0x3F], palette_[lut[idx_fb[3] & 0x3F]],
                    idx_fb[4] & 0x3F, lut[idx_fb[4] & 0x3F], palette_[lut[idx_fb[4] & 0x3F]],
                    idx_fb[5] & 0x3F, lut[idx_fb[5] & 0x3F], palette_[lut[idx_fb[5] & 0x3F]],
                    idx_fb[6] & 0x3F, lut[idx_fb[6] & 0x3F], palette_[lut[idx_fb[6] & 0x3F]],
                    idx_fb[7] & 0x3F, lut[idx_fb[7] & 0x3F], palette_[lut[idx_fb[7] & 0x3F]],
                    idx_fb[8] & 0x3F, lut[idx_fb[8] & 0x3F], palette_[lut[idx_fb[8] & 0x3F]],
                    idx_fb[9] & 0x3F, lut[idx_fb[9] & 0x3F], palette_[lut[idx_fb[9] & 0x3F]],
                    idx_fb[10] & 0x3F, lut[idx_fb[10] & 0x3F], palette_[lut[idx_fb[10] & 0x3F]],
                    idx_fb[11] & 0x3F, lut[idx_fb[11] & 0x3F], palette_[lut[idx_fb[11] & 0x3F]],
                    idx_fb[12] & 0x3F, lut[idx_fb[12] & 0x3F], palette_[lut[idx_fb[12] & 0x3F]],
                    idx_fb[13] & 0x3F, lut[idx_fb[13] & 0x3F], palette_[lut[idx_fb[13] & 0x3F]],
                    idx_fb[14] & 0x3F, lut[idx_fb[14] & 0x3F], palette_[lut[idx_fb[14] & 0x3F]],
                    idx_fb[15] & 0x3F, lut[idx_fb[15] & 0x3F], palette_[lut[idx_fb[15] & 0x3F]]);
            }

            fprintf(stderr,
                "[EAR6_RGB16_F1]"
                " %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X"
                " %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X"
                " %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X"
                " %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X\n",
                rgba_framebuffer_[0], rgba_framebuffer_[1], rgba_framebuffer_[2],
                rgba_framebuffer_[4], rgba_framebuffer_[5], rgba_framebuffer_[6],
                rgba_framebuffer_[8], rgba_framebuffer_[9], rgba_framebuffer_[10],
                rgba_framebuffer_[12], rgba_framebuffer_[13], rgba_framebuffer_[14],
                rgba_framebuffer_[16], rgba_framebuffer_[17], rgba_framebuffer_[18],
                rgba_framebuffer_[20], rgba_framebuffer_[21], rgba_framebuffer_[22],
                rgba_framebuffer_[24], rgba_framebuffer_[25], rgba_framebuffer_[26],
                rgba_framebuffer_[28], rgba_framebuffer_[29], rgba_framebuffer_[30],
                rgba_framebuffer_[32], rgba_framebuffer_[33], rgba_framebuffer_[34],
                rgba_framebuffer_[36], rgba_framebuffer_[37], rgba_framebuffer_[38],
                rgba_framebuffer_[40], rgba_framebuffer_[41], rgba_framebuffer_[42],
                rgba_framebuffer_[44], rgba_framebuffer_[45], rgba_framebuffer_[46],
                rgba_framebuffer_[48], rgba_framebuffer_[49], rgba_framebuffer_[50],
                rgba_framebuffer_[52], rgba_framebuffer_[53], rgba_framebuffer_[54],
                rgba_framebuffer_[56], rgba_framebuffer_[57], rgba_framebuffer_[58],
                rgba_framebuffer_[60], rgba_framebuffer_[61], rgba_framebuffer_[62]);

            fprintf(stderr, "[EAR6_PAL0_F1] %02X\n", ppu->get_palette_ram0() & 0x3F);
        }
    }

    return 0;
}

} // namespace ear6
