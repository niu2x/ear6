#pragma once

#include "ines_memory_handler.h"
#include <cstdint>

namespace ear6::nes {

class NesConsole;

class NesControlManager : public INesMemoryHandler {
public:
    NesControlManager(NesConsole* console) : console_(console) {
        (void)console_;
    }

    void get_memory_ranges(MemoryRanges& ranges) override {
        ranges.add_handler(MemoryOperation::READ, 0x4016, 0x4017);
        ranges.add_handler(MemoryOperation::WRITE, 0x4016);
    }

    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;

    void reset(bool soft);
    void update_control_devices();
    void update_input_state();
    bool has_pending_writes() const { return write_pending_ > 0; }
    void process_writes();

    virtual uint8_t get_open_bus_mask(uint8_t port);

    void set_button_state(int port, int button, bool pressed);
    void clear_all();

protected:
    NesConsole* console_ = nullptr;
    uint8_t write_pending_ = 0;
    uint16_t write_addr_ = 0;
    uint8_t write_value_ = 0;
    uint8_t controller_state_[2] = {};
    int controller_read_pos_[2] = {};
    bool strobe_ = false;
};

} // namespace ear6::nes
