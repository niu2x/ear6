#include "nes_console.h"
#include "vs_control_manager.h"

namespace ear6::nes {

VsControlManager::VsControlManager(NesConsole* console, const RomInfo& info)
    : NesControlManager(console) {
    (void)info;
    dip_switches_ = 0;
    protection_counter_ = 0;
}

void VsControlManager::get_memory_ranges(MemoryRanges& ranges) {
    NesControlManager::get_memory_ranges(ranges);
    ranges.add_handler(MemoryOperation::READ, 0x4020, 0x5FFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x4020, 0x5FFF);
}

uint8_t VsControlManager::read_ram(uint16_t addr) {
    switch (addr) {
        case 0x4016: {
            uint8_t value = NesControlManager::read_ram(addr) & 0x65;
            value |= (dip_switches_ & 0x01) ? 0x08 : 0x00;
            value |= (dip_switches_ & 0x02) ? 0x10 : 0x00;
            return value;
        }
        case 0x4017: {
            uint8_t value = NesControlManager::read_ram(addr) & 0x01;
            value |= (dip_switches_ & 0x04) ? 0x04 : 0x00;
            value |= (dip_switches_ & 0x08) ? 0x08 : 0x00;
            value |= (dip_switches_ & 0x10) ? 0x10 : 0x00;
            value |= (dip_switches_ & 0x20) ? 0x20 : 0x00;
            value |= (dip_switches_ & 0x40) ? 0x40 : 0x00;
            value |= (dip_switches_ & 0x80) ? 0x80 : 0x00;
            return value;
        }
        default:
            return NesControlManager::read_ram(addr);
    }
}

void VsControlManager::write_ram(uint16_t addr, uint8_t value) {
    NesControlManager::write_ram(addr, value);
    if (addr == 0x4016) {
        // Bit 2 selects PRG/CHR bank pair (for VsSystem mapper)
        // Bit 1 is main/sub bit for dual-system
    }
}

void VsControlManager::reset(bool soft) {
    NesControlManager::reset(soft);
    protection_counter_ = 0;
}

uint8_t VsControlManager::get_open_bus_mask(uint8_t port) {
    (void)port;
    return 0x00;
}

} // namespace ear6::nes
