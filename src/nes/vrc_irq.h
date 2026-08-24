#pragma once

#include <cstdint>

namespace ear6::nes {

class NesConsole;

class VrcIrq {
public:
    void initialize(NesConsole* console) { console_ = console; }
    void reset();
    void process_cpu_clock();
    void set_reload_value(uint8_t value) { reload_value_ = value; }
    void set_reload_value_nibble(uint8_t value, bool high_bits);
    void set_control_value(uint8_t value);
    void acknowledge_irq();

private:
    NesConsole* console_ = nullptr;
    uint8_t reload_value_ = 0;
    uint8_t counter_ = 0;
    int16_t prescaler_counter_ = 0;
    bool enabled_ = false;
    bool enabled_after_ack_ = false;
    bool cycle_mode_ = false;
};

} // namespace ear6::nes
