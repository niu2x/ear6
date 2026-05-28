#pragma once
#include "mapper_226.h"

namespace ear6::nes {

class Mapper233 : public Mapper226 {
public:
    void reset(bool soft_reset) override;
protected:
    uint8_t get_prg_page_inner() override;
private:
    uint8_t reset_flag_ = 0;
};

} // namespace ear6::nes
