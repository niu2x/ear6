#pragma once

#include "base_mapper.h"
#include <cstdint>
#include <vector>

namespace ear6::nes {

class Mapper001 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x1000; }
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void reset_buffer();
    void process_bit_write(uint16_t addr, uint8_t value);
    void process_register_write(uint16_t addr, uint8_t val);
    void update_state();

    std::vector<uint8_t> work_ram_;

    uint8_t write_buffer_ = 0;
    uint8_t shift_count_ = 0;
    bool wram_disable_ = false;
    bool chr_mode_ = false;
    bool prg_mode_ = false;
    bool slot_select_ = false;
    uint8_t chr_reg0_ = 0;
    uint8_t chr_reg1_ = 0;
    uint8_t prg_reg_ = 0;
    uint64_t last_write_cycle_ = 0;
    uint16_t last_chr_reg_ = 0;
};

} // namespace ear6::nes
