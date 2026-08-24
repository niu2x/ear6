#include "mapper_vrc2_4.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"

namespace ear6::nes {

void MapperVRC2_4::detect_variant(const RomInfo& info) {
    switch (info.mapper_number) {
        case 21:
            variant_ = info.submapper_id == 2 ? Variant::VRC4C : Variant::VRC4A;
            break;
        case 22:
            variant_ = Variant::VRC2A;
            break;
        case 23:
            variant_ = info.submapper_id == 2 ? Variant::VRC4E : Variant::VRC2B;
            break;
        case 25:
            if (info.submapper_id == 2) {
                variant_ = Variant::VRC4D;
            } else if (info.submapper_id == 3) {
                variant_ = Variant::VRC2C;
            } else {
                variant_ = Variant::VRC4B;
            }
            break;
        default:
            variant_ = Variant::VRC2A;
            break;
    }

    use_heuristics_ = info.submapper_id == 0 && info.mapper_number != 22;
}

void MapperVRC2_4::init(const RomInfo& info,
                        const std::vector<uint8_t>& prg_rom,
                        const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    detect_variant(info);
    prg_reg_0_ = 0;
    prg_reg_1_ = 0;
    prg_mode_ = 0;
    use_microwire_ = !use_heuristics_ && variant_ <= Variant::VRC2C
                     && !info.has_battery && info.work_ram_size == 0;
    chr_high_.fill(0);
    chr_low_.fill(0);
    latch_ = 0;

    irq_reload_value_ = 0;
    irq_counter_ = 0;
    irq_prescaler_counter_ = 0;
    irq_enabled_ = false;
    irq_enabled_after_ack_ = false;
    irq_cycle_mode_ = false;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    if (use_microwire_) {
        add_register_range(0x6000, 0x7FFF, MemoryOperation::ANY);
    }
    update_state();
}

void MapperVRC2_4::setup_default_work_ram() {
    if (!use_microwire_) {
        BaseMapper::setup_default_work_ram();
    }
}

uint16_t MapperVRC2_4::translate_address(uint16_t addr) const {
    uint16_t a0 = 0;
    uint16_t a1 = 0;

    if (use_heuristics_) {
        switch (variant_) {
            case Variant::VRC2C:
            case Variant::VRC4B:
            case Variant::VRC4D:
                a0 = ((addr >> 1) & 0x01) | ((addr >> 3) & 0x01);
                a1 = (addr & 0x01) | ((addr >> 2) & 0x01);
                break;
            case Variant::VRC4A:
            case Variant::VRC4C:
                a0 = ((addr >> 1) & 0x01) | ((addr >> 6) & 0x01);
                a1 = ((addr >> 2) & 0x01) | ((addr >> 7) & 0x01);
                break;
            case Variant::VRC2B:
            case Variant::VRC4E:
                a0 = (addr & 0x01) | ((addr >> 2) & 0x01);
                a1 = ((addr >> 1) & 0x01) | ((addr >> 3) & 0x01);
                break;
            default:
                break;
        }
    } else {
        switch (variant_) {
            case Variant::VRC2A:
            case Variant::VRC2C:
            case Variant::VRC4B:
                a0 = (addr >> 1) & 0x01;
                a1 = addr & 0x01;
                break;
            case Variant::VRC4D:
                a0 = (addr >> 3) & 0x01;
                a1 = (addr >> 2) & 0x01;
                break;
            case Variant::VRC4A:
                a0 = (addr >> 1) & 0x01;
                a1 = (addr >> 2) & 0x01;
                break;
            case Variant::VRC4C:
                a0 = (addr >> 6) & 0x01;
                a1 = (addr >> 7) & 0x01;
                break;
            case Variant::VRC2B:
                a0 = addr & 0x01;
                a1 = (addr >> 1) & 0x01;
                break;
            case Variant::VRC4E:
                a0 = (addr >> 2) & 0x01;
                a1 = (addr >> 3) & 0x01;
                break;
        }
    }

    return (addr & 0xFF00) | (a1 << 1) | a0;
}

void MapperVRC2_4::update_state() {
    for (uint8_t slot = 0; slot < 8; slot++) {
        uint16_t page = chr_low_[slot] | (static_cast<uint16_t>(chr_high_[slot]) << 4);
        if (variant_ == Variant::VRC2A) {
            page >>= 1;
        }
        select_chr_page(slot, page);
    }

    if (prg_mode_ == 0) {
        select_prg_page(0, prg_reg_0_);
        select_prg_page(1, prg_reg_1_);
        select_prg_page(2, static_cast<uint16_t>(-2));
    } else {
        select_prg_page(0, static_cast<uint16_t>(-2));
        select_prg_page(1, prg_reg_1_);
        select_prg_page(2, prg_reg_0_);
    }
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void MapperVRC2_4::set_irq_reload_nibble(uint8_t value, bool high_bits) {
    if (high_bits) {
        irq_reload_value_ = (irq_reload_value_ & 0x0F) | ((value & 0x0F) << 4);
    } else {
        irq_reload_value_ = (irq_reload_value_ & 0xF0) | (value & 0x0F);
    }
}

void MapperVRC2_4::set_irq_control(uint8_t value) {
    irq_enabled_after_ack_ = (value & 0x01) != 0;
    irq_enabled_ = (value & 0x02) != 0;
    irq_cycle_mode_ = (value & 0x04) != 0;
    if (irq_enabled_) {
        irq_counter_ = irq_reload_value_;
        irq_prescaler_counter_ = 341;
    }
    console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
}

void MapperVRC2_4::acknowledge_irq() {
    irq_enabled_ = irq_enabled_after_ack_;
    console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
}

void MapperVRC2_4::process_cpu_clock() {
    bool supports_irq = (use_heuristics_ && rom_info_.mapper_number != 22)
                        || variant_ >= Variant::VRC4A;
    if (!supports_irq || !irq_enabled_) {
        return;
    }

    irq_prescaler_counter_ -= 3;
    if (irq_cycle_mode_ || irq_prescaler_counter_ <= 0) {
        if (irq_counter_ == 0xFF) {
            irq_counter_ = irq_reload_value_;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        } else {
            irq_counter_++;
        }
        irq_prescaler_counter_ += 341;
    }
}

uint8_t MapperVRC2_4::read_register(uint16_t addr) {
    (void)addr;
    return latch_ | (console_->get_memory_manager()->get_open_bus() & 0xFE);
}

void MapperVRC2_4::write_register(uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        latch_ = value & 0x01;
        return;
    }

    addr = translate_address(addr) & 0xF00F;
    if (addr >= 0x8000 && addr <= 0x8006) {
        prg_reg_0_ = value & 0x1F;
    } else if ((variant_ <= Variant::VRC2C && addr >= 0x9000 && addr <= 0x9003)
               || (variant_ >= Variant::VRC4A && addr >= 0x9000 && addr <= 0x9001)) {
        uint8_t mask = 0x03;
        if (!use_heuristics_ && variant_ <= Variant::VRC2C) {
            mask = 0x01;
        }
        switch (value & mask) {
            case 0: set_mirroring_type(MirroringType::VERTICAL); break;
            case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
            case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
            case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
        }
    } else if (variant_ >= Variant::VRC4A && addr >= 0x9002 && addr <= 0x9003) {
        prg_mode_ = (value >> 1) & 0x01;
    } else if (addr >= 0xA000 && addr <= 0xA006) {
        prg_reg_1_ = value & 0x1F;
    } else if (addr >= 0xB000 && addr <= 0xE006) {
        uint8_t reg = ((((addr >> 12) & 0x07) - 3) << 1) + ((addr >> 1) & 0x01);
        if ((addr & 0x01) == 0) {
            chr_low_[reg] = value & 0x0F;
        } else {
            chr_high_[reg] = value & 0x1F;
        }
    } else if (addr == 0xF000) {
        set_irq_reload_nibble(value, false);
    } else if (addr == 0xF001) {
        set_irq_reload_nibble(value, true);
    } else if (addr == 0xF002) {
        set_irq_control(value);
    } else if (addr == 0xF003) {
        acknowledge_irq();
    }

    update_state();
}

} // namespace ear6::nes
