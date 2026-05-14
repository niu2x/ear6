#pragma once

#include "nes_types.h"
#include <cstdint>
#include <vector>

namespace ear6::nes {

class MemoryRanges {
private:
    std::vector<uint16_t> ram_read_addresses_;
    std::vector<uint16_t> ram_write_addresses_;
    bool allow_override_ = false;

public:
    std::vector<uint16_t>* get_read_addresses() { return &ram_read_addresses_; }
    std::vector<uint16_t>* get_write_addresses() { return &ram_write_addresses_; }
    bool get_allow_override() const { return allow_override_; }
    void set_allow_override() { allow_override_ = true; }

    void add_handler(MemoryOperation operation, uint16_t start, uint16_t end = 0) {
        if (end == 0) end = start;
        if (operation == MemoryOperation::READ || operation == MemoryOperation::ANY) {
            for (uint32_t i = start; i <= end; i++) {
                ram_read_addresses_.push_back((uint16_t)i);
            }
        }
        if (operation == MemoryOperation::WRITE || operation == MemoryOperation::ANY) {
            for (uint32_t i = start; i <= end; i++) {
                ram_write_addresses_.push_back((uint16_t)i);
            }
        }
    }
};

class INesMemoryHandler {
public:
    virtual ~INesMemoryHandler() = default;
    virtual void get_memory_ranges(MemoryRanges& ranges) = 0;
    virtual uint8_t read_ram(uint16_t addr) = 0;
    virtual void write_ram(uint16_t addr, uint8_t value) = 0;
};

class OpenBusHandler : public INesMemoryHandler {
private:
    uint8_t external_open_bus_ = 0;
    uint8_t internal_open_bus_ = 0;
public:
    void get_memory_ranges(MemoryRanges& ranges) override { (void)ranges; }
    uint8_t read_ram(uint16_t addr) override { (void)addr; return external_open_bus_; }
    void write_ram(uint16_t addr, uint8_t value) override { (void)addr; (void)value; }
    uint8_t get_open_bus() const { return external_open_bus_; }
    uint8_t get_internal_open_bus() const { return external_open_bus_; }
    void set_open_bus(uint8_t value, bool internal_only) {
        if (!internal_only) external_open_bus_ = value;
        internal_open_bus_ = value;
    }
};

template<size_t Mask>
class InternalRamHandler : public INesMemoryHandler {
private:
    uint8_t* internal_ram_ = nullptr;
public:
    void set_internal_ram(uint8_t* ram) { internal_ram_ = ram; }
    void get_memory_ranges(MemoryRanges& ranges) override {
        ranges.set_allow_override();
        ranges.add_handler(MemoryOperation::ANY, 0, 0x1FFF);
    }
    uint8_t read_ram(uint16_t addr) override { return internal_ram_[addr & Mask]; }
    void write_ram(uint16_t addr, uint8_t value) override { internal_ram_[addr & Mask] = value; }
};

} // namespace ear6::nes
