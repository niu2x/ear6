#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include <cstring>
#include "mapper_001.h"

namespace ear6::nes {

void Mapper001::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    // Allocate 8KB WRAM
    work_ram_.resize(0x2000, 0);

    // Register $8000-$FFFF for MMC1 register writes
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    // Initialize registers with power-on defaults:
    // bit 2,3 of $8000 set: 16KB PRG mode, $8000 swappable
    process_register_write(0x8000, 0xFF | 0x0C);
    process_register_write(0xA000, 0xFF);
    process_register_write(0xC000, 0xFF);
    process_register_write(0xE000, 0x00);

    last_chr_reg_ = 0xA000;

    update_state();
}

void Mapper001::reset_buffer() {
    shift_count_ = 0;
    write_buffer_ = 0;
}

void Mapper001::process_bit_write(uint16_t addr, uint8_t value) {
    if (value & 0x80) {
        reset_buffer();
        prg_mode_ = true;
        slot_select_ = true;
        update_state();
    } else {
        write_buffer_ >>= 1;
        write_buffer_ |= ((value << 4) & 0x10);
        shift_count_++;

        if (shift_count_ == 5) {
            process_register_write(addr, write_buffer_);
            update_state();
            reset_buffer();
        }
    }
}

void Mapper001::process_register_write(uint16_t addr, uint8_t val) {
    switch (addr & 0xE000) {
        case 0x8000:
            switch (val & 0x03) {
                case 0: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                case 1: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
                case 2: set_mirroring_type(MirroringType::VERTICAL); break;
                case 3: set_mirroring_type(MirroringType::HORIZONTAL); break;
            }
            slot_select_ = (val & 0x04) != 0;
            prg_mode_ = (val & 0x08) != 0;
            chr_mode_ = (val & 0x10) != 0;
            break;

        case 0xA000:
            last_chr_reg_ = addr;
            chr_reg0_ = val & 0x1F;
            break;

        case 0xC000:
            last_chr_reg_ = addr;
            chr_reg1_ = val & 0x1F;
            break;

        case 0xE000:
            prg_reg_ = val & 0x0F;
            wram_disable_ = (val & 0x10) != 0;
            break;
    }
}

void Mapper001::update_state() {
    uint8_t extra_reg = (last_chr_reg_ == 0xC000 && chr_mode_) ? chr_reg1_ : chr_reg0_;
    uint8_t prg_bank_select = 0;

    if (prg_size_ == 0x80000) {
        // SUROM: 512KB carts — CHR reg bit 4 selects upper 256KB PRG bank
        // NOTE: full SUROM (MMC1B3 variant) needs 8-bit CHR register storage
        prg_bank_select = extra_reg & 0x10;
    }

    // WRAM at $6000-$7FFF
    bool wram_enabled = !wram_disable_;
    MemoryAccessType wram_access = wram_enabled ? READ_WRITE : NO_ACCESS;

    // Map WRAM if we have battery or work RAM is accessible
    set_cpu_memory_mapping(0x6000, 0x7FFF, work_ram_.data(), 0, 0x2000, wram_access);
    if (!wram_enabled) {
        // Clear the mapping so reads fall through to open bus
        for (uint16_t i = 0x60; i <= 0x7F; i++) {
            prg_pages_[i] = nullptr;
        }
    }

    // PRG banking
    if (prg_mode_) {
        if (slot_select_) {
            select_prg_page(0, prg_reg_ | prg_bank_select);
            select_prg_page(1, 0x0F | prg_bank_select);
        } else {
            select_prg_page(0, 0 | prg_bank_select);
            select_prg_page(1, prg_reg_ | prg_bank_select);
        }
    } else {
        select_prg_page(0, (prg_reg_ & 0xFE) | prg_bank_select);
        select_prg_page(1, ((prg_reg_ & 0xFE) | prg_bank_select) + 1);
    }

    // CHR banking
    if (chr_mode_) {
        select_chr_page(0, chr_reg0_);
        select_chr_page(1, chr_reg1_);
    } else {
        select_chr_page(0, chr_reg0_ & 0x1E);
        select_chr_page(1, (chr_reg0_ & 0x1E) + 1);
    }
}

void Mapper001::write_register(uint16_t addr, uint8_t value) {
    uint64_t current_cycle = console_->get_cpu()->get_cycle_count();

    if ((value & 0x80) || current_cycle - last_write_cycle_ >= 1) {
        process_bit_write(addr, value);
    }
    last_write_cycle_ = current_cycle;
}

} // namespace ear6::nes
