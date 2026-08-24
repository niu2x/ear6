#pragma once

#include "base_mapper.h"
#include "vrc_irq.h"

#include <array>

namespace ear6::nes {

class MapperVRC6 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    struct Pulse {
        void write_register(uint16_t addr, uint8_t value);
        void set_frequency_shift(uint8_t shift) { frequency_shift = shift; }
        void clock();
        uint8_t get_volume() const;

        uint8_t volume = 0;
        uint8_t duty_cycle = 0;
        bool ignore_duty = false;
        uint16_t frequency = 1;
        bool enabled = false;
        int32_t timer = 1;
        uint8_t step = 0;
        uint8_t frequency_shift = 0;
    };

    struct Saw {
        void write_register(uint16_t addr, uint8_t value);
        void set_frequency_shift(uint8_t shift) { frequency_shift = shift; }
        void clock();
        uint8_t get_volume() const;

        uint8_t accumulator_rate = 0;
        uint8_t accumulator = 0;
        uint16_t frequency = 1;
        bool enabled = false;
        int32_t timer = 1;
        uint8_t step = 0;
        uint8_t frequency_shift = 0;
    };

    void update_prg_ram_access();
    void set_ppu_mapping(uint8_t bank, uint8_t page);
    void update_ppu_banking();
    void write_audio_register(uint16_t addr, uint8_t value);
    void clock_audio();

    bool swap_address_lines_ = false;
    uint8_t banking_mode_ = 0;
    std::array<uint8_t, 8> chr_registers_ = {};
    VrcIrq irq_;

    Pulse pulse_1_;
    Pulse pulse_2_;
    Saw saw_;
    bool halt_audio_ = false;
    int32_t last_audio_output_ = 0;
};

} // namespace ear6::nes
