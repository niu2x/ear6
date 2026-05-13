#include <ear6/ear6.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static const uint32_t nes_rgb_palette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

static void write_ppm(const char* path, const uint16_t* fb, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        uint8_t pal_idx = fb[i] & 0x3F;
        uint32_t rgb = nes_rgb_palette[pal_idx];
        fputc((rgb >> 16) & 0xFF, f);
        fputc((rgb >> 8) & 0xFF, f);
        fputc(rgb & 0xFF, f);
    }
    fclose(f);
    printf("Saved %s\n", path);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <rom.nes>\n", prog);
    printf("Options:\n");
    printf("  -f <frames>   Number of frames to run (default: 60)\n");
    printf("  -o <file.ppm> Output PPM screenshot (default: out.ppm)\n");
    printf("  -v            Verbose: print frame info\n");
    printf("  -h            Show this help\n");
}

int main(int argc, char* argv[]) {
    const char* rom_path = nullptr;
    int frames = 60;
    const char* output = "out.ppm";
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            rom_path = argv[i];
        }
    }

    if (!rom_path) {
        print_usage(argv[0]);
        return 1;
    }

    FILE* f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> rom(size);
    if (fread(rom.data(), 1, size, f) != (size_t)size) {
        fprintf(stderr, "Error: short read\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    if (!ctx) {
        fprintf(stderr, "Error: ear6_create failed\n");
        return 1;
    }

    int rc = ear6_load(ctx, rom.data(), (int)rom.size());
    if (rc != 0) {
        fprintf(stderr, "Error: ear6_load failed (%d)\n", rc);
        ear6_destroy(ctx);
        return 1;
    }

    if (verbose) {
        printf("ROM: %s  frames: %d\n", rom_path, frames);
    }

    for (int i = 0; i < frames; ++i) {
        ear6_step(ctx);
        if (verbose && (i % 60 == 0)) {
            printf("frame %d\n", i);
        }
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    int w = ear6_get_frame_width(ctx);
    int h = ear6_get_frame_height(ctx);

    if (fb && w > 0 && h > 0) {
        write_ppm(output, reinterpret_cast<const uint16_t*>(fb), w, h);
    } else {
        fprintf(stderr, "Error: no framebuffer data\n");
    }

    ear6_destroy(ctx);
    return 0;
}
