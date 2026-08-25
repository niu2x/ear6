#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper075 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x1000; }

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_chr_banks();
    uint8_t chr_banks_[2] = {};
};

} // namespace ear6::nes
