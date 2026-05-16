#include "nes_console.h"
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
    uint8_t result = 0;
    if (controller_read_pos_[port] < 8) {
        result = (controller_state_[port] >> controller_read_pos_[port]) & 1;
        controller_read_pos_[port]++;
    }
    result |= 0x40;

    // VS UniSystem: merge DIP switch bits into controller reads
    if (is_vs_system_) {
        if (addr == 0x4016) {
            result = (result & 0x65) | 0x00;  // DIP bits 0-1 → bit 3-4, default 0
        } else if (addr == 0x4017) {
            result = 0x00;  // DIP bits 2-7 → bit 2-7, default 0
        }
    }

    return result;
}

void NesControlManager::write_ram(uint16_t addr, uint8_t value) {
    (void)addr;
    write_addr_ = addr;
    write_value_ = value;
    write_pending_ = 2;
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
    write_pending_--;
}

void NesControlManager::update_control_devices() {}
void NesControlManager::update_input_state() {}
uint8_t NesControlManager::get_open_bus_mask(uint8_t port) {
    (void)port;
    return 0x1F;
}

} // namespace ear6::nes
