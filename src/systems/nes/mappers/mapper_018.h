#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper018 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
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
    void update_prg_bank(uint8_t bank, uint8_t value, bool upper_bits);
    void update_chr_bank(uint8_t bank, uint8_t value, bool upper_bits);
    void reload_irq_counter();

    static constexpr uint16_t IRQ_MASKS[4] = {0xFFFF, 0x0FFF, 0x00FF, 0x000F};

    uint8_t prg_banks_[3] = {};
    uint8_t chr_banks_[8] = {};
    uint8_t irq_reload_value_[4] = {};
    uint16_t irq_counter_ = 0;
    uint8_t irq_counter_size_ = 0;
    bool irq_enabled_ = false;
};

} // namespace ear6::nes
