#pragma once
#include "base_mapper.h"

namespace ear6::nes {

class Mapper032 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
protected:
    void write_register(uint16_t addr, uint8_t value) override;
private:
    uint8_t prg_regs_[2] = {};
    uint8_t prg_mode_ = 0;
    void update_prg_mode();
};

} // namespace ear6::nes
