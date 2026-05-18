#include "mapper_005.h"

#include "nes_console.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include "nes_ppu.h"
#include "nes_sound_mixer.h"

#include <algorithm>
#include <cstring>

namespace ear6::nes {

static constexpr uint8_t MMC5_DUTY[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 1, 1},
    {0, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 0, 0},
};

static constexpr uint8_t LENGTH_TABLE[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

void Mapper005::init(const RomInfo& info,
                     const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info;
    prg_rom_ = prg_rom;
    chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size();
    chr_rom_size_ = (uint32_t)chr_rom.size();

    add_register_range(0xFFFA, 0xFFFB, MemoryOperation::READ);
    add_register_range(0x5000, 0x5206, MemoryOperation::ANY);

    exram_.assign(0x400, 0);
    wram_.assign(0x10000, 0);

    set_extended_ram_mode(0);
    write_register(0x5100, 0x03);
    write_register(0x5117, 0xFF);
    update_chr_banks(true);
}

void Mapper005::reset(bool) {
    ppu_in_frame_ = false;
    need_in_frame_ = false;
    ppu_idle_counter_ = 0;
    nt_read_counter_ = 0;
    last_ppu_read_addr_ = 0;
    scanline_counter_ = 0;
    irq_pending_ = false;
    console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
}

void Mapper005::process_cpu_clock() {
    if (ppu_idle_counter_ > 0) {
        ppu_idle_counter_--;
        if (ppu_idle_counter_ == 0) {
            ppu_in_frame_ = false;
            update_chr_banks(true);
        }
    }

    clock_audio();
}

void Mapper005::clock_audio() {
    for (int i = 0; i < 2; ++i) {
        if (mmc5_sq_timer_[i] == 0) {
            mmc5_sq_duty_pos_[i] = (mmc5_sq_duty_pos_[i] - 1) & 0x07;
            mmc5_sq_timer_[i] = mmc5_sq_period_[i];
            mmc5_sq_output_[i] = MMC5_DUTY[mmc5_sq_duty_[i]][mmc5_sq_duty_pos_[i]]
                                 * (int8_t)(mmc5_sq_constant_volume_[i] ? mmc5_sq_volume_[i] : mmc5_sq_env_ctr_[i]);
            if (mmc5_sq_len_[i] == 0) {
                mmc5_sq_output_[i] = 0;
            }
        } else {
            mmc5_sq_timer_[i]--;
        }
    }

    mmc5_env_clock_divider_--;
    if (mmc5_env_clock_divider_ <= 0) {
        mmc5_env_clock_divider_ = 7457;
        for (int i = 0; i < 2; ++i) {
            if (!mmc5_sq_halt_[i] && mmc5_sq_len_[i] > 0) {
                mmc5_sq_len_[i]--;
            }
            if (mmc5_sq_env_start_[i]) {
                mmc5_sq_env_start_[i] = false;
                mmc5_sq_env_ctr_[i] = 15;
                mmc5_sq_env_div_[i] = mmc5_sq_volume_[i];
            } else if (mmc5_sq_env_div_[i] == 0) {
                mmc5_sq_env_div_[i] = mmc5_sq_volume_[i];
                if (mmc5_sq_env_ctr_[i] > 0) {
                    mmc5_sq_env_ctr_[i]--;
                } else if (mmc5_sq_halt_[i]) {
                    mmc5_sq_env_ctr_[i] = 15;
                }
            } else {
                mmc5_sq_env_div_[i]--;
            }
        }
    }

    const int16_t mixed = -(mmc5_sq_output_[0] + mmc5_sq_output_[1] + pcm_output_);
    if (mixed != mmc5_last_mix_) {
        console_->get_sound_mixer()->add_delta(AudioChannel::MMC5, console_->get_cpu()->get_cycle_count() % NesSoundMixer::CYCLE_LENGTH, mixed - mmc5_last_mix_);
        mmc5_last_mix_ = mixed;
    }
}

void Mapper005::update_prg_banks() {
    auto map_8k = [&](uint16_t start, uint8_t bank) {
        if (bank & 0x80) {
            set_cpu_memory_mapping(start, (uint16_t)(start + 0x1FFF), bank & 0x7F, PrgMemoryType::PRG_ROM, READ);
        } else {
            uint8_t idx = bank & 0x0F;
            set_cpu_memory_mapping(start, (uint16_t)(start + 0x1FFF), wram_.data(), idx * 0x2000, 0x2000,
                                   (prg_ram_protect1_ == 0x02 && prg_ram_protect2_ == 0x01) ? READ_WRITE : READ);
        }
    };

    map_8k(0x6000, prg_banks_[0]);

    switch (prg_mode_) {
        case 0:
            set_cpu_memory_mapping(0x8000, 0xFFFF, prg_banks_[4] & 0x7C, PrgMemoryType::PRG_ROM, READ);
            break;
        case 1:
            set_cpu_memory_mapping(0x8000, 0xBFFF, prg_banks_[2] & 0x7E, PrgMemoryType::PRG_ROM, READ);
            set_cpu_memory_mapping(0xC000, 0xFFFF, prg_banks_[4] & 0x7E, PrgMemoryType::PRG_ROM, READ);
            break;
        case 2:
            set_cpu_memory_mapping(0x8000, 0xBFFF, prg_banks_[2] & 0x7E, PrgMemoryType::PRG_ROM, READ);
            map_8k(0xC000, prg_banks_[3]);
            set_cpu_memory_mapping(0xE000, 0xFFFF, prg_banks_[4] & 0x7F, PrgMemoryType::PRG_ROM, READ);
            break;
        case 3:
        default:
            map_8k(0x8000, prg_banks_[1]);
            map_8k(0xA000, prg_banks_[2]);
            map_8k(0xC000, prg_banks_[3]);
            set_cpu_memory_mapping(0xE000, 0xFFFF, prg_banks_[4] & 0x7F, PrgMemoryType::PRG_ROM, READ);
            break;
    }
}

void Mapper005::update_chr_banks(bool force_update) {
    NesPpu* ppu = console_ ? console_->get_ppu() : nullptr;
    const bool large_sprites = ppu ? ppu->is_large_sprites() : false;
    if (!large_sprites) {
        last_chr_reg_ = 0;
    }

    const bool chr_a = !large_sprites
        || (split_tile_number_ >= 32 && split_tile_number_ < 40)
        || (!ppu_in_frame_ && last_chr_reg_ <= 0x5127);
    if (!force_update && chr_a == prev_chr_a_) {
        return;
    }
    prev_chr_a_ = chr_a;

    if (chr_mode_ == 0) {
        select_chr_page_8x(0, chr_banks_[chr_a ? 0x07 : 0x0B] << 3);
    } else if (chr_mode_ == 1) {
        select_chr_page_4x(0, chr_banks_[chr_a ? 0x03 : 0x0B] << 2);
        select_chr_page_4x(1, chr_banks_[chr_a ? 0x07 : 0x0B] << 2);
    } else if (chr_mode_ == 2) {
        select_chr_page_2x(0, chr_banks_[chr_a ? 0x01 : 0x09] << 1);
        select_chr_page_2x(1, chr_banks_[chr_a ? 0x03 : 0x0B] << 1);
        select_chr_page_2x(2, chr_banks_[chr_a ? 0x05 : 0x09] << 1);
        select_chr_page_2x(3, chr_banks_[chr_a ? 0x07 : 0x0B] << 1);
    } else {
        for (int i = 0; i < 8; ++i) {
            const int idx = chr_a ? i : (8 + (i & 3));
            select_chr_page(i, chr_banks_[idx]);
        }
    }
}

void Mapper005::set_fill_mode_tile(uint8_t tile) {
    fill_mode_tile_ = tile;
}

void Mapper005::set_fill_mode_color(uint8_t color) {
    fill_mode_color_ = color & 0x03;
    fill_mode_attr_byte_ = (uint8_t)(fill_mode_color_ | (fill_mode_color_ << 2) | (fill_mode_color_ << 4) | (fill_mode_color_ << 6));
}

void Mapper005::update_nametable_mapping() {
    for (int i = 0; i < 4; ++i) {
        const uint8_t nt_id = (nametable_mapping_ >> (i * 2)) & 0x03;
        if (nt_id <= 1) {
            set_nametable(i, nt_id);
        }
    }
}

void Mapper005::set_extended_ram_mode(uint8_t mode) {
    extended_ram_mode_ = mode & 0x03;
    update_nametable_mapping();
}

uint8_t Mapper005::read_vram_custom(uint16_t addr) {
    addr &= 0x3FFF;

    const bool is_nt_fetch = addr >= 0x2000 && addr <= 0x2FFF && (addr & 0x03FF) < 0x03C0;
    if (is_nt_fetch) {
        split_in_split_region_ = false;
        split_tile_number_++;
        if (!ppu_in_frame_ && need_in_frame_) {
            need_in_frame_ = false;
            ppu_in_frame_ = true;
        }
    }

    if (nt_read_counter_ >= 2) {
        if (!ppu_in_frame_ && !need_in_frame_) {
            need_in_frame_ = true;
            scanline_counter_ = 0;
        } else {
            scanline_counter_++;
                    if (irq_counter_target_ == scanline_counter_) {
                        irq_pending_ = true;
                        if (irq_enabled_) {
                            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
                }
            }
        }
        nt_read_counter_ = 0;
    } else if (addr >= 0x2000 && addr <= 0x2FFF) {
        if (last_ppu_read_addr_ == addr) {
            nt_read_counter_++;
            if (nt_read_counter_ >= 2) {
                split_tile_number_ = 0;
            }
        } else {
            nt_read_counter_ = 0;
        }
    } else {
        nt_read_counter_ = 0;
    }

    ppu_idle_counter_ = 3;
    last_ppu_read_addr_ = addr;

    if (extended_ram_mode_ <= 1 && ppu_in_frame_) {
        if (vertical_split_enabled_) {
            const uint8_t scanline = split_tile_number_ >= 41 ? (uint8_t)(scanline_counter_ + 1) : scanline_counter_;
            const uint8_t split_scroll = (uint8_t)((scanline + vertical_split_scroll_) % 240);
            const uint8_t col = (uint8_t)((split_tile_number_ + 2) % 42);
            if (addr >= 0x2000) {
                if (is_nt_fetch) {
                    if (col <= 32 && ((vertical_split_right_side_ && col >= vertical_split_delimiter_tile_) ||
                                      (!vertical_split_right_side_ && col < vertical_split_delimiter_tile_))) {
                        split_in_split_region_ = true;
                        split_tile_ = (uint16_t)(((split_scroll & 0xF8) << 2) | col);
                        return exram_[split_tile_ & 0x3FF];
                    }
                    split_in_split_region_ = false;
                } else if (split_in_split_region_) {
                    const uint8_t shift = (uint8_t)(((split_tile_ >> 4) & 0x04) | (split_tile_ & 0x02));
                    const uint16_t at_addr = (uint16_t)(0x3C0 | ((split_tile_ & 0x380) >> 4) | ((split_tile_ & 0x1F) >> 2));
                    const uint8_t pal = (uint8_t)((exram_[at_addr & 0x3FF] >> shift) & 0x03);
                    return (uint8_t)(pal * 0x55);
                }
            } else if (split_in_split_region_) {
                const uint32_t chr_addr = (vertical_split_bank_ << 12) + (((addr & ~0x07) | (split_scroll & 0x07)) & 0x0FFF);
                return chr_rom_size_ ? chr_rom_[chr_addr & (chr_rom_size_ - 1)] : chr_ram_[chr_addr & (chr_ram_size_ - 1)];
            }
        }

        if (extended_ram_mode_ == 1 && (split_tile_number_ < 32 || split_tile_number_ >= 40)) {
            if (is_nt_fetch) {
                ex_attribute_last_nt_fetch_ = addr & 0x03FF;
                ex_attr_last_fetch_counter_ = 3;
            } else if (ex_attr_last_fetch_counter_ > 0) {
                ex_attr_last_fetch_counter_--;
                if (ex_attr_last_fetch_counter_ == 2) {
                    const uint8_t value = exram_[ex_attribute_last_nt_fetch_ & 0x3FF];
                    ex_attr_selected_chr_bank_ = (uint8_t)((value & 0x3F) | (chr_upper_bits_ << 6));
                    const uint8_t palette = (uint8_t)((value & 0xC0) >> 6);
                    return (uint8_t)(palette * 0x55);
                }
                if (ex_attr_last_fetch_counter_ == 1 || ex_attr_last_fetch_counter_ == 0) {
                    const uint32_t caddr = (ex_attr_selected_chr_bank_ << 12) + (addr & 0x0FFF);
                    return chr_rom_size_ ? chr_rom_[caddr & (chr_rom_size_ - 1)] : chr_ram_[caddr & (chr_ram_size_ - 1)];
                }
            }
        }
    }

    if (addr >= 0x2000 && addr <= 0x2FFF) {
        const int nt = (addr - 0x2000) / 0x400;
        const uint16_t nt_off = addr & 0x3FF;
        const uint8_t nt_id = (nametable_mapping_ >> (nt * 2)) & 0x03;
        if (nt_id == 2) {
            if (extended_ram_mode_ <= 1 || extended_ram_mode_ == 2) {
                return exram_[nt_off];
            }
            return 0;
        }
        if (nt_id == 3) {
            if (nt_off < 32 * 30) {
                return fill_mode_tile_;
            }
            return fill_mode_attr_byte_;
        }
    }

    return read_vram(addr);
}

uint8_t Mapper005::read_ram(uint16_t addr) {
    if (pcm_read_mode_ && addr >= 0x8000) {
        const uint8_t value = BaseMapper::read_ram(addr);
        if (value != 0) {
            pcm_output_ = value;
            if (pcm_irq_pending_) {
                pcm_irq_pending_ = false;
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            }
        } else if (pcm_irq_enabled_) {
            pcm_irq_pending_ = true;
            console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
        }
        return value;
    }
    return BaseMapper::read_ram(addr);
}

void Mapper005::write_register(uint16_t addr, uint8_t value) {
    if (addr >= 0x5113 && addr <= 0x5117) {
        prg_banks_[addr - 0x5113] = value;
        update_prg_banks();
        return;
    }
    if (addr >= 0x5120 && addr <= 0x512B) {
        const uint16_t new_val = (uint16_t)(value | (chr_upper_bits_ << 8));
        if (new_val != chr_banks_[addr - 0x5120] || last_chr_reg_ != addr) {
            chr_banks_[addr - 0x5120] = new_val;
            last_chr_reg_ = addr;
            update_chr_banks(true);
        }
        return;
    }

    switch (addr) {
        case 0x5000:
        case 0x5004: {
            const int c = (addr == 0x5000) ? 0 : 1;
            mmc5_sq_duty_[c] = (value >> 6) & 0x03;
            mmc5_sq_halt_[c] = (value & 0x20) != 0;
            mmc5_sq_constant_volume_[c] = (value & 0x10) != 0;
            mmc5_sq_volume_[c] = value & 0x0F;
            break;
        }
        case 0x5002:
        case 0x5006: {
            const int c = (addr == 0x5002) ? 0 : 1;
            mmc5_sq_period_[c] = (mmc5_sq_period_[c] & 0x0700) | value;
            break;
        }
        case 0x5003:
        case 0x5007: {
            const int c = (addr == 0x5003) ? 0 : 1;
            mmc5_sq_period_[c] = (mmc5_sq_period_[c] & 0x00FF) | ((value & 0x07) << 8);
            mmc5_sq_duty_pos_[c] = 0;
            mmc5_sq_env_start_[c] = true;
            mmc5_sq_len_[c] = LENGTH_TABLE[(value >> 3) & 0x1F];
            break;
        }
        case 0x5010:
            pcm_read_mode_ = (value & 0x01) != 0;
            pcm_irq_enabled_ = (value & 0x80) != 0;
            if (!pcm_irq_enabled_ && pcm_irq_pending_) {
                pcm_irq_pending_ = false;
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            }
            break;
        case 0x5011:
            if (!pcm_read_mode_ && value != 0) {
                pcm_output_ = value;
            }
            break;
        case 0x5015:
            if ((value & 0x01) == 0) mmc5_sq_len_[0] = 0;
            if ((value & 0x02) == 0) mmc5_sq_len_[1] = 0;
            break;
        case 0x5100:
            prg_mode_ = value & 0x03;
            update_prg_banks();
            break;
        case 0x5101:
            chr_mode_ = value & 0x03;
            update_chr_banks(true);
            break;
        case 0x5102:
            prg_ram_protect1_ = value & 0x03;
            update_prg_banks();
            break;
        case 0x5103:
            prg_ram_protect2_ = value & 0x03;
            update_prg_banks();
            break;
        case 0x5104:
            set_extended_ram_mode(value & 0x03);
            break;
        case 0x5105:
            nametable_mapping_ = value;
            update_nametable_mapping();
            break;
        case 0x5106:
            set_fill_mode_tile(value);
            break;
        case 0x5107:
            set_fill_mode_color(value & 0x03);
            break;
        case 0x5130:
            chr_upper_bits_ = value & 0x03;
            break;
        case 0x5200:
            vertical_split_enabled_ = (value & 0x80) != 0;
            vertical_split_right_side_ = (value & 0x40) != 0;
            vertical_split_delimiter_tile_ = value & 0x1F;
            break;
        case 0x5201:
            vertical_split_scroll_ = value;
            break;
        case 0x5202:
            vertical_split_bank_ = value;
            break;
        case 0x5203:
            irq_counter_target_ = value;
            break;
        case 0x5204:
            irq_enabled_ = (value & 0x80) != 0;
            if (!irq_enabled_) {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            } else if (irq_pending_) {
                console_->get_cpu()->set_irq_source(IRQSource::EXTERNAL);
            }
            break;
        case 0x5205:
            multiplier_value1_ = value;
            break;
        case 0x5206:
            multiplier_value2_ = value;
            break;
        default:
            break;
    }
}

uint8_t Mapper005::read_register(uint16_t addr) {
    switch (addr) {
        case 0x5010:
            return pcm_irq_pending_ ? 0x80 : 0x00;
        case 0x5015: {
            uint8_t status = 0;
            status |= mmc5_sq_len_[0] > 0 ? 0x01 : 0x00;
            status |= mmc5_sq_len_[1] > 0 ? 0x02 : 0x00;
            return status;
        }
        case 0x5204: {
            const uint8_t v = (ppu_in_frame_ ? 0x40 : 0x00) | (irq_pending_ ? 0x80 : 0x00);
            irq_pending_ = false;
            if (!pcm_irq_pending_) {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            }
            return v;
        }
        case 0x5205:
            return (uint8_t)((multiplier_value1_ * multiplier_value2_) & 0xFF);
        case 0x5206:
            return (uint8_t)((multiplier_value1_ * multiplier_value2_) >> 8);
        case 0xFFFA:
        case 0xFFFB:
            ppu_in_frame_ = false;
            scanline_counter_ = 0;
            irq_pending_ = false;
            if (!pcm_irq_pending_) {
                console_->get_cpu()->clear_irq_source(IRQSource::EXTERNAL);
            }
            update_chr_banks(true);
            return BaseMapper::read_ram(addr);
        default:
            break;
    }
    return console_->get_memory_manager()->get_open_bus();
}

} // namespace ear6::nes
