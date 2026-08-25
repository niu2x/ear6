#pragma once

#include "mapper_004.h"

namespace ear6::nes {

class Mapper074 : public Mapper004 {
public:
    uint32_t get_chr_ram_size() override { return 0x0800; }
    uint16_t get_chr_ram_page_size() override { return 0x0400; }

protected:
    void select_chr_page(uint16_t slot, uint16_t page,
                         ChrMemoryType type = ChrMemoryType::DEFAULT) override;
};

} // namespace ear6::nes
