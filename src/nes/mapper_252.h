#pragma once

#include "base_mapper.h"
#include "vrc_irq.h"

#include <array>

namespace ear6::nes {

class Mapper252 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_state();

    std::array<uint8_t, 8> chr_registers_ = {};
    VrcIrq irq_;
};

} // namespace ear6::nes
