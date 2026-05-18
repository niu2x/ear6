#pragma once

#include "base_mapper.h"

namespace ear6::nes {

class Mapper009 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;

    uint16_t get_prg_page_size() override;
    uint16_t get_chr_page_size() override { return 0x1000; }
    bool has_vram_address_hook() override { return true; }
    void notify_vram_address_change(uint16_t addr) override;

protected:
    void write_register(uint16_t addr, uint8_t value) override;

private:
    void update_chr_mapping();

    uint8_t left_latch_ = 1;
    uint8_t right_latch_ = 1;
    uint8_t prg_page_ = 0;
    uint8_t left_chr_page_[2] = {};
    uint8_t right_chr_page_[2] = {};
};

} // namespace ear6::nes
