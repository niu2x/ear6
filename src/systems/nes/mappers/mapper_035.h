#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper035 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_vram_address_hook() override { return true; }
    void notify_vram_address_change(uint16_t addr) override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    bool is_a12_rising_edge(uint16_t addr);
    uint8_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    uint64_t a12_low_clock_ = 0;
};

} // namespace ear6::nes
