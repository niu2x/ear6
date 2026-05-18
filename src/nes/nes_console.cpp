#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include "nes_sound_mixer.h"
#include "nes_memory_manager.h"
#include "nes_control_manager.h"
#include "vs_control_manager.h"
#include "base_mapper.h"
#include "ines_loader.h"
#include "mapper_factory.h"
#include "nes_db_embedded.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void apply_nesdb_overrides(ear6::nes::RomInfo& info, uint32_t prg_chr_crc32) {
    enum NesDbField {
        FIELD_CRC = 0,
        FIELD_SYSTEM = 1,
        FIELD_BOARD = 2,
        FIELD_PCB = 3,
        FIELD_CHIP = 4,
        FIELD_MAPPER = 5,
        FIELD_PRG_ROM_SIZE = 6,
        FIELD_CHR_ROM_SIZE = 7,
        FIELD_CHR_RAM_SIZE = 8,
        FIELD_WORK_RAM_SIZE = 9,
        FIELD_SAVE_RAM_SIZE = 10,
        FIELD_BATTERY = 11,
        FIELD_MIRRORING = 12,
        FIELD_INPUT_TYPE = 13,
        FIELD_BUS_CONFLICTS = 14,
        FIELD_SUB_MAPPER = 15,
        FIELD_VS_SYSTEM_TYPE = 16,
        FIELD_PPU_MODEL = 17,
    };

    struct NesDbEntry {
        bool has_mapper = false;
        int mapper = 0;
        bool has_battery = false;
        bool battery = false;
        bool has_bus_conflicts = false;
        ear6::nes::RomInfo::BusConflictType bus_conflicts = ear6::nes::RomInfo::BusConflictType::DEFAULT;
        bool has_submapper = false;
        int submapper = 0;
        bool has_input_type = false;
        int input_type = 0;
        bool has_ppu_model = false;
        int ppu_model = 0;
        bool is_vs_system = false;
        bool has_mirroring = false;
        ear6::nes::MirroringType mirroring = ear6::nes::MirroringType::HORIZONTAL;
    };

    static bool loaded = false;
    static std::unordered_map<uint32_t, NesDbEntry> db;

    if (!loaded) {
        loaded = true;
        std::istringstream in(get_embedded_nes_db_text());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string part;
            while (std::getline(ss, part, ',')) {
                fields.push_back(part);
            }

            if (fields.size() < 14) {
                continue;
            }

            uint32_t crc = static_cast<uint32_t>(std::strtoul(fields[0].c_str(), nullptr, 16));
            if (crc == 0) {
                continue;
            }

            NesDbEntry entry;
            entry.is_vs_system = (fields[FIELD_SYSTEM] == "VsSystem");

            if (!fields[FIELD_MAPPER].empty()) {
                int mapper = std::atoi(fields[FIELD_MAPPER].c_str());
                if (mapper >= 0 && mapper < 1024) {
                    entry.has_mapper = true;
                    entry.mapper = mapper;
                }
            }

            if (!fields[FIELD_BATTERY].empty()) {
                entry.has_battery = true;
                entry.battery = (std::atoi(fields[FIELD_BATTERY].c_str()) != 0);
            }

            if (fields.size() > FIELD_BUS_CONFLICTS && !fields[FIELD_BUS_CONFLICTS].empty()) {
                entry.has_bus_conflicts = true;
                int bc = std::atoi(fields[FIELD_BUS_CONFLICTS].c_str());
                if (bc == 1) {
                    entry.bus_conflicts = ear6::nes::RomInfo::BusConflictType::YES;
                } else if (bc == 0) {
                    entry.bus_conflicts = ear6::nes::RomInfo::BusConflictType::NO;
                } else {
                    entry.bus_conflicts = ear6::nes::RomInfo::BusConflictType::DEFAULT;
                }
            }

            if (fields.size() > FIELD_SUB_MAPPER && !fields[FIELD_SUB_MAPPER].empty()) {
                entry.has_submapper = true;
                entry.submapper = std::atoi(fields[FIELD_SUB_MAPPER].c_str());
            }

            if (!fields[FIELD_INPUT_TYPE].empty()) {
                entry.has_input_type = true;
                entry.input_type = std::atoi(fields[FIELD_INPUT_TYPE].c_str());
            }
            if (fields.size() > FIELD_MIRRORING && !fields[FIELD_MIRRORING].empty()) {
                const std::string& m = fields[FIELD_MIRRORING];
                if (m == "h") {
                    entry.has_mirroring = true;
                    entry.mirroring = ear6::nes::MirroringType::HORIZONTAL;
                } else if (m == "v") {
                    entry.has_mirroring = true;
                    entry.mirroring = ear6::nes::MirroringType::VERTICAL;
                } else if (m == "4") {
                    entry.has_mirroring = true;
                    entry.mirroring = ear6::nes::MirroringType::FOUR_SCREENS;
                }
            }
            if (fields.size() > FIELD_PPU_MODEL && !fields[FIELD_PPU_MODEL].empty()) {
                entry.has_ppu_model = true;
                entry.ppu_model = std::atoi(fields[FIELD_PPU_MODEL].c_str());
            }
            db[crc] = entry;
        }
    }

    auto it = db.find(prg_chr_crc32);
    if (it == db.end()) {
        if (std::getenv("EAR6_TRACE_NESDB") != nullptr) {
            fprintf(stderr, "[EAR6_NESDB] miss crc=%08X\n", prg_chr_crc32);
        }
        return;
    }

    const NesDbEntry& entry = it->second;
    if (std::getenv("EAR6_TRACE_NESDB") != nullptr) {
        fprintf(stderr, "[EAR6_NESDB] hit crc=%08X mapper=%d has_mapper=%d battery=%d has_battery=%d bus=%d has_bus=%d sub=%d has_sub=%d input=%d has_input=%d vs=%d ppu=%d has_ppu=%d\n",
            prg_chr_crc32,
            entry.mapper,
            entry.has_mapper ? 1 : 0,
            entry.battery ? 1 : 0,
            entry.has_battery ? 1 : 0,
            (int)entry.bus_conflicts,
            entry.has_bus_conflicts ? 1 : 0,
            entry.submapper,
            entry.has_submapper ? 1 : 0,
            entry.input_type,
            entry.has_input_type ? 1 : 0,
            entry.is_vs_system ? 1 : 0,
            entry.ppu_model,
            entry.has_ppu_model ? 1 : 0);
    }
    if (entry.has_mapper) {
        info.mapper_number = entry.mapper;
    }
    if (entry.has_battery) {
        info.has_battery = entry.battery;
    }
    if (entry.has_bus_conflicts) {
        info.bus_conflicts = entry.bus_conflicts;
    }
    if (entry.has_submapper) {
        info.submapper_id = entry.submapper;
    }
    if (entry.is_vs_system) {
        // Keep standard control manager behavior aligned with mesen2-cli path.
        // Do not force VsControlManager from DB system field.
        info.use_vs_palette = true;
    }
    if (entry.has_ppu_model) {
        if (entry.ppu_model == 1) {
            info.vs_ppu_model = ear6::nes::RomInfo::VsPpuModel::PPU_2C03;
            info.use_vs_palette = true;
        } else if (entry.ppu_model >= 2 && entry.ppu_model <= 10) {
            info.vs_ppu_model = static_cast<ear6::nes::RomInfo::VsPpuModel>(entry.ppu_model);
            info.use_vs_palette = true;
        }
    }
    if (entry.has_input_type) {
        if (entry.input_type == 7) {
            info.input_type = ear6::nes::RomInfo::GameInputType::VS_ZAPPER;
        } else if (entry.input_type == 8) {
            info.input_type = ear6::nes::RomInfo::GameInputType::ZAPPER;
        } else if (entry.input_type == 1) {
            info.input_type = ear6::nes::RomInfo::GameInputType::STANDARD_CONTROLLERS;
        }
    }
    if (entry.has_mirroring) {
        info.mirroring = entry.mirroring;
    }
}

} // namespace

namespace ear6::nes {

NesConsole::NesConsole()
    : sound_mixer_(std::make_unique<NesSoundMixer>()) {
    init_components();
}

NesConsole::~NesConsole() = default;

void NesConsole::init_components() {
    // Created in load_rom after mapper is known
}

int NesConsole::load_rom(const void* data, int size) {
    const uint8_t* rom_data = static_cast<const uint8_t*>(data);

    if (!INesLoader::is_valid(rom_data, size)) {
        return -1;
    }

    RomInfo info = INesLoader::parse_header(rom_data);
    const int header_mapper_number = info.mapper_number;

    int prg_size = info.prg_banks * 0x4000;
    int chr_size = info.chr_banks * 0x2000;
    int header_size = 16 + (info.has_trainer ? 512 : 0);
    uint32_t prg_chr_crc32 = crc32_update(0, rom_data + header_size, (size_t)(prg_size + chr_size));
    apply_nesdb_overrides(info, prg_chr_crc32);
    if (!MapperFactory::is_supported(info.mapper_number) && MapperFactory::is_supported(header_mapper_number)) {
        info.mapper_number = header_mapper_number;
    }

    rom_info_ = info;

    printf("[NES] Mapper: %d, PRG: %d*16KB, CHR: %d*8KB\n",
           info.mapper_number, info.prg_banks, info.chr_banks);

    std::vector<uint8_t> prg_rom(prg_size);
    std::vector<uint8_t> chr_rom(chr_size);
    std::memcpy(prg_rom.data(), rom_data + header_size, prg_size);
    std::memcpy(chr_rom.data(), rom_data + header_size + prg_size, chr_size);

    mapper_.reset(MapperFactory::create(info.mapper_number));
    mapper_->initialize(this);
    if (rom_info_.bus_conflicts == RomInfo::BusConflictType::YES) {
        mapper_->set_has_bus_conflicts(true);
    } else if (rom_info_.bus_conflicts == RomInfo::BusConflictType::NO) {
        mapper_->set_has_bus_conflicts(false);
    }
    mapper_->init(info, prg_rom, chr_rom);

    // Initialize CHR RAM when no CHR ROM present (matches Mesen2's BaseMapper::Initialize)
    if (!mapper_->has_chr_rom()) {
        mapper_->initialize_chr_ram();
    }

    // Create core components
    memory_manager_.reset(new NesMemoryManager(this, mapper_.get()));
    cpu_.reset(new NesCpu(this));
    ppu_.reset(new NesPpu(this));
    apu_.reset(new NesApu(this, sound_mixer_.get()));
    if (rom_info_.is_vs_system) {
        control_manager_.reset(new VsControlManager(this, rom_info_));
        ppu_->set_no_odd_frame_skip();
    } else {
        control_manager_.reset(new NesControlManager(this));
    }
    if (rom_info_.use_vs_palette) {
        ppu_->set_no_odd_frame_skip();
    }
    if (std::getenv("EAR6_CLI_INPUT_TOPOLOGY") != nullptr) {
        control_manager_->set_cli_exp_bit3_mode(true);
    }
    // Keep input behavior aligned with mesen2-cli API path:
    // CLI config currently exposes only NES/SNES controller types,
    // so port2 remains standard controller unless explicit API support is added.

    // Register IO devices in order
    memory_manager_->register_io_device(ppu_.get());
    memory_manager_->register_io_device(apu_.get());
    memory_manager_->register_io_device(control_manager_.get());
    memory_manager_->register_io_device(mapper_.get());

    reset(false);
    return 0;
}

void NesConsole::reset(bool soft_reset) {
    // Reset order must match mesen2: PPU before CPU, so CPU's dummy cycles
    // advance the freshly-reset PPU rather than being thrown away by PPU reset.
    if (memory_manager_) memory_manager_->reset(soft_reset);
    if (ppu_) ppu_->reset(soft_reset);
    if (apu_) apu_->reset(soft_reset);
    if (cpu_) cpu_->reset(soft_reset);
    if (control_manager_) control_manager_->reset(soft_reset);
}

void NesConsole::run_frame() {
    uint32_t frame = ppu_->get_frame_count();
    while (frame == ppu_->get_frame_count()) {
        cpu_->exec();
    }
    if (ppu_->get_frame_count() > 0) {
        last_completed_ppu_frame_ = ppu_->get_frame_count() - 1;
    } else {
        last_completed_ppu_frame_ = 0;
    }
    apu_->end_frame();
    apu_->push_frame();
}

void NesConsole::process_cpu_clock() {
    if (mapper_->has_cpu_clock_hook()) {
        mapper_->process_cpu_clock();
    }
    apu_->process_cpu_clock();
    if (control_manager_->has_pending_writes()) {
        control_manager_->process_writes();
    }
}

const uint16_t* NesConsole::get_framebuffer() const {
    if (ppu_) {
        return ppu_->get_framebuffer();
    }
    return nullptr;
}

const int16_t* NesConsole::get_audiobuffer() const {
    if (apu_) {
        return apu_->get_buffer();
    }
    return nullptr;
}

int NesConsole::get_audio_num_samples() const {
    if (apu_) {
        return apu_->get_samples();
    }
    return 0;
}

void NesConsole::consume_audio() {
    if (apu_) {
        apu_->consume_audio();
    }
}

void NesConsole::set_button_state(int port, int button, bool pressed) {
    if (control_manager_) {
        control_manager_->set_button_state(port, button, pressed);
    }
}

void NesConsole::clear_input() {
    if (control_manager_) {
        control_manager_->clear_all();
    }
}

} // namespace ear6::nes
