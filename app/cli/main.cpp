#include <ear6/ear6.h>
#include <ear6/nes.h>

#include <boost/program_options.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static void write_ppm(const char* path, const uint8_t* fb, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        fputc(fb[i * 4 + 0], f);
        fputc(fb[i * 4 + 1], f);
        fputc(fb[i * 4 + 2], f);
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

static SystemHint parse_system_hint(const std::string& s) {
    if (s == "nes" || s == "NES") return SystemHint::Nes;
    if (s == "flash" || s == "FLASH") return SystemHint::Flash;
    if (s == "test" || s == "TEST") return SystemHint::Test;
    if (s == "auto" || s == "AUTO") return SystemHint::Auto;
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

    int rc = ear6_load_data(ctx, rom.data(), (int)rom.size(), rom_path);
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

        // Press Start after frame 30 completes, release after frame 35 completes
        // (matches mesen2-cli timing: notification callback fires after each frame)
        if (i == 28) {
            ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 1);
            if (verbose) printf("[%d/%d] press Start\n", i, frames);
        } else if (i == 33) {
            ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 0);
            if (verbose) printf("[%d/%d] release Start\n", i, frames);
        }
        if (verbose && (i % 60 == 0 || i == frames - 1)) {
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
        write_ppm(output, fb, w, h);
    } else {
        fprintf(stderr, "Error: no framebuffer data\n");
        ear6_destroy(ctx);
        return 1;
    }

    ear6_destroy(ctx);
    return 0;
}

// -----------------------------------------------------------------------
// WAV file writer
// -----------------------------------------------------------------------

struct AudioWriter {
    FILE* file = nullptr;
    long total_samples = 0; // total stereo pairs written
    int total_frames = 0;

    bool open(const char* path) {
        file = fopen(path, "wb");
        if (!file) return false;

        uint8_t header[44] = {};
        fwrite(header, 1, 44, file);
        return true;
    }

    void write(const int16_t* samples, int count) {
        if (!file || !samples || count <= 0) return;
        fwrite(samples, sizeof(int16_t), (size_t)count * 2, file);
        total_samples += count;
    }

    void close() {
        if (!file) return;

        // Compute effective sample rate from actual audio generated
        uint32_t data_size = (uint32_t)(sizeof(int16_t) * 2 * total_samples);
        uint32_t file_size = data_size + 36;
        uint32_t sample_rate = (total_frames > 0)
            ? (uint32_t)((double)total_samples * 60.0 / (double)total_frames)
            : 44100;
        uint16_t channels = 2;
        uint16_t bits_per_sample = 16;
        uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
        uint16_t block_align = (uint16_t)(channels * bits_per_sample / 8);

        fseek(file, 0, SEEK_SET);

        uint8_t header[44];
        memcpy(header, "RIFF", 4);
        memcpy(header + 4, &file_size, 4);
        memcpy(header + 8, "WAVE", 4);
        memcpy(header + 12, "fmt ", 4);
        uint32_t fmt_size = 16;
        memcpy(header + 16, &fmt_size, 4);
        uint16_t audio_fmt = 1;
        memcpy(header + 20, &audio_fmt, 2);
        memcpy(header + 22, &channels, 2);
        memcpy(header + 24, &sample_rate, 4);
        memcpy(header + 28, &byte_rate, 4);
        memcpy(header + 32, &block_align, 2);
        memcpy(header + 34, &bits_per_sample, 2);
        memcpy(header + 36, "data", 4);
        memcpy(header + 40, &data_size, 4);

        fwrite(header, 1, 44, file);
        fclose(file);
        file = nullptr;

        printf("Audio: %ld stereo samples, %d frames, %u Hz\n",
               total_samples, total_frames, sample_rate);
    }
};

// Audio callback
struct RecordContext {
    AudioWriter* writer = nullptr;
};

static void on_audio(const int16_t* data, int num_samples, void* user_data) {
    auto* ctx = static_cast<RecordContext*>(user_data);
    ctx->writer->write(data, num_samples);
}

// -----------------------------------------------------------------------
// Subcommand: record
// -----------------------------------------------------------------------

static int cmd_record(const char* rom_path, int frames, const char* output,
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

    int rc = ear6_load_data(ctx, rom.data(), (int)rom.size(), rom_path);
    if (rc != 0) {
        fprintf(stderr, "Error: ear6_load failed (%d)\n", rc);
        ear6_destroy(ctx);
        return 1;
    }

    AudioWriter writer;
    if (!writer.open(output)) {
        fprintf(stderr, "Error: cannot write '%s'\n", output);
        ear6_destroy(ctx);
        return 1;
    }

    writer.total_frames = frames;

    RecordContext record_ctx;
    record_ctx.writer = &writer;
    ear6_set_audio_callback(ctx, on_audio, &record_ctx);

    if (verbose) {
        printf("ROM: %s  system: %s  frames: %d  output: %s\n",
               rom_path, system_hint_name(detected), frames, output);
    }

    for (int i = 0; i < frames; ++i) {
        // Press Start 3 times, each for 10 frames, 60 frames apart
        if (i < 3 * 60) {
            int block = i / 60;
            int offset = i - block * 60;
            if (offset == 0) {
                ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 1);
                if (verbose) printf("[%d/%d] press Start\n", i, frames);
            } else if (offset == 10) {
                ear6_nes_set_button_state(ctx, EAR6_NES_BUTTON_START, 0);
                if (verbose) printf("[%d/%d] release Start\n", i, frames);
            }
        }

        ear6_step(ctx);

        if (verbose && (i % 60 == 0 || i == frames - 1)) {
            printf("[%d/%d] frame %d\n", i, frames, i);
        }
    }

    ear6_set_audio_callback(ctx, nullptr, nullptr);

    writer.close();
    printf("Written %ld stereo samples to %s\n", writer.total_samples, output);
    printf("Play with: ffplay -f wav %s\n", output);

    ear6_destroy(ctx);
    return 0;
}

static int dispatch_record(const std::vector<std::string>& args,
                           SystemHint system_hint) {
    po::options_description desc("record options");
    desc.add_options()
        ("help,h", "Show this help")
        ("frames,f", po::value<int>()->default_value(600), "Number of frames to run")
        ("output,o", po::value<std::string>()->default_value("out.wav"), "Output WAV file")
        ("verbose,v", po::bool_switch(), "Print frame info")
        ("rom", po::value<std::string>(), "ROM file path");

    po::positional_options_description pos;
    pos.add("rom", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(args)
                      .options(desc)
                      .positional(pos)
                      .run(),
                  vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << "Usage: ear6-cli record [options] <rom>\n\n" << desc << std::endl;
        return 0;
    }

    if (!vm.count("rom")) {
        std::cerr << "Error: ROM file is required\n\n" << desc << std::endl;
        return 1;
    }

    return cmd_record(
        vm["rom"].as<std::string>().c_str(),
        vm["frames"].as<int>(),
        vm["output"].as<std::string>().c_str(),
        vm["verbose"].as<bool>(),
        system_hint
    );
}

// -----------------------------------------------------------------------
// Subcommand dispatch using boost::program_options
// -----------------------------------------------------------------------

static int dispatch_screenshot(const std::vector<std::string>& args,
                               SystemHint system_hint) {
    po::options_description desc("screenshot options");
    desc.add_options()
        ("help,h", "Show this help")
        ("frames,f", po::value<int>()->default_value(60), "Number of frames to run")
        ("output,o", po::value<std::string>()->default_value("out.ppm"), "Output PPM screenshot")
        ("verbose,v", po::bool_switch(), "Print frame info")
        ("rom", po::value<std::string>(), "ROM file path");

    po::positional_options_description pos;
    pos.add("rom", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(args)
                      .options(desc)
                      .positional(pos)
                      .run(),
                  vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << "Usage: ear6-cli screenshot [options] <rom>\n\n" << desc << std::endl;
        return 0;
    }

    if (!vm.count("rom")) {
        std::cerr << "Error: ROM file is required\n\n" << desc << std::endl;
        return 1;
    }

    return cmd_screenshot(
        vm["rom"].as<std::string>().c_str(),
        vm["frames"].as<int>(),
        vm["output"].as<std::string>().c_str(),
        vm["verbose"].as<bool>(),
        system_hint
    );
}

static int dispatch_info(const std::vector<std::string>& args,
                         SystemHint system_hint) {
    po::options_description desc("info options");
    desc.add_options()
        ("help,h", "Show this help")
        ("rom", po::value<std::string>(), "ROM file path");

    po::positional_options_description pos;
    pos.add("rom", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(args)
                      .options(desc)
                      .positional(pos)
                      .run(),
                  vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << "Usage: ear6-cli info [options] <rom>\n\n" << desc << std::endl;
        return 0;
    }

    if (!vm.count("rom")) {
        std::cerr << "Error: ROM file is required\n\n" << desc << std::endl;
        return 1;
    }

    return cmd_info(vm["rom"].as<std::string>().c_str(), system_hint);
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    po::options_description global_opts("Global options");
    global_opts.add_options()
        ("help,h", "Show this help")
        ("system", po::value<std::string>()->default_value("auto"),
         "System type (auto, nes, flash, test)")
        ("command", po::value<std::string>(), "Subcommand")
        ("subargs", po::value<std::vector<std::string>>(), "Subcommand arguments");

    po::positional_options_description pos;
    pos.add("command", 1).add("subargs", -1);

    po::variables_map vm;
    std::vector<std::string> all_unrec;
    try {
        auto parsed = po::command_line_parser(argc, argv)
                          .options(global_opts)
                          .positional(pos)
                          .allow_unregistered()
                          .run();
        po::store(parsed, vm);
        po::notify(vm);
        all_unrec = po::collect_unrecognized(parsed.options, po::include_positional);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n\n"
                  << "Usage: ear6-cli [options] <command> [args]\n\n"
                  << global_opts << "\n"
                  << "Commands:\n"
                  << "  screenshot   Run emulation and save a PPM screenshot\n"
                  << "  info         Display ROM file information\n"
                  << "  record       Run emulation and record audio to WAV\n"
                  << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << "Usage: ear6-cli [options] <command> [args]\n\n"
                  << global_opts << "\n"
                  << "Commands:\n"
                  << "  screenshot   Run emulation and save a PPM screenshot\n"
                  << "  info         Display ROM file information\n"
                  << "  record       Run emulation and record audio to WAV\n\n"
                  << "Use 'ear6-cli <command> --help' for command-specific help.\n"
                  << std::endl;
        return 0;
    }

    SystemHint system_hint = parse_system_hint(vm["system"].as<std::string>());
    const std::string& sys_val = vm["system"].as<std::string>();
    if (system_hint == SystemHint::Auto
        && sys_val != "auto" && sys_val != "AUTO") {
        std::cerr << "Warning: unknown system type '" << sys_val
                  << "', using auto-detect\n";
        system_hint = SystemHint::Auto;
    }

    if (all_unrec.empty()) {
        std::cout << "Usage: ear6-cli [options] <command> [args]\n\n"
                  << global_opts << "\n"
                  << "Commands:\n"
                  << "  screenshot   Run emulation and save a PPM screenshot\n"
                  << "  info         Display ROM file information\n"
                  << "  record       Run emulation and record audio to WAV\n"
                  << std::endl;
        return 1;
    }

    const std::string& cmd = all_unrec[0];
    std::vector<std::string> subargs(all_unrec.begin() + 1, all_unrec.end());

    if (cmd == "screenshot") {
        return dispatch_screenshot(subargs, system_hint);
    }
    if (cmd == "info") {
        return dispatch_info(subargs, system_hint);
    }
    if (cmd == "record") {
        return dispatch_record(subargs, system_hint);
    }

    std::cerr << "Unknown command: " << cmd << "\n\n"
              << "Usage: ear6-cli [options] <command> [args]\n\n"
              << global_opts << "\n"
              << "Commands:\n"
              << "  screenshot   Run emulation and save a PPM screenshot\n"
              << "  info         Display ROM file information\n"
              << "  record       Run emulation and record audio to WAV\n"
              << std::endl;
    return 1;
}
