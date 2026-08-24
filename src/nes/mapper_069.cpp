#include "mapper_069.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_sound_mixer.h"

namespace ear6::nes {

void Mapper069::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = static_cast<uint32_t>(prg_rom.size());
    chr_rom_size_ = static_cast<uint32_t>(chr_rom.size());

    command_ = 0;
    work_ram_value_ = 0;
    irq_enabled_ = false;
    irq_counter_enabled_ = false;
    irq_counter_ = 0;

    audio_registers_.fill(0);
    audio_timers_.fill(0);
    audio_tone_steps_.fill(0);
    current_audio_register_ = 0;
    last_audio_output_ = 0;
    process_audio_tick_ = false;
    double output = 1.0;
    audio_volume_lut_[0] = 0;
    for (uint8_t i = 1; i < audio_volume_lut_.size(); i++) {
        output *= 1.1885022274370184377301224648922;
        output *= 1.1885022274370184377301224648922;
        audio_volume_lut_[i] = static_cast<uint8_t>(output);
    }

    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    select_prg_page(3, static_cast<uint16_t>(-1));
    update_work_ram();
}

void Mapper069::setup_default_work_ram() {
    update_work_ram();
}

void Mapper069::update_work_ram() {
    if ((work_ram_value_ & 0x40) != 0) {
        std::vector<uint8_t>& ram = rom_info_.has_battery ? save_ram_ : work_ram_;
        uint32_t page_count = static_cast<uint32_t>(ram.size()) / 0x2000;
        uint32_t page = work_ram_value_ & 0x3F;
        if (page_count > 0) {
            page %= page_count;
        }
        int8_t access = (work_ram_value_ & 0x80) != 0 ? READ_WRITE : NO_ACCESS;
        set_cpu_memory_mapping(0x6000, 0x7FFF, ram.data(), page * 0x2000,
                               static_cast<uint32_t>(ram.size()), access);
    } else {
        set_cpu_memory_mapping(0x6000, 0x7FFF, work_ram_value_ & 0x3F,
                               PrgMemoryType::PRG_ROM, READ);
    }
}

uint16_t Mapper069::get_audio_period(uint8_t channel) const {
    return audio_registers_[channel * 2]
           | (static_cast<uint16_t>(audio_registers_[channel * 2 + 1]) << 8);
}

uint8_t Mapper069::get_audio_volume(uint8_t channel) const {
    return audio_volume_lut_[audio_registers_[8 + channel] & 0x0F];
}

bool Mapper069::is_tone_enabled(uint8_t channel) const {
    return ((audio_registers_[7] >> channel) & 0x01) == 0;
}

void Mapper069::update_audio_channel(uint8_t channel) {
    audio_timers_[channel]--;
    if (audio_timers_[channel] <= 0) {
        audio_timers_[channel] = static_cast<int16_t>(get_audio_period(channel));
        audio_tone_steps_[channel] = (audio_tone_steps_[channel] + 1) & 0x0F;
    }
}

void Mapper069::update_audio_output() {
    int16_t output = 0;
    for (uint8_t channel = 0; channel < 3; channel++) {
        if (is_tone_enabled(channel) && audio_tone_steps_[channel] < 0x08) {
            output += get_audio_volume(channel);
        }
    }
    console_->get_sound_mixer()->add_delta(
        AudioChannel::SUNSOFT5B,
        console_->get_cpu()->get_cycle_count() % NesSoundMixer::CYCLE_LENGTH,
        output - last_audio_output_);
    last_audio_output_ = output;
}

void Mapper069::clock_audio() {
    if (process_audio_tick_) {
        for (uint8_t channel = 0; channel < 3; channel++) {
            update_audio_channel(channel);
        }
        update_audio_output();
    }
    process_audio_tick_ = !process_audio_tick_;
}

void Mapper069::write_audio_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE000) {
        case 0xC000:
            current_audio_register_ = value;
            break;
        case 0xE000:
            if (current_audio_register_ <= 0x0F) {
                audio_registers_[current_audio_register_] = value;
            }
            break;
    }
}

void Mapper069::process_cpu_clock() {
    if (irq_counter_enabled_) {
        irq_counter_--;
        if (irq_counter_ == 0xFFFF && irq_enabled_) {
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
    }
    clock_audio();
}

void Mapper069::write_register(uint16_t addr, uint8_t value) {
    switch (addr & 0xE000) {
        case 0x8000:
            command_ = value & 0x0F;
            break;
        case 0xA000:
            if (command_ <= 7) {
                select_chr_page(command_, value);
            } else {
                switch (command_) {
                    case 8:
                        work_ram_value_ = value;
                        update_work_ram();
                        break;
                    case 9: case 0xA: case 0xB:
                        select_prg_page(command_ - 9, value & 0x3F);
                        break;
                    case 0xC:
                        switch (value & 0x03) {
                            case 0: set_mirroring_type(MirroringType::VERTICAL); break;
                            case 1: set_mirroring_type(MirroringType::HORIZONTAL); break;
                            case 2: set_mirroring_type(MirroringType::SCREEN_A_ONLY); break;
                            case 3: set_mirroring_type(MirroringType::SCREEN_B_ONLY); break;
                        }
                        break;
                    case 0xD:
                        irq_enabled_ = (value & 0x01) != 0;
                        irq_counter_enabled_ = (value & 0x80) != 0;
                        console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
                        break;
                    case 0xE:
                        irq_counter_ = (irq_counter_ & 0xFF00) | value;
                        break;
                    case 0xF:
                        irq_counter_ = (irq_counter_ & 0x00FF)
                                       | (static_cast<uint16_t>(value) << 8);
                        break;
                }
            }
            break;
        case 0xC000:
        case 0xE000:
            write_audio_register(addr, value);
            break;
    }
}

} // namespace ear6::nes
