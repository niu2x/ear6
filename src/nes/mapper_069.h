#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class Mapper069 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    uint32_t get_work_ram_size() override { return 0x8000; }
    uint32_t get_save_ram_size() override { return 0x8000; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;
    void setup_default_work_ram() override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_work_ram();
    void write_audio_register(uint16_t addr, uint8_t value);
    void clock_audio();
    uint16_t get_audio_period(uint8_t channel) const;
    uint8_t get_audio_volume(uint8_t channel) const;
    bool is_tone_enabled(uint8_t channel) const;
    void update_audio_channel(uint8_t channel);
    void update_audio_output();

    uint8_t command_ = 0;
    uint8_t work_ram_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_counter_enabled_ = false;
    uint16_t irq_counter_ = 0;

    std::array<uint8_t, 16> audio_volume_lut_ = {};
    std::array<uint8_t, 16> audio_registers_ = {};
    std::array<int16_t, 3> audio_timers_ = {};
    std::array<uint8_t, 3> audio_tone_steps_ = {};
    uint8_t current_audio_register_ = 0;
    int16_t last_audio_output_ = 0;
    bool process_audio_tick_ = false;
};

} // namespace ear6::nes
