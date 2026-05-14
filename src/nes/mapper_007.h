#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper007 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x8000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
    void write_register(uint16_t addr, uint8_t value) override;
};

} // namespace ear6::nes
