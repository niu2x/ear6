#pragma once

#include "base_mapper.h"
#include <cstdint>
#include <vector>

namespace ear6::nes {

class Mapper002 : public BaseMapper {
public:
    void serialize(ear6::StateStream& stream) override;
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x2000; }
    void write_register(uint16_t addr, uint8_t value) override;

private:
    bool locate_state_memory(
        uint8_t* pointer, uint8_t& region, uint32_t& offset) const override;
    uint8_t* restore_state_memory(uint8_t region, uint32_t offset) override;
    std::vector<uint8_t> work_ram_;
};

} // namespace ear6::nes
