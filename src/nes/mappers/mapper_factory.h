#pragma once

#include <memory>
#include "base_mapper.h"

namespace ear6::nes {

class MapperFactory {
public:
    static BaseMapper* create(int mapper_number);
    static bool is_supported(int mapper_number);
};

} // namespace ear6::nes
