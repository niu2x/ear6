#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper000 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
    void reset(bool soft_reset) override;
    void write_ram(uint16_t addr, uint8_t value) override;
    uint8_t read_ram(uint16_t addr) override;
    void get_memory_ranges(MemoryRanges& ranges) override;

private:
    bool chr_is_ram_ = false;
};

} // namespace ear6::nes
