#pragma once

#include "ines_memory_handler.h"
#include <cstdint>
#include <memory>

namespace ear6::nes {

class NesConsole;
class BaseMapper;

class NesMemoryManager {
public:
    static constexpr int CPU_MEMORY_SIZE = 0x10000;

    NesMemoryManager(NesConsole* console, BaseMapper* mapper);
    ~NesMemoryManager();

    void reset(bool soft_reset);
    void register_io_device(INesMemoryHandler* handler);
    void unregister_io_device(INesMemoryHandler* handler);

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);

    uint8_t get_open_bus(uint8_t mask = 0xFF);
    uint8_t* get_internal_ram();
    OpenBusHandler& get_open_bus_handler() { return open_bus_handler_; }

private:
    void init_memory_handlers(INesMemoryHandler** handlers, INesMemoryHandler* handler,
                              std::vector<uint16_t>* addresses, bool allow_override);

    NesConsole* console_ = nullptr;
    BaseMapper* mapper_ = nullptr;

    uint8_t* internal_ram_ = nullptr;
    uint32_t internal_ram_size_ = 0;
    OpenBusHandler open_bus_handler_;
    std::unique_ptr<INesMemoryHandler> internal_ram_handler_;
    INesMemoryHandler** ram_read_handlers_ = nullptr;
    INesMemoryHandler** ram_write_handlers_ = nullptr;
};

} // namespace ear6::nes
