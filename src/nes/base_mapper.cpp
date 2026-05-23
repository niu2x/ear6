#include "nes_console.h"
#include "nes_memory_manager.h"
#include <cstring>
#include "base_mapper.h"

namespace ear6::nes {

BaseMapper::BaseMapper() {
    nametable_ram_ = new uint8_t[NAMETABLE_SIZE * 4];
    memset(nametable_ram_, 0, NAMETABLE_SIZE * 4);
    nt_ram_size_ = NAMETABLE_SIZE * 4;

    for (uint16_t i = 0; i < 0x100; i++) {
        prg_pages_[i] = nullptr;
        chr_pages_[i] = nullptr;
        prg_memory_access_[i] = NO_ACCESS;
        chr_memory_access_[i] = NO_ACCESS;
        chr_memory_type_[i] = ChrMemoryType::DEFAULT;
    }
}

BaseMapper::~BaseMapper() {
    delete[] nametable_ram_;
}

void BaseMapper::initialize(NesConsole* console) {
    console_ = console;
}

void BaseMapper::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::READ, 0x4020, 0xFFFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x4020, 0xFFFF);
    ranges.add_handler(MemoryOperation::READ, 0x6000, 0x7FFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x6000, 0x7FFF);
}

uint8_t BaseMapper::read_ram(uint16_t addr) {
    if (allow_register_read() && is_read_register_addr_[addr]) {
        return read_register(addr);
    }
    if (prg_memory_access_[addr >> 8] & READ) {
        return prg_pages_[addr >> 8][(uint8_t)addr];
    }
    return console_->get_memory_manager()->get_open_bus();
}

void BaseMapper::write_ram(uint16_t addr, uint8_t value) {
    if (is_write_register_addr_[addr]) {
        if (has_bus_conflicts_) value &= prg_pages_[addr >> 8][(uint8_t)addr];
        write_register(addr, value);
    } else if (prg_memory_access_[addr >> 8] & WRITE) {
        prg_pages_[addr >> 8][(uint8_t)addr] = value;
    }
}

void BaseMapper::write_register(uint16_t addr, uint8_t value) {
    (void)addr;
    (void)value;
}

void BaseMapper::add_register_range(uint16_t start, uint16_t end, MemoryOperation operation) {
    for (uint32_t i = start; i <= end; i++) {
        if (operation == MemoryOperation::READ || operation == MemoryOperation::ANY)
            is_read_register_addr_[i] = true;
        if (operation == MemoryOperation::WRITE || operation == MemoryOperation::ANY)
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
        prg_memory_access_[i] = (access_type < 0) ? READ_WRITE : static_cast<MemoryAccessType>(access_type);
        source_offset += 0x100;
    }
    (void)source_size;
}

void BaseMapper::set_cpu_memory_mapping(uint16_t start, uint16_t end, int16_t page_number,
                                         PrgMemoryType type, int8_t access_type) {
    (void)type;
    uint32_t page_size = get_prg_page_size();
    uint8_t* source = prg_rom_.data();
    if (prg_size_ > 0) {
        uint32_t max_page = prg_size_ / page_size;
        if (page_number < 0) {
            page_number = (int16_t)(max_page + page_number);
        } else {
            page_number %= (int16_t)max_page;
        }
    }
    uint32_t source_offset = (uint32_t)page_number * page_size;
    set_cpu_memory_mapping(start, end, source, source_offset, page_size, access_type);
}

void BaseMapper::select_prg_page(uint16_t slot, uint16_t page, PrgMemoryType type) {
    uint16_t page_size = get_prg_page_size();
    if (prg_size_ > 0) {
        uint16_t max_page = prg_size_ / page_size;
        page %= max_page;
    }
    uint16_t start = 0x8000 + slot * page_size;
    uint16_t end = start + page_size - 1;
    set_cpu_memory_mapping(start, end, page, type);
}

void BaseMapper::select_prg_page_2x(uint16_t slot, uint16_t page, PrgMemoryType type) {
    select_prg_page(slot, page, type);
    select_prg_page(slot + 1, page + 1, type);
}

void BaseMapper::select_prg_page_4x(uint16_t slot, uint16_t page, PrgMemoryType type) {
    select_prg_page_2x(slot, page, type);
    select_prg_page_2x(slot + 2, page + 2, type);
}

void BaseMapper::initialize_chr_ram(int32_t size) {
    uint32_t default_size = get_chr_ram_size();
    chr_ram_size_ = (size >= 0) ? (uint32_t)size : default_size;
    if (chr_ram_size_ > 0) {
        chr_ram_.resize(chr_ram_size_, 0);
    }
}

void BaseMapper::setup_default_work_ram() {
    if (rom_info_.has_battery && save_ram_size_ > 0) {
        set_cpu_memory_mapping(0x6000, 0x7FFF, save_ram_.data(), 0, save_ram_size_, READ_WRITE);
    } else if (work_ram_size_ > 0) {
        set_cpu_memory_mapping(0x6000, 0x7FFF, work_ram_.data(), 0, work_ram_size_, READ_WRITE);
    }
}

void BaseMapper::apply_trainer_data(const std::vector<uint8_t>& trainer_data) {
    if (trainer_data.size() != 512) return;
    if (work_ram_size_ >= 0x2000) {
        memcpy(work_ram_.data() + 0x1000, trainer_data.data(), 512);
    } else if (save_ram_size_ >= 0x2000) {
        memcpy(save_ram_.data() + 0x1000, trainer_data.data(), 512);
    }
}

void BaseMapper::init_work_ram(const RomInfo& info) {
    if (info.has_battery) {
        save_ram_size_ = get_save_ram_size();
        save_ram_.resize(save_ram_size_, 0);
        has_default_work_ram_ = save_ram_size_ > 0;
    } else {
        work_ram_size_ = get_work_ram_size();
        work_ram_.resize(work_ram_size_, 0);
        has_default_work_ram_ = work_ram_size_ > 0;
    }
    if (info.work_ram_size > 0) {
        work_ram_size_ = info.work_ram_size * 1024;
        work_ram_.resize(work_ram_size_, 0);
        has_default_work_ram_ = true;
    }
}

void BaseMapper::set_ppu_memory_mapping(uint16_t start, uint16_t end, uint16_t page_number,
                                         ChrMemoryType type, int8_t access_type) {
    if (type == ChrMemoryType::DEFAULT) {
        type = (chr_rom_size_ > 0) ? ChrMemoryType::CHR_ROM : ChrMemoryType::CHR_RAM;
    }

    uint32_t page_size;
    uint32_t page_count;
    switch (type) {
        case ChrMemoryType::CHR_ROM:
            page_size = get_chr_page_size();
            page_count = (chr_rom_size_ > 0) ? (chr_rom_size_ / page_size) : 0;
            break;
        case ChrMemoryType::CHR_RAM: {
            if (chr_ram_size_ == 0) {
                initialize_chr_ram();
            }
            page_size = get_chr_ram_page_size();
            page_count = (chr_ram_size_ > 0) ? (chr_ram_size_ / page_size) : 0;
            break;
        }
        default:
            return;
    }

    if (page_count > 0) {
        page_number %= page_count;
    }

    set_ppu_memory_mapping(start, end, type, (uint32_t)page_number * page_size, access_type);
}

void BaseMapper::set_ppu_memory_mapping(uint16_t start, uint16_t end,
                                         ChrMemoryType type, uint32_t source_offset,
                                         int8_t access_type) {
    uint8_t* source = nullptr;
    uint32_t source_size = 0;

    switch (type) {
        case ChrMemoryType::CHR_ROM:
            source = chr_rom_.data();
            source_size = chr_rom_size_;
            break;
        case ChrMemoryType::CHR_RAM:
            source = chr_ram_.data();
            source_size = chr_ram_size_;
            break;
        default:
            break;
    }

    int first_slot = start >> 8;
    int slot_count = ((int)end - (int)start + 1) >> 8;
    for (int i = 0; i < slot_count; i++) {
        int slot = first_slot + i;
        uint32_t offset = source_offset + (uint32_t)i * 0x100;
        if (source && source_size > 0 && offset < source_size) {
            chr_pages_[slot] = source + offset;
            chr_memory_access_[slot] = (access_type < 0) ? READ_WRITE : static_cast<MemoryAccessType>(access_type);
            chr_memory_type_[slot] = type;
        } else {
            chr_pages_[slot] = nullptr;
            chr_memory_access_[slot] = NO_ACCESS;
        }
    }
}

void BaseMapper::set_ppu_memory_mapping(uint16_t start, uint16_t end, uint8_t* source,
                                         uint32_t source_offset, uint32_t source_size,
                                         int8_t access_type) {
    start &= 0x3FFF;
    end &= 0x3FFF;
    start >>= 8;
    end >>= 8;
    for (uint16_t i = start; i <= end; i++) {
        if (source && source_offset < source_size) {
            chr_pages_[i] = source + source_offset;
            chr_memory_access_[i] = (access_type < 0) ? READ_WRITE : static_cast<MemoryAccessType>(access_type);
        } else {
            chr_pages_[i] = nullptr;
            chr_memory_access_[i] = NO_ACCESS;
        }
        source_offset += 0x100;
    }
}

void BaseMapper::select_chr_page(uint16_t slot, uint16_t page, ChrMemoryType type) {
    uint16_t page_size = get_chr_page_size();
    if (chr_rom_size_ > 0) {
        uint16_t max_page = chr_rom_size_ / page_size;
        if (max_page > 0) page %= max_page;
    }
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
    if (chr_memory_access_[addr >> 8] & READ) {
        return chr_pages_[addr >> 8][(uint8_t)addr];
    }
    return addr;
}

void BaseMapper::write_vram(uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;
    if (chr_memory_access_[addr >> 8] & WRITE) {
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
        case MirroringType::VERTICAL:
            set_nametable(0, 0);
            set_nametable(1, 1);
            set_nametable(2, 0);
            set_nametable(3, 1);
            break;
        case MirroringType::HORIZONTAL:
            set_nametable(0, 0);
            set_nametable(1, 0);
            set_nametable(2, 1);
            set_nametable(3, 1);
            break;
        case MirroringType::SCREEN_A_ONLY:
            set_nametable(0, 0);
            set_nametable(1, 0);
            set_nametable(2, 0);
            set_nametable(3, 0);
            break;
        case MirroringType::SCREEN_B_ONLY:
            set_nametable(0, 1);
            set_nametable(1, 1);
            set_nametable(2, 1);
            set_nametable(3, 1);
            break;
        case MirroringType::FOUR_SCREENS:
            set_nametable(0, 0);
            set_nametable(1, 1);
            set_nametable(2, 2);
            set_nametable(3, 3);
            break;
    }
}

} // namespace ear6::nes
