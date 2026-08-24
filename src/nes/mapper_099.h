#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper099 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
    uint32_t get_work_ram_size() override { return 0x0800; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;
    void setup_default_work_ram() override;

private:
    uint8_t prg_chr_select_bit_ = 0;
};

} // namespace ear6::nes
