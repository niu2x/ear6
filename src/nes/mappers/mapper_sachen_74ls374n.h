#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class MapperSachen74LS374N : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x8000; }
    uint16_t get_chr_page_size() override { return 0x2000; }

protected:
    bool allow_register_read() override { return true; }
    uint8_t read_register(uint16_t addr) override;
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_state();
    uint8_t current_register_ = 0;
    uint8_t registers_[8] = {};
};

} // namespace ear6::nes
