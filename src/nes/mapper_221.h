#pragma once
#include "base_mapper.h"

namespace ear6::nes {

class Mapper221 : public BaseMapper {
public:
    void init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
protected:
    void write_register(uint16_t addr, uint8_t value) override;
private:
    uint16_t mode_ = 0;
    uint8_t prg_reg_ = 0;
    void update_state();
};

} // namespace ear6::nes
