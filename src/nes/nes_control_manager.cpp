#include "nes_console.h"
#include "nes_memory_manager.h"
#include "nes_control_manager.h"

namespace ear6::nes {

void NesControlManager::reset(bool soft) {
    (void)soft;
    write_pending_ = 0;
    for (int i = 0; i < 2; i++) {
        controller_state_[i] = 0;
        controller_read_pos_[i] = 0;
    }
    strobe_ = false;
}

void NesControlManager::set_button_state(int port, int button, bool pressed) {
    if (port >= 0 && port < 2) {
        if (pressed) {
            controller_state_[port] |= (1 << button);
        } else {
            controller_state_[port] &= ~(1 << button);
        }
    }
}

void NesControlManager::clear_all() {
    controller_state_[0] = 0;
    controller_state_[1] = 0;
}

uint8_t NesControlManager::read_ram(uint16_t addr) {
    uint8_t port = (uint8_t)(addr & 1);
    if (port > 1) port = 0;
    uint8_t result = console_->get_memory_manager()->get_open_bus(get_open_bus_mask(port));
    uint8_t serial_bit = 0;
    if (strobe_) {
        serial_bit = controller_state_[port] & 1;
    } else if (controller_read_pos_[port] < 8) {
        serial_bit = (controller_state_[port] >> controller_read_pos_[port]) & 1;
        controller_read_pos_[port]++;
    } else {
        // Standard NES controller keeps returning 1 after 8 serial bits.
        serial_bit = 1;
    }
    result |= serial_bit;
    return result;
}

void NesControlManager::write_ram(uint16_t addr, uint8_t value) {
    write_addr_ = addr;
    write_value_ = value;
    write_pending_ = 2;
}

void NesControlManager::apply_write(uint8_t value) {
    bool new_strobe = (value & 1) != 0;
    if (strobe_ && !new_strobe) {
        controller_read_pos_[0] = 0;
        controller_read_pos_[1] = 0;
    }
    strobe_ = new_strobe;
    if (strobe_) {
        controller_read_pos_[0] = 0;
        controller_read_pos_[1] = 0;
    }
}

void NesControlManager::process_writes() {
    if (write_pending_ == 0) {
        return;
    }
    write_pending_--;
    if (write_pending_ == 0 && write_addr_ == 0x4016) {
        apply_write(write_value_);
    }
}

void NesControlManager::update_control_devices() {}
void NesControlManager::update_input_state() {}

uint8_t NesControlManager::get_open_bus_mask(uint8_t port) {
    (void)port;
    return 0x00;
}

} // namespace ear6::nes
