#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include <cstdio>
#include <cstdlib>
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
    kb_row_ = 0;
    kb_column_ = 0;
    kb_enabled_ = false;
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
    if (port2_zapper_enabled_ && addr == 0x4017) {
        uint8_t value = console_->get_memory_manager()->get_open_bus(get_open_bus_mask(1));
        value |= 0x09;
        if (std::getenv("EAR6_TRACE_4017") != nullptr) {
            fprintf(stderr, "[EAR6_4017] zapper=1 val=%02X\n", value);
        }
        return value;
    }

    uint8_t port = (uint8_t)(addr & 1);
    if (port > 1) port = 0;
    uint8_t result = console_->get_memory_manager()->get_open_bus(get_open_bus_mask(port));
    uint8_t serial_bit = 0;
    if (port == 1 && cli_exp_bit3_mode_) {
        serial_bit = 0;
    }
    if (strobe_) {
        if (!(port == 1 && cli_exp_bit3_mode_)) {
            serial_bit = controller_state_[port] & 1;
        }
    } else if (controller_read_pos_[port] < 8) {
        if (!(port == 1 && cli_exp_bit3_mode_)) {
            serial_bit = (controller_state_[port] >> controller_read_pos_[port]) & 1;
            controller_read_pos_[port]++;
        }
    } else {
        // Standard NES controller keeps returning 1 after 8 serial bits.
        serial_bit = (port == 1 && cli_exp_bit3_mode_) ? 0 : 1;
    }
    result |= serial_bit;
    if (addr == 0x4017) {
        if (cli_exp_bit3_mode_) {
            result |= 0x08;
        }
        if (kb_enabled_) {
            result |= ((~get_active_keys(kb_row_, kb_column_)) << 1) & 0x1E;
        }
    }
    if (std::getenv("EAR6_TRACE_4017") != nullptr && addr == 0x4017) {
        fprintf(stderr, "[EAR6_4017] zapper=0 val=%02X serial=%d pos=%d\n", result, serial_bit, controller_read_pos_[1]);
    }
    return result;
}

void NesControlManager::write_ram(uint16_t addr, uint8_t value) {
    write_addr_ = addr;
    write_value_ = value;
    write_pending_ = (console_->get_cpu()->get_cycle_count() & 0x01) ? 1 : 2;
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

    // FamilyBasicKeyboard: $4016 write
    // Bit 0: reset row to 0
    // Bit 1: column select; rising edge increments row
    // Bit 2: enable
    uint8_t prev_column = kb_column_;
    kb_column_ = (value & 0x02) >> 1;
    if (!kb_column_ && prev_column) {
        kb_row_ = (kb_row_ + 1) % 10;
    }
    if (value & 0x01) {
        kb_row_ = 0;
    }
    kb_enabled_ = (value & 0x04) != 0;
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
    return 0xE0;
}

uint8_t NesControlManager::get_active_keys(uint8_t row, uint8_t column) {
    (void)row;
    (void)column;
    return 0; // No keys pressed by default
}

} // namespace ear6::nes
