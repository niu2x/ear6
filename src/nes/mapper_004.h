#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper004 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_vram_address_hook() override { return true; }
    void notify_vram_address_change(uint16_t addr) override;
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_state();
    void update_mirroring();
    void update_prg_mapping();
    void update_chr_mapping();
    bool is_a12_rising_edge(uint16_t addr);

    uint8_t irq_reload_value_ = 0;
    uint8_t irq_counter_ = 0;
    bool irq_reload_ = false;
    bool irq_enabled_ = false;
    bool force_mmc3_rev_a_irqs_ = false;
    uint8_t prg_mode_ = 0;
    uint8_t chr_mode_ = 0;
    uint8_t current_register_ = 0;
    uint8_t registers_[8] = {};

    bool wram_enabled_ = false;
    bool wram_write_protected_ = false;
    uint8_t reg_a000_ = 0;

    uint8_t a12_low_counter_ = 0;

    std::vector<uint8_t> work_ram_;
};

} // namespace ear6::nes
