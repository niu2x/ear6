#pragma once

#include "mapper_004.h"

namespace ear6::nes {

class Mapper245 : public Mapper004 {
protected:
    void update_state() override;
    void update_prg_mapping() override;
};

} // namespace ear6::nes
