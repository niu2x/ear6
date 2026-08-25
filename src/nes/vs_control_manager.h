#pragma once

#include "nes_control_manager.h"
#include "nes_types.h"

namespace ear6::nes {

class NesConsole;

class VsControlManager : public NesControlManager {
public:
    VsControlManager(NesConsole* console, const RomInfo& info);

    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;
    uint8_t get_open_bus_mask(uint8_t port) override;
    uint8_t get_prg_chr_select_bit() const { return prg_chr_select_bit_; }

    void reset(bool soft);
    void serialize(ear6::StateStream& stream) override;

private:
    uint32_t dip_switches_ = 0;
    uint32_t protection_counter_ = 0;
    uint8_t prg_chr_select_bit_ = 0;
};

} // namespace ear6::nes
