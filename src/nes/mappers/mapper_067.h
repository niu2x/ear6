#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper067 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;

    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x0800; }

    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    uint16_t irq_counter_ = 0;
    bool irq_latch_ = false;
    bool irq_enabled_ = false;
};

} // namespace ear6::nes
