#include "nes_apu.h"

namespace ear6::nes {

void NesApu::reset(bool soft_reset) {
    (void)soft_reset;
    frame_cycle_counter_ = 0;
    cycle_counter_ = 0;
    for (int i = 0; i < 4; i++) {
        sq1_regs_[i] = 0;
        sq2_regs_[i] = 0;
        dmc_regs_[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        tri_regs_[i] = 0;
        noise_regs_[i] = 0;
    }
    status_ = 0;
    frame_counter_ = 0;
    buffer_.clear();
}

uint8_t NesApu::read_ram(uint16_t addr) {
    if (addr == 0x4015) {
        uint8_t s = status_ & 0x1F;
        s |= 0x20; // Frame counter bit
        return s;
    }
    return 0;
}

void NesApu::write_ram(uint16_t addr, uint8_t value) {
    if (addr >= 0x4000 && addr <= 0x4003) {
        sq1_regs_[addr & 0x03] = value;
    } else if (addr >= 0x4004 && addr <= 0x4007) {
        sq2_regs_[addr & 0x03] = value;
    } else if (addr >= 0x4008 && addr <= 0x400A) {
        tri_regs_[addr & 0x03] = value;
    } else if (addr >= 0x400C && addr <= 0x400E) {
        noise_regs_[addr & 0x03] = (addr == 0x400E) ? value : value;
    } else if (addr == 0x400F) {
        (void)addr; (void)value;
    } else if (addr >= 0x4010 && addr <= 0x4013) {
        dmc_regs_[addr & 0x03] = value;
    } else if (addr == 0x4015) {
        status_ = value;
    } else if (addr == 0x4017) {
        frame_counter_ = value;
    }
}

void NesApu::run(int ppu_cycles) {
    (void)ppu_cycles;
    // Simplified: accumulate dummy samples at 48000 Hz
    (void)apu_active_;
}

void NesApu::process_cpu_clock() {
    // Pass - will eventually implement proper APU
}

void NesApu::end_frame() {
    // Add a dummy sample for now
    int16_t sample = 0; // silence
    buffer_.push_back(sample);
    buffer_.push_back(sample);
}

} // namespace ear6::nes
