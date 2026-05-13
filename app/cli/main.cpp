#include <ear6/ear6.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

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

static bool load_file(const char* path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(size);
    if (fread(out.data(), 1, size, f) != (size_t)size) {
        fprintf(stderr, "Error: short read from %s\n", path);
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

// -----------------------------------------------------------------------
// System type detection
// -----------------------------------------------------------------------

enum class SystemHint {
    Auto,
    Nes,
    Flash,
    Test,
    Unknown
};

static const char* system_hint_name(SystemHint h) {
    switch (h) {
        case SystemHint::Auto:    return "auto";
        case SystemHint::Nes:     return "nes";
        case SystemHint::Flash:   return "flash";
        case SystemHint::Test:    return "test";
        case SystemHint::Unknown: return "unknown";
    }
    return "unknown";
}

static SystemHint parse_system_hint(const char* s) {
    if (strcmp(s, "nes") == 0 || strcmp(s, "NES") == 0) return SystemHint::Nes;
    if (strcmp(s, "flash") == 0 || strcmp(s, "FLASH") == 0) return SystemHint::Flash;
    if (strcmp(s, "test") == 0 || strcmp(s, "TEST") == 0) return SystemHint::Test;
    if (strcmp(s, "auto") == 0 || strcmp(s, "AUTO") == 0) return SystemHint::Auto;
    return SystemHint::Auto;
}

static bool has_ines_magic(const uint8_t* data, int size) {
    return size >= 16 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

static SystemHint detect_system(const uint8_t* data, int size, SystemHint hint) {
    if (hint != SystemHint::Auto) return hint;
    if (has_ines_magic(data, size)) return SystemHint::Nes;
    return SystemHint::Unknown;
}

// -----------------------------------------------------------------------
// iNES header parsing (for info command)
// -----------------------------------------------------------------------

struct NesRomInfo {
    int mapper_number = 0;
    int prg_banks = 0;
    int chr_banks = 0;
    bool has_battery = false;
    bool has_trainer = false;
    const char* mirroring_str = "unknown";
    int prg_size = 0;
    int chr_size = 0;
    const char* chr_type = "unknown";
    bool valid = false;
};

static NesRomInfo parse_nes_info(const uint8_t* data, int size) {
    NesRomInfo info;
    if (!has_ines_magic(data, size)) return info;

    info.valid = true;
    info.prg_banks = data[4];
    info.chr_banks = data[5];
    info.mapper_number = (data[6] >> 4) | (data[7] & 0xF0);
    info.has_battery = (data[6] & 0x02) != 0;
    info.has_trainer = (data[6] & 0x04) != 0;

    if (data[6] & 0x08) {
        info.mirroring_str = "Four-screen";
    } else {
        info.mirroring_str = (data[6] & 0x01) ? "Vertical" : "Horizontal";
    }

    info.prg_size = info.prg_banks * 16384;
    info.chr_size = info.chr_banks * 8192;
    info.chr_type = (info.chr_banks == 0) ? "CHR RAM" : "CHR ROM";

    return info;
}

// -----------------------------------------------------------------------
// Subcommand: info
// -----------------------------------------------------------------------

static int cmd_info(const char* rom_path, SystemHint system_hint) {
    std::vector<uint8_t> data;
    if (!load_file(rom_path, data)) return 1;

    SystemHint detected = detect_system(data.data(), (int)data.size(), system_hint);

    printf("File:  %s\n", rom_path);
    printf("Size:  %zu bytes\n", data.size());
    printf("Type:  %s", system_hint_name(detected));
    if (system_hint == SystemHint::Auto && detected != SystemHint::Auto)
        printf(" (auto-detected)");
    printf("\n");

    switch (detected) {
        case SystemHint::Nes: {
            NesRomInfo i = parse_nes_info(data.data(), (int)data.size());
            if (!i.valid) {
                printf("       (not a valid iNES ROM)\n");
                return 1;
            }
            printf("Format: iNES\n");
            printf("Mapper: %d\n", i.mapper_number);
            printf("PRG:    %d x 16KB = %d KB\n", i.prg_banks, i.prg_size / 1024);
            printf("CHR:    %d x 8KB  = %d KB  (%s)\n", i.chr_banks, i.chr_size / 1024, i.chr_type);
            printf("Mirror: %s\n", i.mirroring_str);
            printf("Battery: %s\n", i.has_battery ? "yes" : "no");
            printf("Trainer: %s\n", i.has_trainer ? "yes" : "no");
            return 0;
        }
        case SystemHint::Unknown:
            printf("       (unrecognized file format; use --system to override)\n");
            return 1;
        default:
            printf("       (info not yet available for this system type)\n");
            return 1;
    }
}

// -----------------------------------------------------------------------
// Subcommand: screenshot
// -----------------------------------------------------------------------

static Ear6SystemType to_ear6_type(SystemHint h) {
    switch (h) {
        case SystemHint::Nes:   return EAR6_SYSTEM_NES;
        case SystemHint::Test:  return EAR6_SYSTEM_TEST;
        case SystemHint::Flash: return EAR6_SYSTEM_FLASH;
        default:                return EAR6_SYSTEM_NES;
    }
}

static bool is_known(SystemHint h) {
    return h == SystemHint::Nes || h == SystemHint::Flash || h == SystemHint::Test;
}

static int cmd_screenshot(const char* rom_path, int frames, const char* output,
                          bool verbose, SystemHint system_hint) {
    std::vector<uint8_t> rom;
    if (!load_file(rom_path, rom)) return 1;

    SystemHint detected = detect_system(rom.data(), (int)rom.size(), system_hint);
    if (!is_known(detected)) {
        fprintf(stderr, "Error: cannot determine system type for '%s'\n", rom_path);
        fprintf(stderr, "Use --system <type> to specify (nes, flash, test)\n");
        return 1;
    }

    Ear6* ctx = ear6_create(to_ear6_type(detected));
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
        printf("ROM: %s  system: %s  frames: %d\n",
               rom_path, system_hint_name(detected), frames);
    }

    for (int i = 0; i < frames; ++i) {
        ear6_step(ctx);
        if (verbose && (i % 60 == 0)) {
            printf("[%d/%d] frame %d\n", i, frames, i);
        }
    }
    if (verbose) {
        printf("[%d/%d] done\n", frames, frames);
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    int w = ear6_get_frame_width(ctx);
    int h = ear6_get_frame_height(ctx);

    if (fb && w > 0 && h > 0) {
        write_ppm(output, reinterpret_cast<const uint16_t*>(fb), w, h);
    } else {
        fprintf(stderr, "Error: no framebuffer data\n");
        ear6_destroy(ctx);
        return 1;
    }

    ear6_destroy(ctx);
    return 0;
}

// -----------------------------------------------------------------------
// Help
// -----------------------------------------------------------------------

static void print_global_usage(const char* prog) {
    printf("Usage: %s [options] <command> [args]\n", prog);
    printf("\nGlobal options:\n");
    printf("  --system <type>  System type (auto, nes, flash, test; default: auto)\n");
    printf("  -h, --help       Show this help\n");
    printf("\nCommands:\n");
    printf("  screenshot   Run emulation and save a PPM screenshot\n");
    printf("  info         Display ROM file information\n");
    printf("\nUse '%s <command> --help' for command-specific help.\n", prog);
}

static void print_screenshot_usage(const char* prog) {
    printf("Usage: %s screenshot [options] <rom>\n", prog);
    printf("Options:\n");
    printf("  -f <frames>   Number of frames to run (default: 60)\n");
    printf("  -o <file.ppm> Output PPM screenshot (default: out.ppm)\n");
    printf("  -v            Verbose: print frame info\n");
    printf("  -h, --help    Show this help\n");
}

static void print_info_usage(const char* prog) {
    printf("Usage: %s info [options] <rom>\n", prog);
    printf("Options:\n");
    printf("  -h, --help    Show this help\n");
}

// -----------------------------------------------------------------------
// Subcommand dispatch
// -----------------------------------------------------------------------

static int dispatch_screenshot(int argc, char** argv, SystemHint system_hint) {
    const char* rom_path = nullptr;
    int frames = 60;
    const char* output = "out.ppm";
    bool verbose = false;

    for (int i = 0; i < argc; ++i) {
        if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)) {
            print_screenshot_usage("ear6-cli");
            return 0;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (argv[i][0] != '-') {
            rom_path = argv[i];
        }
    }

    if (!rom_path) {
        print_screenshot_usage("ear6-cli");
        return 1;
    }

    return cmd_screenshot(rom_path, frames, output, verbose, system_hint);
}

static int dispatch_info(int argc, char** argv, SystemHint system_hint) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_info_usage("ear6-cli");
            return 0;
        }
    }

    if (argc < 1 || argv[0][0] == '-') {
        print_info_usage("ear6-cli");
        return 1;
    }

    return cmd_info(argv[0], system_hint);
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    SystemHint global_system = SystemHint::Auto;

    // Consume global options (must come before subcommand)
    int pos = 1;
    while (pos < argc) {
        if (strcmp(argv[pos], "--system") == 0 && pos + 1 < argc) {
            global_system = parse_system_hint(argv[pos + 1]);
            if (global_system == SystemHint::Auto
                && strcmp(argv[pos + 1], "auto") != 0
                && strcmp(argv[pos + 1], "AUTO") != 0) {
                fprintf(stderr, "Warning: unknown system type '%s', using auto-detect\n",
                        argv[pos + 1]);
                global_system = SystemHint::Auto;
            }
            pos += 2;
        } else if (strcmp(argv[pos], "-h") == 0 || strcmp(argv[pos], "--help") == 0) {
            print_global_usage(argv[0]);
            return 0;
        } else {
            break;
        }
    }

    int remaining = argc - pos;
    char** args = argv + pos;

    if (remaining == 0) {
        print_global_usage(argv[0]);
        return 1;
    }

    // Check for known subcommands
    if (strcmp(args[0], "screenshot") == 0) {
        return dispatch_screenshot(remaining - 1, args + 1, global_system);
    }
    if (strcmp(args[0], "info") == 0) {
        return dispatch_info(remaining - 1, args + 1, global_system);
    }

    fprintf(stderr, "Unknown command: %s\n", args[0]);
    print_global_usage(argv[0]);
    return 1;
}
