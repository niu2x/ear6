#pragma once
#include "base_mapper.h"

namespace ear6::nes {

class Mapper060 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) override;
    void reset(bool soft_reset) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
private:
    uint8_t reset_counter_ = 0;
};

} // namespace ear6::nes
