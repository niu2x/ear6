#pragma once

#include "nes_types.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace ear6::nes {

class NesCpu;
class NesPpu;
class NesApu;
class NesMemoryManager;
class NesControlManager;
class BaseMapper;
class NesSoundMixer;

class NesConsole {
public:
    NesConsole();
    ~NesConsole();

    int load_rom(const void* data, int size);
    void reset(bool soft_reset);
    void run_frame();

    // CPU memory access
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t value);

    // Component access
    NesCpu* get_cpu() { return cpu_.get(); }
    NesPpu* get_ppu() { return ppu_.get(); }
    NesApu* get_apu() { return apu_.get(); }
    BaseMapper* get_mapper() { return mapper_.get(); }
    NesMemoryManager* get_memory_manager() { return memory_manager_.get(); }
    NesControlManager* get_control_manager() { return control_manager_.get(); }
    NesSoundMixer* get_sound_mixer() { return sound_mixer_.get(); }

    void process_cpu_clock();

    // Framebuffer access
    const uint16_t* get_framebuffer() const;
    int get_frame_width() const { return 256; }
    int get_frame_height() const { return 240; }

    // Audio
    const int16_t* get_audiobuffer() const;
    int get_audio_num_samples() const;
    void consume_audio();

    // Input
    void set_button_state(int port, int button, bool pressed);
    void clear_input();

private:
    void init_components();

    std::unique_ptr<NesCpu> cpu_;
    std::unique_ptr<NesPpu> ppu_;
    std::unique_ptr<NesApu> apu_;
    std::unique_ptr<NesSoundMixer> sound_mixer_;
    std::unique_ptr<NesMemoryManager> memory_manager_;
    std::unique_ptr<NesControlManager> control_manager_;
    std::unique_ptr<BaseMapper> mapper_;

    RomInfo rom_info_;
};

} // namespace ear6::nes
