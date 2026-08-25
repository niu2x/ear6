#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class Mapper068 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x0800; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;
    void setup_default_work_ram() override;
    void write_ram(uint16_t addr, uint8_t value) override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_nametables();
    void update_state();

    std::array<uint8_t, 2> nametable_registers_ = {};
    bool use_chr_nametables_ = false;
    bool prg_ram_enabled_ = false;
    uint32_t licensing_timer_ = 0;
    bool using_external_rom_ = false;
    uint8_t external_page_ = 0;
};

} // namespace ear6::nes
