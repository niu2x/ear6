#include "mapper_090.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"

namespace ear6::nes {

void Mapper090::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    prg_registers_.fill(0);
    chr_low_registers_.fill(0);
    chr_high_registers_.fill(0);
    prg_mode_ = 0;
    enable_prg_at_6000_ = false;
    chr_mode_ = 0;
    chr_block_mode_ = false;
    chr_block_ = 0;
    mirror_chr_ = false;
    mirroring_register_ = 0;
    irq_enabled_ = false;
    irq_source_ = IrqSource::CPU_CLOCK;
    irq_count_direction_ = 0;
    irq_small_prescaler_ = false;
    irq_prescaler_ = 0;
    irq_counter_ = 0;
    irq_xor_register_ = 0;
    last_ppu_address_ = 0;
    multiply_value_1_ = 0;
    multiply_value_2_ = 0;
    register_ram_value_ = 0;

    set_mirroring_type(info.mirroring);
    add_register_range(0x5000, 0x5FFF, MemoryOperation::ANY);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    update_state();
}

void Mapper090::setup_default_work_ram() {
    update_prg_state();
}

uint8_t Mapper090::invert_prg_bits(uint8_t value, bool invert) const {
    if (!invert) {
        return value;
    }
    return ((value & 0x01) << 6)
           | ((value & 0x02) << 4)
           | ((value & 0x04) << 2)
           | ((value & 0x10) >> 2)
           | ((value & 0x20) >> 4)
           | ((value & 0x40) >> 6);
}

void Mapper090::update_prg_state() {
    bool invert = (prg_mode_ & 0x03) == 0x03;
    std::array<uint8_t, 4> pages = {};
    for (uint8_t i = 0; i < pages.size(); i++) {
        pages[i] = invert_prg_bits(prg_registers_[i], invert);
    }

    switch (prg_mode_ & 0x03) {
        case 0:
            select_prg_page_4x(0, (prg_mode_ & 0x04) != 0 ? pages[3] : 0x3C);
            if (enable_prg_at_6000_) {
                set_cpu_memory_mapping(0x6000, 0x7FFF, pages[3] * 4 + 3,
                                       PrgMemoryType::PRG_ROM, READ);
            }
            break;
        case 1:
            select_prg_page_2x(0, pages[1] << 1);
            select_prg_page_2x(2, (prg_mode_ & 0x04) != 0
                                  ? pages[3] << 1 : 0x3E);
            if (enable_prg_at_6000_) {
                set_cpu_memory_mapping(0x6000, 0x7FFF, pages[3] * 2 + 1,
                                       PrgMemoryType::PRG_ROM, READ);
            }
            break;
        case 2:
        case 3:
            select_prg_page(0, pages[0]);
            select_prg_page(1, pages[1]);
            select_prg_page(2, pages[2]);
            select_prg_page(3, (prg_mode_ & 0x04) != 0 ? pages[3] : 0x3F);
            if (enable_prg_at_6000_) {
                set_cpu_memory_mapping(0x6000, 0x7FFF, pages[3],
                                       PrgMemoryType::PRG_ROM, READ);
            }
            break;
    }

    if (!enable_prg_at_6000_) {
        remove_cpu_memory_mapping(0x6000, 0x7FFF);
    }
}

uint16_t Mapper090::get_chr_register(uint8_t index) const {
    if (chr_mode_ >= 2 && mirror_chr_ && (index == 2 || index == 3)) {
        index -= 2;
    }
    if (!chr_block_mode_) {
        return chr_low_registers_[index]
               | (static_cast<uint16_t>(chr_high_registers_[index]) << 8);
    }

    static constexpr uint8_t MASKS[] = {0x1F, 0x3F, 0x7F, 0xFF};
    uint8_t mask = MASKS[chr_mode_ & 0x03];
    uint8_t shift = 5 + (chr_mode_ & 0x03);
    return (chr_low_registers_[index] & mask)
           | (static_cast<uint16_t>(chr_block_) << shift);
}

void Mapper090::update_chr_state() {
    std::array<uint16_t, 8> pages = {};
    for (uint8_t i = 0; i < pages.size(); i++) {
        pages[i] = get_chr_register(i);
    }

    switch (chr_mode_) {
        case 0:
            select_chr_page_8x(0, pages[0] << 3);
            break;
        case 1:
            select_chr_page_4x(0, pages[0] << 2);
            select_chr_page_4x(4, pages[4] << 2);
            break;
        case 2:
            select_chr_page_2x(0, pages[0] << 1);
            select_chr_page_2x(2, pages[2] << 1);
            select_chr_page_2x(4, pages[4] << 1);
            select_chr_page_2x(6, pages[6] << 1);
            break;
        case 3:
            for (uint8_t i = 0; i < pages.size(); i++) {
                select_chr_page(i, pages[i]);
            }
            break;
    }
}

void Mapper090::update_mirroring_state() {
    switch (mirroring_register_) {
        case 0: set_mirroring_type(MirroringType::VERTICAL); break;
        case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
        case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
        case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
    }
}

void Mapper090::update_state() {
    update_prg_state();
    update_chr_state();
    update_mirroring_state();
}

uint8_t Mapper090::read_register(uint16_t addr) {
    uint16_t key = addr & 0xF803;
    uint16_t product = static_cast<uint16_t>(multiply_value_1_)
                       * multiply_value_2_;
    switch (key) {
        case 0x5000: return 0;
        case 0x5800: return product & 0xFF;
        case 0x5801: return product >> 8;
        case 0x5803: return register_ram_value_;
        default: return console_->get_memory_manager()->get_open_bus();
    }
}

void Mapper090::write_register(uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        switch (addr & 0xF803) {
            case 0x5800: multiply_value_1_ = value; break;
            case 0x5801: multiply_value_2_ = value; break;
            case 0x5803: register_ram_value_ = value; break;
        }
        update_state();
        return;
    }

    switch (addr & 0xF007) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
        case 0x8004: case 0x8005: case 0x8006: case 0x8007:
            prg_registers_[addr & 0x03] = value & 0x7F;
            break;
        case 0x9000: case 0x9001: case 0x9002: case 0x9003:
        case 0x9004: case 0x9005: case 0x9006: case 0x9007:
            chr_low_registers_[addr & 0x07] = value;
            break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
        case 0xA004: case 0xA005: case 0xA006: case 0xA007:
            chr_high_registers_[addr & 0x07] = value;
            break;
        case 0xC000:
            irq_enabled_ = (value & 0x01) != 0;
            if (!irq_enabled_) {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            }
            break;
        case 0xC001:
            irq_count_direction_ = (value >> 6) & 0x03;
            irq_small_prescaler_ = (value & 0x04) != 0;
            irq_source_ = static_cast<IrqSource>(value & 0x03);
            break;
        case 0xC002:
            irq_enabled_ = false;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;
        case 0xC003:
            irq_enabled_ = true;
            break;
        case 0xC004: irq_prescaler_ = value ^ irq_xor_register_; break;
        case 0xC005: irq_counter_ = value ^ irq_xor_register_; break;
        case 0xC006: irq_xor_register_ = value; break;
        case 0xD000:
            prg_mode_ = value & 0x07;
            chr_mode_ = (value >> 3) & 0x03;
            enable_prg_at_6000_ = (value & 0x80) != 0;
            break;
        case 0xD001:
            mirroring_register_ = value & 0x03;
            break;
        case 0xD003:
            mirror_chr_ = (value & 0x80) != 0;
            chr_block_mode_ = (value & 0x20) == 0;
            chr_block_ = ((value & 0x18) >> 2) | (value & 0x01);
            break;
    }
    update_state();
}

void Mapper090::tick_irq_counter() {
    bool clock_counter = false;
    uint8_t mask = irq_small_prescaler_ ? 0x07 : 0xFF;
    uint8_t prescaler = irq_prescaler_ & mask;
    if (irq_count_direction_ == 0x01) {
        prescaler++;
        clock_counter = (prescaler & mask) == 0;
    } else if (irq_count_direction_ == 0x02) {
        prescaler--;
        clock_counter = prescaler == 0;
    }
    irq_prescaler_ = (irq_prescaler_ & ~mask) | (prescaler & mask);

    if (!clock_counter) {
        return;
    }
    if (irq_count_direction_ == 0x01) {
        irq_counter_++;
        if (irq_counter_ == 0 && irq_enabled_) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    } else if (irq_count_direction_ == 0x02) {
        irq_counter_--;
        if (irq_counter_ == 0xFF && irq_enabled_) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
}

void Mapper090::process_cpu_clock() {
    if (irq_source_ == IrqSource::CPU_CLOCK
        || (irq_source_ == IrqSource::CPU_WRITE
            && console_->get_cpu()->is_cpu_write())) {
        tick_irq_counter();
    }
}

uint8_t Mapper090::read_vram_custom(uint16_t addr) {
    if (irq_source_ == IrqSource::PPU_READ) {
        tick_irq_counter();
    }
    return read_vram(addr);
}

void Mapper090::notify_vram_address_change(uint16_t addr) {
    if (irq_source_ == IrqSource::PPU_A12_RISE
        && (addr & 0x1000) != 0
        && (last_ppu_address_ & 0x1000) == 0) {
        tick_irq_counter();
    }
    last_ppu_address_ = addr;
}

} // namespace ear6::nes
