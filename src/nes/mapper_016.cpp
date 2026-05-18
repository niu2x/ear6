#include "mapper_016.h"
#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include "eeprom_24c0x.h"
#include "datach_barcode_reader.h"

namespace ear6::nes {

void Mapper016::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    submapper_ = (uint8_t)info.submapper_id;
    int mapper_id = info.mapper_number;

    for (int i = 0; i < 8; i++) chr_regs_[i] = 0;
    irq_enabled_ = false;
    irq_counter_ = 0;
    irq_reload_ = 0;
    prg_page_ = 0;
    prg_bank_select_ = 0;

    if (mapper_id == 153) {
        add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
        sram_.resize(0x2000, 0);
        // No register reads at $6000 - mapped as SRAM
    } else if (mapper_id == 157) {
        add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
        add_register_range(0x6000, 0x7FFF, MemoryOperation::READ);
        barcode_reader_.reset(new DatachBarcodeReader());
        standard_eeprom_.reset(new Eeprom24C02());
    } else if (mapper_id == 159) {
        add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
        add_register_range(0x6000, 0x7FFF, MemoryOperation::READ);
        standard_eeprom_.reset(new Eeprom24C01());
    } else {
        // mapper 16
        if (submapper_ == 4) {
            add_register_range(0x6000, 0x7FFF, MemoryOperation::WRITE);
        } else {
            add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
            if (submapper_ == 0 || submapper_ == 5) {
                standard_eeprom_.reset(new Eeprom24C02());
            }
        }
        add_register_range(0x6000, 0x7FFF, MemoryOperation::READ);
    }

    select_prg_page(1, 0x0F);
}

void Mapper016::reset(bool soft_reset) {
    (void)soft_reset;
    int mapper_id = rom_info_.mapper_number;

    for (int i = 0; i < 8; i++) chr_regs_[i] = 0;
    irq_enabled_ = false;
    irq_counter_ = 0;
    irq_reload_ = 0;
    prg_page_ = 0;
    prg_bank_select_ = 0;

    if (mapper_id == 153) {
        memset(sram_.data(), 0, sram_.size());
    }

    select_prg_page(1, 0x0F);
}

void Mapper016::process_cpu_clock() {
    if (irq_enabled_) {
        if (irq_counter_ == 0) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
        irq_counter_--;
    }
}

void Mapper016::update_prg() {
    bool use_prg_bank_select = false;
    int mapper_id = rom_info_.mapper_number;
    if (mapper_id == 153) {
        use_prg_bank_select = true;
    } else if (mapper_id == 16 && prg_size_ >= 0x80000) {
        use_prg_bank_select = true;
    }

    if (use_prg_bank_select) {
        select_prg_page(0, prg_page_ | prg_bank_select_);
        select_prg_page(1, 0x0F | prg_bank_select_);
    } else {
        select_prg_page(0, prg_page_);
        select_prg_page(1, 0x0F);
    }
}

void Mapper016::write_register(uint16_t addr, uint8_t value) {
    int mapper_id = rom_info_.mapper_number;
    uint8_t reg = addr & 0x000F;

    switch (reg) {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x04: case 0x05: case 0x06: case 0x07: {
            uint8_t slot = reg;
            chr_regs_[slot] = value;

            if (mapper_id == 153 || prg_size_ >= 0x80000) {
                prg_bank_select_ = 0;
                for (int i = 0; i < 8; i++) {
                    prg_bank_select_ |= (chr_regs_[i] & 0x01) << 4;
                }
                update_prg();
            } else if (!has_chr_ram() && mapper_id != 157) {
                select_chr_page(slot, value);
            }

            if (extra_eeprom_ && mapper_id == 157 && reg <= 3) {
                extra_eeprom_->write_scl((value >> 3) & 0x01);
            }
            break;
        }

        case 0x08:
            prg_page_ = value & 0x0F;
            update_prg();
            break;

        case 0x09:
            switch (value & 0x03) {
                case 0: set_mirroring_type(MirroringType::VERTICAL); break;
                case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
                case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
            }
            break;

        case 0x0A:
            irq_enabled_ = (value & 0x01) != 0;
            if (mapper_id != 16 || submapper_ != 4) {
                irq_counter_ = irq_reload_;
            }
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;

        case 0x0B:
            if (mapper_id != 16 || submapper_ != 4) {
                irq_reload_ = (irq_reload_ & 0xFF00) | value;
            } else {
                irq_counter_ = (irq_counter_ & 0xFF00) | value;
            }
            break;

        case 0x0C:
            if (mapper_id != 16 || submapper_ != 4) {
                irq_reload_ = (irq_reload_ & 0x00FF) | ((uint16_t)value << 8);
            } else {
                irq_counter_ = (irq_counter_ & 0x00FF) | ((uint16_t)value << 8);
            }
            break;

        case 0x0D:
            if (mapper_id == 153) {
                int8_t access = (value & 0x20) ? READ_WRITE : NO_ACCESS;
                set_cpu_memory_mapping(0x6000, 0x7FFF, sram_.data(), 0, 0x2000, access);
            } else {
                uint8_t scl = (value >> 5) & 0x01;
                uint8_t sda = (value >> 6) & 0x01;
                if (standard_eeprom_) {
                    standard_eeprom_->write(scl, sda);
                }
                if (extra_eeprom_) {
                    extra_eeprom_->write_sda(sda);
                }
            }
            break;
    }
}

uint8_t Mapper016::read_register(uint16_t addr) {
    (void)addr;
    uint8_t output = 0;

    if (barcode_reader_) {
        output |= barcode_reader_->get_output();
    }

    if (extra_eeprom_ && standard_eeprom_) {
        output |= (standard_eeprom_->read() && extra_eeprom_->read()) << 4;
    } else if (standard_eeprom_) {
        output |= (standard_eeprom_->read() << 4);
    }

    return output | console_->get_memory_manager()->get_open_bus(0xE7);
}

} // namespace ear6::nes
