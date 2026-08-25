#pragma once
#include "base_mapper.h"

namespace ear6::nes {

class Mapper156 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
protected:
    void write_register(uint16_t addr, uint8_t value) override;
private:
    uint8_t chr_low_[8] = {};
    uint8_t chr_high_[8] = {};
    void update_chr_banks();
};

} // namespace ear6::nes
