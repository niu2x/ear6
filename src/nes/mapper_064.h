#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class Mapper064 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    bool has_vram_address_hook() override { return true; }
    void process_cpu_clock() override;
    void notify_vram_address_change(uint16_t addr) override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_state();
    void clock_irq_counter(uint8_t delay);
    bool is_a12_rising_edge(uint16_t addr);

    std::array<uint8_t, 16> registers_ = {};
    uint8_t current_register_ = 0;
    bool irq_enabled_ = false;
    bool irq_cycle_mode_ = false;
    bool need_reload_ = false;
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    uint8_t cpu_clock_counter_ = 0;
    uint8_t irq_delay_ = 0;
    bool force_clock_ = false;
    uint64_t a12_low_clock_ = 0;
};

} // namespace ear6::nes
