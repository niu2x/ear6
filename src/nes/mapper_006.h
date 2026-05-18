#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper006 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;

    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    uint32_t get_chr_ram_size() override { return 0x8000; }

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    uint16_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    bool ffe_alt_mode_ = true;
};

} // namespace ear6::nes
