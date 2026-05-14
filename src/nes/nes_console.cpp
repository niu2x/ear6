#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include "nes_sound_mixer.h"
#include "nes_memory_manager.h"
#include "nes_control_manager.h"
#include "base_mapper.h"
#include "ines_loader.h"
#include "mapper_factory.h"
#include <cstdio>
#include <cstring>

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
    rom_info_ = info;

    printf("[NES] Mapper: %d, PRG: %d*16KB, CHR: %d*8KB\n",
           info.mapper_number, info.prg_banks, info.chr_banks);

    int prg_size = info.prg_banks * 0x4000;
    int chr_size = info.chr_banks * 0x2000;
    int header_size = (rom_data[6] & 0x04) ? 32 : 16;

    std::vector<uint8_t> prg_rom(prg_size);
    std::vector<uint8_t> chr_rom(chr_size);
    std::memcpy(prg_rom.data(), rom_data + header_size, prg_size);
    std::memcpy(chr_rom.data(), rom_data + header_size + prg_size, chr_size);

    mapper_.reset(MapperFactory::create(info.mapper_number));
    mapper_->initialize(this);
    mapper_->init(info, prg_rom, chr_rom);

    // Create core components
    memory_manager_.reset(new NesMemoryManager(this, mapper_.get()));
    cpu_.reset(new NesCpu(this));
    ppu_.reset(new NesPpu(this));
    apu_.reset(new NesApu(this, sound_mixer_.get()));
    control_manager_.reset(new NesControlManager(this));

    // Register IO devices in order
    memory_manager_->register_io_device(ppu_.get());
    memory_manager_->register_io_device(apu_.get());
    memory_manager_->register_io_device(control_manager_.get());
    memory_manager_->register_io_device(mapper_.get());

    reset(false);
    return 0;
}

void NesConsole::reset(bool soft_reset) {
    if (cpu_) cpu_->reset(soft_reset);
    if (ppu_) ppu_->reset(soft_reset);
    if (apu_) apu_->reset(soft_reset);
    if (memory_manager_) memory_manager_->reset(soft_reset);
    if (control_manager_) control_manager_->reset(soft_reset);
}

void NesConsole::run_frame() {
    uint32_t frame = ppu_->get_frame_count();
    while (frame == ppu_->get_frame_count()) {
        cpu_->exec();
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
