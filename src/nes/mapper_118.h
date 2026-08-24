#pragma once

#include "mapper_004.h"

namespace ear6::nes {

class Mapper118 : public Mapper004 {
protected:
    void update_mirroring() override {}
    void write_register(uint16_t addr, uint8_t value) override;
};

} // namespace ear6::nes
