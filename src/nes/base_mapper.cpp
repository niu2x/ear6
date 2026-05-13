#include "nes_console.h"
#include "nes_memory_manager.h"
#include <cstring>
#include "base_mapper.h"

namespace ear6::nes {

BaseMapper::BaseMapper() {
    nametable_ram_ = new uint8_t[NAMETABLE_SIZE * 4];
    memset(nametable_ram_, 0, NAMETABLE_SIZE * 4);
    nt_ram_size_ = NAMETABLE_SIZE * 4;
}

BaseMapper::~BaseMapper() {
    delete[] nametable_ram_;
}

void BaseMapper::initialize(NesConsole* console) {
    console_ = console;
}

void BaseMapper::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::Read, 0x4020, 0xFFFF);
    ranges.add_handler(MemoryOperation::Write, 0x4020, 0xFFFF);
    ranges.add_handler(MemoryOperation::Read, 0x6000, 0x7FFF);
    ranges.add_handler(MemoryOperation::Write, 0x6000, 0x7FFF);
}

uint8_t BaseMapper::read_ram(uint16_t addr) {
    if (allow_register_read() && is_read_register_addr_[addr]) {
        return read_register(addr);
    }
    if (prg_memory_access_[addr >> 8] & Read) {
        return prg_pages_[addr >> 8][(uint8_t)addr];
    }
    return console_->get_memory_manager()->get_open_bus();
}

void BaseMapper::write_ram(uint16_t addr, uint8_t value) {
    if (is_write_register_addr_[addr]) {
        if (has_bus_conflicts_) value &= prg_pages_[addr >> 8][(uint8_t)addr];
        write_register(addr, value);
    }
}

void BaseMapper::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    (void)value;
}

void BaseMapper::add_register_range(uint16_t start, uint16_t end, MemoryOperation operation) {
    for (uint16_t i = start; i <= end; i++) {
        if (operation == MemoryOperation::Read || operation == MemoryOperation::Any)
            is_read_register_addr_[i] = true;
        if (operation == MemoryOperation::Write || operation == MemoryOperation::Any)
            is_write_register_addr_[i] = true;
    }
}

void BaseMapper::set_cpu_memory_mapping(uint16_t start, uint16_t end, uint8_t* source,
                                             uint32_t source_offset, uint32_t source_size,
                                             int8_t access_type) {
    start >>= 8;
    end >>= 8;
    for (uint16_t i = start; i <= end; i++) {
        prg_pages_[i] = source + source_offset;
        prg_memory_access_[i] = (access_type < 0) ? ReadWrite : static_cast<MemoryAccessType>(access_type);
        source_offset += 0x100;
    }
    (void)source_size;
}

void BaseMapper::set_cpu_memory_mapping(uint16_t start, uint16_t end, int16_t page_number,
                                         PrgMemoryType type, int8_t access_type) {
    (void)type;
    uint32_t page_size = get_prg_page_size();
    uint8_t* source = prg_rom_.data();
    uint32_t source_offset = (uint32_t)page_number * page_size;
    set_cpu_memory_mapping(start, end, source, source_offset, page_size, access_type);
}

void BaseMapper::select_prg_page(uint16_t slot, uint16_t page, PrgMemoryType type) {
    uint16_t page_size = get_prg_page_size();
    uint16_t start = 0x8000 + slot * page_size;
    uint16_t end = start + page_size - 1;
    set_cpu_memory_mapping(start, end, page, type);
}

void BaseMapper::select_prg_page_2x(uint16_t slot, uint16_t page, PrgMemoryType type) {
    select_prg_page(slot, page, type);
    if (get_prg_page_size() < 0x2000) {
        select_prg_page(slot + 1, page + 1, type);
    }
}

void BaseMapper::select_prg_page_4x(uint16_t slot, uint16_t page, PrgMemoryType type) {
    select_prg_page_2x(slot, page, type);
    select_prg_page_2x(slot + 2, page + 2, type);
}

void BaseMapper::set_ppu_memory_mapping(uint16_t start, uint16_t end, uint16_t page_number,
                                         ChrMemoryType type, int8_t access_type) {
    (void)type;
    uint32_t page_size = get_chr_page_size();
    uint8_t* source = chr_rom_.data();
    uint32_t source_offset = (uint32_t)page_number * page_size;
    set_ppu_memory_mapping(start, end, source, source_offset, page_size, access_type);
}

void BaseMapper::set_ppu_memory_mapping(uint16_t start, uint16_t end, uint8_t* source,
                                         uint32_t source_offset, uint32_t source_size,
                                         int8_t access_type) {
    (void)source_size;
    start >>= 8;
    end >>= 8;
    for (uint16_t i = start; i <= end; i++) {
        chr_pages_[i] = source + source_offset;
        chr_memory_access_[i] = (access_type < 0) ? ReadWrite : static_cast<MemoryAccessType>(access_type);
        chr_memory_type_[i] = ChrMemoryType::ChrRom;
        source_offset += 0x100;
    }
}

void BaseMapper::select_chr_page(uint16_t slot, uint16_t page, ChrMemoryType type) {
    uint16_t page_size = get_chr_page_size();
    uint16_t start = slot * page_size;
    uint16_t end = start + page_size - 1;
    set_ppu_memory_mapping(start, end, page, type);
}

void BaseMapper::select_chr_page_2x(uint16_t slot, uint16_t page, ChrMemoryType type) {
    select_chr_page(slot, page, type);
    if (get_chr_page_size() < 0x800) {
        select_chr_page(slot + 1, page + 1, type);
    }
}

void BaseMapper::select_chr_page_4x(uint16_t slot, uint16_t page, ChrMemoryType type) {
    select_chr_page_2x(slot, page, type);
    select_chr_page_2x(slot + 2, page + 2, type);
}

void BaseMapper::select_chr_page_8x(uint16_t slot, uint16_t page, ChrMemoryType type) {
    select_chr_page_4x(slot, page, type);
    select_chr_page_4x(slot + 4, page + 4, type);
}

uint8_t BaseMapper::read_vram(uint16_t addr) {
    addr &= 0x3FFF;
    if (chr_memory_access_[addr >> 8] & Read) {
        return chr_pages_[addr >> 8][(uint8_t)addr];
    }
    return addr;
}

void BaseMapper::write_vram(uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;
    if (chr_memory_access_[addr >> 8] & Write) {
        chr_pages_[addr >> 8][(uint8_t)addr] = value;
    }
}

uint8_t* BaseMapper::get_nametable(uint8_t index) {
    if (index < 4) {
        return nametable_ram_ + index * NAMETABLE_SIZE;
    }
    return nametable_ram_;
}

void BaseMapper::set_nametable(uint8_t index, uint8_t nametable_index) {
    set_ppu_memory_mapping(0x2000 + index * 0x400, 0x2000 + index * 0x400 + 0x3FF,
                               get_nametable(nametable_index), 0, NAMETABLE_SIZE);
    set_ppu_memory_mapping(0x3000 + index * 0x400, 0x3000 + index * 0x400 + 0x3FF,
                               get_nametable(nametable_index), 0, NAMETABLE_SIZE);
}

void BaseMapper::set_mirroring_type(MirroringType type) {
    mirroring_type_ = type;
    switch (type) {
        case MirroringType::Vertical:
            set_nametable(0, 0);
            set_nametable(1, 1);
            set_nametable(2, 0);
            set_nametable(3, 1);
            break;
        case MirroringType::Horizontal:
            set_nametable(0, 0);
            set_nametable(1, 0);
            set_nametable(2, 1);
            set_nametable(3, 1);
            break;
        case MirroringType::ScreenAOnly:
            set_nametable(0, 0);
            set_nametable(1, 0);
            set_nametable(2, 0);
            set_nametable(3, 0);
            break;
        case MirroringType::ScreenBOnly:
            set_nametable(0, 1);
            set_nametable(1, 1);
            set_nametable(2, 1);
            set_nametable(3, 1);
            break;
        case MirroringType::FourScreens:
            set_nametable(0, 0);
            set_nametable(1, 1);
            set_nametable(2, 2);
            set_nametable(3, 3);
            break;
    }
}

} // namespace ear6::nes
