#include "mapper_019.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_sound_mixer.h"

namespace ear6::nes {

void Mapper019::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    auto_detect_variant_ = false;
    if (info.mapper_number == 210) {
        if (info.submapper_id == 1) {
            variant_ = Variant::NAMCO175;
        } else if (info.submapper_id == 2) {
            variant_ = Variant::NAMCO340;
        } else {
            variant_ = Variant::UNKNOWN;
            auto_detect_variant_ = true;
        }
    } else if (info.chip.find("175") != std::string::npos) {
        variant_ = Variant::NAMCO175;
    } else if (info.chip.find("340") != std::string::npos) {
        variant_ = Variant::NAMCO340;
    } else {
        variant_ = Variant::NAMCO163;
        auto_detect_variant_ = info.chip.find("163") == std::string::npos;
    }

    not_namco340_ = false;
    write_protect_ = 0;
    low_chr_nt_mode_ = false;
    high_chr_nt_mode_ = false;
    irq_counter_ = 0;
    audio_ram_.fill(0);
    channel_output_.fill(0);
    audio_ram_position_ = 0;
    audio_auto_increment_ = false;
    audio_update_counter_ = 0;
    current_audio_channel_ = 7;
    last_audio_output_ = 0;
    audio_disabled_ = false;

    init_work_ram(info);
    std::vector<uint8_t>& ram = info.has_battery ? save_ram_ : work_ram_;
    if (ram.size() < 0x2000) {
        ram.resize(0x2000, 0);
    }

    add_register_range(0x4800, 0x5FFF, MemoryOperation::ANY);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);

    set_mirroring_type(info.mirroring);
    select_prg_page(3, static_cast<uint16_t>(prg_size_ / get_prg_page_size() - 1));
    update_save_ram_access();
}

void Mapper019::set_variant(Variant variant) {
    if (auto_detect_variant_ && (!not_namco340_ || variant != Variant::NAMCO340)) {
        variant_ = variant;
    }
}

void Mapper019::update_save_ram_access() {
    std::vector<uint8_t>& ram = rom_info_.has_battery ? save_ram_ : work_ram_;
    if (variant_ == Variant::NAMCO163) {
        bool global_write_enable = (write_protect_ & 0x40) != 0;
        for (uint8_t page = 0; page < 4; page++) {
            int8_t access = global_write_enable && (write_protect_ & (1 << page)) == 0
                                ? READ_WRITE
                                : READ;
            uint16_t start = 0x6000 + page * 0x0800;
            set_cpu_memory_mapping(start, start + 0x07FF, ram.data(),
                                   page * 0x0800, static_cast<uint32_t>(ram.size()), access);
        }
    } else if (variant_ == Variant::NAMCO175) {
        int8_t access = (write_protect_ & 0x01) != 0 ? READ_WRITE : READ;
        set_cpu_memory_mapping(0x6000, 0x7FFF, ram.data(), 0,
                               static_cast<uint32_t>(ram.size()), access);
    } else {
        set_cpu_memory_mapping(0x6000, 0x7FFF, ram.data(), 0,
                               static_cast<uint32_t>(ram.size()), NO_ACCESS);
    }
}

void Mapper019::map_chr_page(uint8_t slot, uint8_t value, bool allow_nametable) {
    uint16_t start = slot * 0x0400;
    uint16_t end = start + 0x03FF;
    if (allow_nametable && value >= 0xE0) {
        set_ppu_memory_mapping(start, end, get_nametable(value & 0x01), 0,
                               NAMETABLE_SIZE, READ_WRITE);
        if (slot >= 8) {
            set_ppu_memory_mapping(start + 0x1000, end + 0x1000,
                                   get_nametable(value & 0x01), 0,
                                   NAMETABLE_SIZE, READ_WRITE);
        }
    } else {
        set_ppu_memory_mapping(start, end, value, ChrMemoryType::CHR_ROM, READ);
        if (slot >= 8) {
            set_ppu_memory_mapping(start + 0x1000, end + 0x1000, value,
                                   ChrMemoryType::CHR_ROM, READ);
        }
    }
}

uint32_t Mapper019::get_audio_frequency(uint8_t channel) const {
    uint8_t base = 0x40 + channel * 8;
    return ((audio_ram_[base + 4] & 0x03) << 16)
           | (audio_ram_[base + 2] << 8)
           | audio_ram_[base];
}

uint32_t Mapper019::get_audio_phase(uint8_t channel) const {
    uint8_t base = 0x40 + channel * 8;
    return (audio_ram_[base + 5] << 16)
           | (audio_ram_[base + 3] << 8)
           | audio_ram_[base + 1];
}

void Mapper019::set_audio_phase(uint8_t channel, uint32_t phase) {
    uint8_t base = 0x40 + channel * 8;
    audio_ram_[base + 5] = (phase >> 16) & 0xFF;
    audio_ram_[base + 3] = (phase >> 8) & 0xFF;
    audio_ram_[base + 1] = phase & 0xFF;
}

void Mapper019::update_audio_channel(uint8_t channel) {
    uint8_t base = 0x40 + channel * 8;
    uint16_t length = 256 - (audio_ram_[base + 4] & 0xFC);
    uint32_t phase = (get_audio_phase(channel) + get_audio_frequency(channel))
                     % (static_cast<uint32_t>(length) << 16);
    uint8_t sample_position = ((phase >> 16) + audio_ram_[base + 6]) & 0xFF;
    uint8_t packed_sample = audio_ram_[sample_position >> 1];
    int8_t sample = (sample_position & 1) ? (packed_sample >> 4) : (packed_sample & 0x0F);
    channel_output_[channel] = (sample - 8) * (audio_ram_[base + 7] & 0x0F);
    update_audio_output();
    set_audio_phase(channel, phase);
}

void Mapper019::update_audio_output() {
    uint8_t channel_count_minus_one = (audio_ram_[0x7F] >> 4) & 0x07;
    int16_t output = 0;
    for (int channel = 7; channel >= 7 - channel_count_minus_one; channel--) {
        output += channel_output_[channel];
    }
    output /= channel_count_minus_one + 1;

    console_->get_sound_mixer()->add_delta(
        AudioChannel::NAMCO163,
        console_->get_cpu()->get_cycle_count() % NesSoundMixer::CYCLE_LENGTH,
        output - last_audio_output_);
    last_audio_output_ = output;
}

void Mapper019::clock_audio() {
    if (audio_disabled_) return;

    audio_update_counter_++;
    if (audio_update_counter_ == 15) {
        update_audio_channel(static_cast<uint8_t>(current_audio_channel_));
        audio_update_counter_ = 0;
        current_audio_channel_--;
        int8_t first_channel = 7 - ((audio_ram_[0x7F] >> 4) & 0x07);
        if (current_audio_channel_ < first_channel) {
            current_audio_channel_ = 7;
        }
    }
}

void Mapper019::process_cpu_clock() {
    if ((irq_counter_ & 0x8000) != 0 && (irq_counter_ & 0x7FFF) != 0x7FFF) {
        irq_counter_++;
        if ((irq_counter_ & 0x7FFF) == 0x7FFF) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
    if (variant_ == Variant::NAMCO163) {
        clock_audio();
    }
}

void Mapper019::write_ram(uint16_t addr, uint8_t value) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        not_namco340_ = true;
        if (variant_ == Variant::NAMCO340) {
            set_variant(Variant::UNKNOWN);
        }
    }
    BaseMapper::write_ram(addr, value);
}

uint8_t Mapper019::read_register(uint16_t addr) {
    switch (addr & 0xF800) {
        case 0x4800: {
            uint8_t value = audio_ram_[audio_ram_position_];
            if (audio_auto_increment_) {
                audio_ram_position_ = (audio_ram_position_ + 1) & 0x7F;
            }
            return value;
        }
        case 0x5000:
            return irq_counter_ & 0xFF;
        case 0x5800:
            return irq_counter_ >> 8;
        default:
            return BaseMapper::read_register(addr);
    }
}

void Mapper019::write_register(uint16_t addr, uint8_t value) {
    addr &= 0xF800;
    switch (addr) {
        case 0x4800:
            set_variant(Variant::NAMCO163);
            audio_ram_[audio_ram_position_] = value;
            if (audio_auto_increment_) {
                audio_ram_position_ = (audio_ram_position_ + 1) & 0x7F;
            }
            break;

        case 0x5000:
            set_variant(Variant::NAMCO163);
            irq_counter_ = (irq_counter_ & 0xFF00) | value;
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;

        case 0x5800:
            set_variant(Variant::NAMCO163);
            irq_counter_ = (irq_counter_ & 0x00FF) | (static_cast<uint16_t>(value) << 8);
            console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            break;

        case 0x8000: case 0x8800: case 0x9000: case 0x9800: {
            uint8_t bank = (addr - 0x8000) >> 11;
            map_chr_page(bank, value, !low_chr_nt_mode_ && variant_ == Variant::NAMCO163);
            break;
        }

        case 0xA000: case 0xA800: case 0xB000: case 0xB800: {
            uint8_t bank = ((addr - 0xA000) >> 11) + 4;
            map_chr_page(bank, value, !high_chr_nt_mode_ && variant_ == Variant::NAMCO163);
            break;
        }

        case 0xC000: case 0xC800: case 0xD000: case 0xD800:
            if (addr >= 0xC800) {
                set_variant(Variant::NAMCO163);
            } else if (variant_ != Variant::NAMCO163) {
                set_variant(Variant::NAMCO175);
            }
            if (variant_ == Variant::NAMCO175) {
                write_protect_ = value;
                update_save_ram_access();
            } else {
                map_chr_page(((addr - 0xC000) >> 11) + 8, value,
                             variant_ == Variant::NAMCO163);
            }
            break;

        case 0xE000:
            if ((value & 0x80) != 0) {
                set_variant(Variant::NAMCO340);
            } else if ((value & 0x40) != 0 && variant_ != Variant::NAMCO163) {
                set_variant(Variant::NAMCO340);
            }
            select_prg_page(0, value & 0x3F);
            if (variant_ == Variant::NAMCO340) {
                switch ((value >> 6) & 0x03) {
                    case 0: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                    case 1: set_mirroring_type(MirroringType::VERTICAL); break;
                    case 2: set_mirroring_type(MirroringType::HORIZONTAL); break;
                    case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
                }
            } else if (variant_ == Variant::NAMCO163) {
                audio_disabled_ = (value & 0x40) != 0;
            }
            break;

        case 0xE800:
            select_prg_page(1, value & 0x3F);
            if (variant_ == Variant::NAMCO163) {
                low_chr_nt_mode_ = (value & 0x40) != 0;
                high_chr_nt_mode_ = (value & 0x80) != 0;
            }
            break;

        case 0xF000:
            select_prg_page(2, value & 0x3F);
            break;

        case 0xF800:
            set_variant(Variant::NAMCO163);
            if (variant_ == Variant::NAMCO163) {
                write_protect_ = value;
                update_save_ram_access();
                audio_ram_position_ = value & 0x7F;
                audio_auto_increment_ = (value & 0x80) != 0;
            }
            break;
    }
}

} // namespace ear6::nes
