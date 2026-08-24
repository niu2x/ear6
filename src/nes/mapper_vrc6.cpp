#include "mapper_vrc6.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_sound_mixer.h"

namespace ear6::nes {

void MapperVRC6::Pulse::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0x03) {
        case 0:
            volume = value & 0x0F;
            duty_cycle = (value >> 4) & 0x07;
            ignore_duty = (value & 0x80) != 0;
            break;
        case 1:
            frequency = (frequency & 0x0F00) | value;
            break;
        case 2:
            frequency = (frequency & 0x00FF) | ((value & 0x0F) << 8);
            enabled = (value & 0x80) != 0;
            if (!enabled) {
                step = 0;
            }
            break;
    }
}

void MapperVRC6::Pulse::clock() {
    if (!enabled) {
        return;
    }
    timer--;
    if (timer == 0) {
        step = (step + 1) & 0x0F;
        timer = (frequency >> frequency_shift) + 1;
    }
}

uint8_t MapperVRC6::Pulse::get_volume() const {
    if (!enabled) {
        return 0;
    }
    return ignore_duty || step <= duty_cycle ? volume : 0;
}

void MapperVRC6::Saw::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0x03) {
        case 0:
            accumulator_rate = value & 0x3F;
            break;
        case 1:
            frequency = (frequency & 0x0F00) | value;
            break;
        case 2:
            frequency = (frequency & 0x00FF) | ((value & 0x0F) << 8);
            enabled = (value & 0x80) != 0;
            if (!enabled) {
                accumulator = 0;
                step = 0;
            }
            break;
    }
}

void MapperVRC6::Saw::clock() {
    if (!enabled) {
        return;
    }
    timer--;
    if (timer == 0) {
        step = (step + 1) % 14;
        timer = (frequency >> frequency_shift) + 1;
        if (step == 0) {
            accumulator = 0;
        } else if ((step & 0x01) == 0) {
            accumulator += accumulator_rate;
        }
    }
}

uint8_t MapperVRC6::Saw::get_volume() const {
    return enabled ? accumulator >> 3 : 0;
}

void MapperVRC6::init(const RomInfo& info,
                      const std::vector<uint8_t>& prg_rom,
                      const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    swap_address_lines_ = info.mapper_number == 26;
    banking_mode_ = 0;
    chr_registers_.fill(0);
    irq_.initialize(console_);
    irq_.reset();
    pulse_1_ = Pulse{};
    pulse_2_ = Pulse{};
    saw_ = Saw{};
    halt_audio_ = false;
    last_audio_output_ = 0;

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
}

void MapperVRC6::update_prg_ram_access() {
    std::vector<uint8_t>& ram = rom_info_.has_battery ? save_ram_ : work_ram_;
    int8_t access = (banking_mode_ & 0x80) != 0 ? READ_WRITE : NO_ACCESS;
    set_cpu_memory_mapping(0x6000, 0x7FFF, ram.data(), 0,
                           static_cast<uint32_t>(ram.size()), access);
}

void MapperVRC6::set_ppu_mapping(uint8_t bank, uint8_t page) {
    uint16_t start = 0x2000 + bank * 0x0400;
    set_ppu_memory_mapping(start, start + 0x03FF, page);
    set_ppu_memory_mapping(start + 0x1000, start + 0x13FF, page);
}

void MapperVRC6::update_ppu_banking() {
    uint8_t mask = (banking_mode_ & 0x20) != 0 ? 0xFE : 0xFF;
    uint8_t or_mask = (banking_mode_ & 0x20) != 0 ? 1 : 0;

    switch (banking_mode_ & 0x03) {
        case 0:
            for (uint8_t slot = 0; slot < 8; slot++) {
                select_chr_page(slot, chr_registers_[slot]);
            }
            break;
        case 1:
            for (uint8_t pair = 0; pair < 4; pair++) {
                select_chr_page(pair * 2, chr_registers_[pair] & mask);
                select_chr_page(pair * 2 + 1,
                                (chr_registers_[pair] & mask) | or_mask);
            }
            break;
        case 2:
        case 3:
            for (uint8_t slot = 0; slot < 4; slot++) {
                select_chr_page(slot, chr_registers_[slot]);
            }
            select_chr_page(4, chr_registers_[4] & mask);
            select_chr_page(5, (chr_registers_[4] & mask) | or_mask);
            select_chr_page(6, chr_registers_[5] & mask);
            select_chr_page(7, (chr_registers_[5] & mask) | or_mask);
            break;
    }

    if ((banking_mode_ & 0x10) != 0) {
        switch (banking_mode_ & 0x2F) {
            case 0x20:
            case 0x27:
                set_ppu_mapping(0, chr_registers_[6] & 0xFE);
                set_ppu_mapping(1, (chr_registers_[6] & 0xFE) | 1);
                set_ppu_mapping(2, chr_registers_[7] & 0xFE);
                set_ppu_mapping(3, (chr_registers_[7] & 0xFE) | 1);
                break;
            case 0x23:
            case 0x24:
                set_ppu_mapping(0, chr_registers_[6] & 0xFE);
                set_ppu_mapping(1, chr_registers_[7] & 0xFE);
                set_ppu_mapping(2, (chr_registers_[6] & 0xFE) | 1);
                set_ppu_mapping(3, (chr_registers_[7] & 0xFE) | 1);
                break;
            case 0x28:
            case 0x2F:
                set_ppu_mapping(0, chr_registers_[6] & 0xFE);
                set_ppu_mapping(1, chr_registers_[6] & 0xFE);
                set_ppu_mapping(2, chr_registers_[7] & 0xFE);
                set_ppu_mapping(3, chr_registers_[7] & 0xFE);
                break;
            case 0x2B:
            case 0x2C:
                set_ppu_mapping(0, (chr_registers_[6] & 0xFE) | 1);
                set_ppu_mapping(1, (chr_registers_[7] & 0xFE) | 1);
                set_ppu_mapping(2, (chr_registers_[6] & 0xFE) | 1);
                set_ppu_mapping(3, (chr_registers_[7] & 0xFE) | 1);
                break;
            default:
                switch (banking_mode_ & 0x07) {
                    case 0:
                    case 6:
                    case 7:
                        set_ppu_mapping(0, chr_registers_[6]);
                        set_ppu_mapping(1, chr_registers_[6]);
                        set_ppu_mapping(2, chr_registers_[7]);
                        set_ppu_mapping(3, chr_registers_[7]);
                        break;
                    case 1:
                    case 5:
                        set_ppu_mapping(0, chr_registers_[4]);
                        set_ppu_mapping(1, chr_registers_[5]);
                        set_ppu_mapping(2, chr_registers_[6]);
                        set_ppu_mapping(3, chr_registers_[7]);
                        break;
                    case 2:
                    case 3:
                    case 4:
                        set_ppu_mapping(0, chr_registers_[6]);
                        set_ppu_mapping(1, chr_registers_[7]);
                        set_ppu_mapping(2, chr_registers_[6]);
                        set_ppu_mapping(3, chr_registers_[7]);
                        break;
                }
                break;
        }
    } else {
        switch (banking_mode_ & 0x2F) {
            case 0x20:
            case 0x27:
                set_mirroring_type(MirroringType::VERTICAL);
                break;
            case 0x23:
            case 0x24:
                set_mirroring_type(MirroringType::HORIZONTAL);
                break;
            case 0x28:
            case 0x2F:
                set_mirroring_type(MirroringType::SCREEN_A_ONLY);
                break;
            case 0x2B:
            case 0x2C:
                set_mirroring_type(MirroringType::SCREEN_B_ONLY);
                break;
            default:
                switch (banking_mode_ & 0x07) {
                    case 0:
                    case 6:
                    case 7:
                        set_nametable(0, chr_registers_[6] & 0x01);
                        set_nametable(1, chr_registers_[6] & 0x01);
                        set_nametable(2, chr_registers_[7] & 0x01);
                        set_nametable(3, chr_registers_[7] & 0x01);
                        break;
                    case 1:
                    case 5:
                        set_nametable(0, chr_registers_[4] & 0x01);
                        set_nametable(1, chr_registers_[5] & 0x01);
                        set_nametable(2, chr_registers_[6] & 0x01);
                        set_nametable(3, chr_registers_[7] & 0x01);
                        break;
                    case 2:
                    case 3:
                    case 4:
                        set_nametable(0, chr_registers_[6] & 0x01);
                        set_nametable(1, chr_registers_[7] & 0x01);
                        set_nametable(2, chr_registers_[6] & 0x01);
                        set_nametable(3, chr_registers_[7] & 0x01);
                        break;
                }
                break;
        }
    }
    update_prg_ram_access();
}

void MapperVRC6::write_audio_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xF003) {
        case 0x9000: case 0x9001: case 0x9002:
            pulse_1_.write_register(addr, value);
            break;
        case 0x9003: {
            halt_audio_ = (value & 0x01) != 0;
            uint8_t shift = (value & 0x04) != 0 ? 8 : ((value & 0x02) != 0 ? 4 : 0);
            pulse_1_.set_frequency_shift(shift);
            pulse_2_.set_frequency_shift(shift);
            saw_.set_frequency_shift(shift);
            break;
        }
        case 0xA000: case 0xA001: case 0xA002:
            pulse_2_.write_register(addr, value);
            break;
        case 0xB000: case 0xB001: case 0xB002:
            saw_.write_register(addr, value);
            break;
    }
}

void MapperVRC6::clock_audio() {
    if (!halt_audio_) {
        pulse_1_.clock();
        pulse_2_.clock();
        saw_.clock();
    }
    int32_t output = pulse_1_.get_volume() + pulse_2_.get_volume() + saw_.get_volume();
    console_->get_sound_mixer()->add_delta(
        AudioChannel::VRC6,
        console_->get_cpu()->get_cycle_count() % NesSoundMixer::CYCLE_LENGTH,
        static_cast<int16_t>((output - last_audio_output_) * 15));
    last_audio_output_ = output;
}

void MapperVRC6::process_cpu_clock() {
    irq_.process_cpu_clock();
    clock_audio();
}

void MapperVRC6::write_register(uint16_t addr, uint8_t value) {
    if (swap_address_lines_) {
        addr = (addr & 0xFFFC) | ((addr & 0x01) << 1) | ((addr & 0x02) >> 1);
    }

    switch (addr & 0xF003) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            select_prg_page_2x(0, (value & 0x0F) << 1);
            break;
        case 0x9000: case 0x9001: case 0x9002: case 0x9003:
        case 0xA000: case 0xA001: case 0xA002:
        case 0xB000: case 0xB001: case 0xB002:
            write_audio_register(addr, value);
            break;
        case 0xB003:
            banking_mode_ = value;
            update_ppu_banking();
            break;
        case 0xC000: case 0xC001: case 0xC002: case 0xC003:
            select_prg_page(2, value & 0x1F);
            break;
        case 0xD000: case 0xD001: case 0xD002: case 0xD003:
            chr_registers_[addr & 0x03] = value;
            update_ppu_banking();
            break;
        case 0xE000: case 0xE001: case 0xE002: case 0xE003:
            chr_registers_[4 + (addr & 0x03)] = value;
            update_ppu_banking();
            break;
        case 0xF000:
            irq_.set_reload_value(value);
            break;
        case 0xF001:
            irq_.set_control_value(value);
            break;
        case 0xF002:
            irq_.acknowledge_irq();
            break;
    }
}

} // namespace ear6::nes
