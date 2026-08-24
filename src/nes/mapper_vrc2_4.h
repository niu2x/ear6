#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class MapperVRC2_4 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    bool allow_register_read() override { return true; }
    void process_cpu_clock() override;
    void setup_default_work_ram() override;

protected:
    uint8_t read_register(uint16_t addr) override;
    void write_register(uint16_t addr, uint8_t value) override;

private:
    enum class Variant {
        VRC2A,
        VRC2B,
        VRC2C,
        VRC4A,
        VRC4B,
        VRC4C,
        VRC4D,
        VRC4E,
    };

    void detect_variant(const RomInfo& info);
    uint16_t translate_address(uint16_t addr) const;
    void update_state();
    void set_irq_reload_nibble(uint8_t value, bool high_bits);
    void set_irq_control(uint8_t value);
    void acknowledge_irq();

    Variant variant_ = Variant::VRC2A;
    bool use_heuristics_ = false;
    bool use_microwire_ = false;

    uint8_t prg_reg_0_ = 0;
    uint8_t prg_reg_1_ = 0;
    uint8_t prg_mode_ = 0;
    std::array<uint8_t, 8> chr_high_ = {};
    std::array<uint8_t, 8> chr_low_ = {};
    uint8_t latch_ = 0;

    uint8_t irq_reload_value_ = 0;
    uint8_t irq_counter_ = 0;
    int16_t irq_prescaler_counter_ = 0;
    bool irq_enabled_ = false;
    bool irq_enabled_after_ack_ = false;
    bool irq_cycle_mode_ = false;
};

} // namespace ear6::nes
