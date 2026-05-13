#pragma once

#include "nes_types.h"
#include <cstdint>

namespace ear6::nes {

class INesLoader {
public:
    static bool is_valid(const uint8_t* data, int size);
    static RomInfo parse_header(const uint8_t* data);
};

} // namespace ear6::nes
