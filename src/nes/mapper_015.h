#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper015 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    void reset(bool soft_reset) override;

    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x2000; }

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    bool enable_chr_ram_write_protection_ = true;
};

} // namespace ear6::nes
