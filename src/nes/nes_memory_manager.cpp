#include "nes_console.h"
#include "base_mapper.h"
#include <cstring>
#include "nes_memory_manager.h"

namespace ear6::nes {

NesMemoryManager::NesMemoryManager(NesConsole* console, BaseMapper* mapper) {
    console_ = console;
    mapper_ = mapper;

    internal_ram_size_ = 0x800;
    internal_ram_ = new uint8_t[internal_ram_size_];
    memset(internal_ram_, 0, internal_ram_size_);

    internal_ram_handler_.reset(new InternalRamHandler<0x7FF>());
    ((InternalRamHandler<0x7FF>*)internal_ram_handler_.get())->set_internal_ram(internal_ram_);

    ram_read_handlers_ = new INesMemoryHandler*[CPU_MEMORY_SIZE];
    ram_write_handlers_ = new INesMemoryHandler*[CPU_MEMORY_SIZE];

    for (int i = 0; i < CPU_MEMORY_SIZE; i++) {
        ram_read_handlers_[i] = &open_bus_handler_;
        ram_write_handlers_[i] = &open_bus_handler_;
    }

    register_io_device(internal_ram_handler_.get());
}

NesMemoryManager::~NesMemoryManager() {
    delete[] internal_ram_;
    delete[] ram_read_handlers_;
    delete[] ram_write_handlers_;
}

void NesMemoryManager::reset(bool soft_reset) {
    if (!soft_reset) {
        memset(internal_ram_, 0, internal_ram_size_);
    }
}

void NesMemoryManager::init_memory_handlers(INesMemoryHandler** handlers, INesMemoryHandler* handler,
                                             std::vector<uint16_t>* addresses, bool allow_override) {
    for (uint16_t addr : *addresses) {
        if (!allow_override && handlers[addr] != &open_bus_handler_ && handlers[addr] != handler) {
            continue;
        }
        handlers[addr] = handler;
    }
}

void NesMemoryManager::register_io_device(INesMemoryHandler* handler) {
    MemoryRanges ranges;
    handler->get_memory_ranges(ranges);

    init_memory_handlers(ram_read_handlers_, handler, ranges.get_read_addresses(), ranges.get_allow_override());
    init_memory_handlers(ram_write_handlers_, handler, ranges.get_write_addresses(), ranges.get_allow_override());
}

void NesMemoryManager::unregister_io_device(INesMemoryHandler* handler) {
    MemoryRanges ranges;
    handler->get_memory_ranges(ranges);

    for (uint16_t addr : *ranges.get_read_addresses()) {
        ram_read_handlers_[addr] = &open_bus_handler_;
    }
    for (uint16_t addr : *ranges.get_write_addresses()) {
        ram_write_handlers_[addr] = &open_bus_handler_;
    }
}

uint8_t NesMemoryManager::read(uint16_t addr) {
    uint8_t value = ram_read_handlers_[addr]->read_ram(addr);
    open_bus_handler_.set_open_bus(value, addr == 0x4015);
    return value;
}

void NesMemoryManager::write(uint16_t addr, uint8_t value) {
    ram_write_handlers_[addr]->write_ram(addr, value);
    open_bus_handler_.set_open_bus(value, false);
}

uint8_t NesMemoryManager::get_open_bus(uint8_t mask) {
    return open_bus_handler_.get_open_bus() & mask;
}

uint8_t* NesMemoryManager::get_internal_ram() {
    return internal_ram_;
}

} // namespace ear6::nes
