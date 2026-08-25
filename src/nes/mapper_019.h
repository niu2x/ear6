#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class Mapper019 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    bool allow_register_read() override { return true; }
    void process_cpu_clock() override;
    void setup_default_work_ram() override {}
    void write_ram(uint16_t addr, uint8_t value) override;

protected:
    uint8_t read_register(uint16_t addr) override;
    void write_register(uint16_t addr, uint8_t value) override;

private:
    enum class Variant {
        NAMCO163,
        NAMCO175,
        NAMCO340,
        UNKNOWN,
    };

    void set_variant(Variant variant);
    void update_save_ram_access();
    void map_chr_page(uint8_t slot, uint8_t value, bool allow_nametable);
    void clock_audio();
    void update_audio_channel(uint8_t channel);
    void update_audio_output();
    uint32_t get_audio_frequency(uint8_t channel) const;
    uint32_t get_audio_phase(uint8_t channel) const;
    void set_audio_phase(uint8_t channel, uint32_t phase);

    Variant variant_ = Variant::NAMCO163;
    bool auto_detect_variant_ = false;
    bool not_namco340_ = false;
    uint8_t write_protect_ = 0;
    bool low_chr_nt_mode_ = false;
    bool high_chr_nt_mode_ = false;
    uint16_t irq_counter_ = 0;

    std::array<uint8_t, 0x80> audio_ram_ = {};
    std::array<int16_t, 8> channel_output_ = {};
    uint8_t audio_ram_position_ = 0;
    bool audio_auto_increment_ = false;
    uint8_t audio_update_counter_ = 0;
    int8_t current_audio_channel_ = 7;
    int16_t last_audio_output_ = 0;
    bool audio_disabled_ = false;
};

} // namespace ear6::nes
