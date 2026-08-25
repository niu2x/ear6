#pragma once

#include "mapper_004.h"

#include <array>

namespace ear6::nes {

class Mapper045 : public Mapper004 {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;
    void select_prg_page(uint16_t slot, uint16_t page,
                         PrgMemoryType type = PrgMemoryType::PRG_ROM) override;
    void select_chr_page(uint16_t slot, uint16_t page,
                         ChrMemoryType type = ChrMemoryType::DEFAULT) override;

private:
    std::array<uint8_t, 4> outer_registers_ = {};
    uint8_t outer_register_index_ = 0;
};

} // namespace ear6::nes
