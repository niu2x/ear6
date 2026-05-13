#pragma once

#include "ines_memory_handler.h"
#include <cstdint>
#include <vector>

namespace ear6::nes {

class NesConsole;

class NesApu : public INesMemoryHandler {
public:
    NesApu(NesConsole* console) : console_(console) {
        (void)console_;
        buffer_.reserve(8192);
    }

    void get_memory_ranges(MemoryRanges& ranges) override {
        ranges.add_handler(MemoryOperation::Read, 0x4000, 0x4013);
        ranges.add_handler(MemoryOperation::Read, 0x4015);
        ranges.add_handler(MemoryOperation::Write, 0x4000, 0x4015);
        ranges.add_handler(MemoryOperation::Write, 0x4017);
    }

    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;

    void reset(bool soft_reset);
    void run(int ppu_cycles);
    void end_frame();
    void process_cpu_clock();

    void set_apu_status(bool active) { apu_active_ = active; }

    const int16_t* get_buffer() const { return buffer_.data(); }
    int get_samples() const { return (int)buffer_.size(); }

    uint16_t get_dmc_read_address() { return 0x8000; }

private:
    NesConsole* console_ = nullptr;

    bool apu_active_ = true;
    uint64_t frame_cycle_counter_ = 0;
    uint64_t cycle_counter_ = 0;

    uint8_t sq1_regs_[4] = {};
    uint8_t sq2_regs_[4] = {};
    uint8_t tri_regs_[3] = {};
    uint8_t noise_regs_[3] = {};
    uint8_t dmc_regs_[4] = {};
    uint8_t status_ = 0;
    uint8_t frame_counter_ = 0;

    std::vector<int16_t> buffer_;
};

} // namespace ear6::nes
