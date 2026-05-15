#include "nes_console.h"
#include "nes_memory_manager.h"
#include "base_mapper.h"
#include "nes_cpu.h"
#include <cstring>
#include <cstdio>
#include "nes_ppu.h"

namespace ear6::nes {



NesPpu::NesPpu(NesConsole* console) {
    console_ = console;
    mapper_ = console->get_mapper();
    master_clock_ = 0;
    master_clock_divider_ = 4;

    output_buffers_[0] = new uint16_t[256 * 240];
    output_buffers_[1] = new uint16_t[256 * 240];
    current_output_buffer_ = output_buffers_[0];
    memset(output_buffers_[0], 0, 256 * 240 * sizeof(uint16_t));
    memset(output_buffers_[1], 0, 256 * 240 * sizeof(uint16_t));
    framebuffer_ = output_buffers_[0];

    const uint8_t palette_boot[0x20] = {
        0x09,0x01,0x00,0x01,0x00,0x02,0x02,0x0D,0x08,0x10,0x08,0x24,0x00,0x00,0x04,0x2C,
        0x09,0x01,0x34,0x03,0x00,0x04,0x00,0x14,0x08,0x3A,0x00,0x02,0x00,0x20,0x2C,0x08
    };
    memcpy(palette_ram_, palette_boot, sizeof(palette_ram_));
    memset(corrupt_oam_row_, 0, sizeof(corrupt_oam_row_));
    video_ram_addr_ = 0;
    memset(oam_decay_cycles_, 0, sizeof(oam_decay_cycles_));
    reset(false);
}

NesPpu::~NesPpu() {
    delete[] output_buffers_[0];
    delete[] output_buffers_[1];
}

void NesPpu::get_memory_ranges(MemoryRanges& ranges) {
    ranges.add_handler(MemoryOperation::READ, 0x2000, 0x3FFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x2000, 0x3FFF);
    ranges.add_handler(MemoryOperation::WRITE, 0x4014);
}

void NesPpu::reset(bool soft_reset) {
    master_clock_ = 0;
    memset(oam_decay_cycles_, 0, sizeof(oam_decay_cycles_));
    enable_oam_decay_ = false;

    if (soft_reset) return;

    prevent_vbl_flag_ = false;
    need_state_update_ = false;
    prev_rendering_enabled_ = false;
    rendering_enabled_ = false;
    ignore_vram_read_ = 0;
    open_bus_ = 0;
    memset(open_bus_decay_stamp_, 0, sizeof(open_bus_decay_stamp_));
    tmp_video_ram_addr_ = 0;
    high_bit_shift_ = 0;
    low_bit_shift_ = 0;
    sprite_ram_addr_ = 0;
    x_scroll_ = 0;
    write_toggle_ = false;
    control_ = {};
    mask_ = {};
    status_flags_ = {};
    tile_ = {};
    current_tile_palette_ = 0;
    previous_tile_palette_ = 0;
    ppu_bus_address_ = 0;
    intensify_color_bits_ = 0;
    palette_ram_mask_ = 0x3F;
    last_updated_pixel_ = -1;
    oam_copy_buffer_ = 0;
    sprite_in_range_ = false;
    sprite0_added_ = false;
    sprite_addr_h_ = 0;
    sprite_addr_l_ = 0;
    oam_copy_done_ = false;
    memset(has_sprite_, 0, sizeof(has_sprite_));
    for (auto& st : sprite_tiles_) st = NesSpriteInfo{};
    sprite_count_ = 0;
    secondary_oam_addr_ = 0;
    sprite0_visible_ = false;
    sprite_index_ = 0;
    scanline_ = -1;
    cycle_ = 340;
    frame_count_ = 1;
    memory_read_buffer_ = 0;
    overflow_bug_counter_ = 0;
    update_vram_addr_delay_ = 0;
    update_vram_addr_ = 0;
    first_visible_sprite_addr_ = 0;
    last_visible_sprite_addr_ = 0;
    allow_full_ppu_access_ = false;
    update_minimum_draw_cycles();
}

void NesPpu::run(uint64_t run_to) {
    do {
        exec();
        master_clock_ += master_clock_divider_;
    } while (master_clock_ + master_clock_divider_ <= run_to);
}

void NesPpu::exec() {
    if (cycle_ < 340) {
        cycle_++;

        if (scanline_ < 240) {
            process_scanline_impl();
        }
    } else {
        process_scanline_first_cycle();
    }

    if (need_state_update_) {
        update_state();
    }
}

void NesPpu::process_scanline_first_cycle() {
    cycle_ = 0;
    if (++scanline_ > (int)vblank_end_) {
        last_updated_pixel_ = -1;
        scanline_ = -1;
        sprite_count_ = 0;
        if (rendering_enabled_) {
            process_oam_corruption();
        }
        update_minimum_draw_cycles();
    }

    if (scanline_ < 240) {
        if (scanline_ == -1) {
            status_flags_.sprite_overflow = false;
            status_flags_.sprite_zero_hit = false;
            allow_full_ppu_access_ = true;
            current_output_buffer_ = (current_output_buffer_ == output_buffers_[0]) ? output_buffers_[1] : output_buffers_[0];
            framebuffer_ = current_output_buffer_;
        } else if (prev_rendering_enabled_) {
            if (scanline_ > 0 || (!(frame_count_ & 0x01))) {
                set_bus_address((tile_.tile_addr << 4) | (video_ram_addr_ >> 12) | control_.background_pattern_addr);
            }
        }
    } else if (scanline_ == 240) {
        set_bus_address(video_ram_addr_ & 0x3FFF);
        send_frame();
        frame_count_++;
    } else if (scanline_ == 241) {
        if (!prevent_vbl_flag_) {
            status_flags_.vertical_blank = true;
            begin_vblank();
        }
        prevent_vbl_flag_ = false;
    }
}

void NesPpu::process_scanline_impl() {
    if (cycle_ <= 256) {
        load_tile_info();

        if (prev_rendering_enabled_ && (cycle_ & 0x07) == 0) {
            inc_horizontal_scrolling();
            if (cycle_ == 256) {
                inc_vertical_scrolling();
            }
        }

        if (scanline_ >= 0) {
            // DrawPixel
            if (rendering_enabled_ || ((video_ram_addr_ & 0x3F00) != 0x3F00)) {
                uint32_t color = get_pixel_color();
                current_output_buffer_[(scanline_ << 8) + cycle_ - 1] = palette_ram_[color & 0x03 ? color : 0];
            } else {
                current_output_buffer_[(scanline_ << 8) + cycle_ - 1] = palette_ram_[video_ram_addr_ & 0x1F];
            }

            shift_tile_registers();
            process_sprite_evaluation();
        } else if (cycle_ < 9) {
            if (cycle_ == 1) {
                status_flags_.vertical_blank = false;
                console_->get_cpu()->clear_nmi_flag();
            }
            if (sprite_ram_addr_ >= 0x08 && rendering_enabled_) {
                write_sprite_ram(cycle_ - 1, read_sprite_ram((sprite_ram_addr_ & 0xF8) + cycle_ - 1));
            }
        }
    } else if (cycle_ >= 257 && cycle_ <= 320) {
        if (cycle_ == 257) {
            sprite_index_ = 0;
            memset(has_sprite_, 0, sizeof(has_sprite_));
            if (prev_rendering_enabled_) {
                video_ram_addr_ = (video_ram_addr_ & ~0x041F) | (tmp_video_ram_addr_ & 0x041F);
            }
        }
        if (rendering_enabled_) {
            sprite_ram_addr_ = 0;
            switch ((cycle_ - 257) % 8) {
                case 0: read_vram(get_nametable_addr()); break;
                case 2: read_vram(get_attribute_addr()); break;
                case 4: load_sprite_tile_info(); break;
            }
            if (scanline_ == -1 && cycle_ >= 280 && cycle_ <= 304) {
                video_ram_addr_ = (video_ram_addr_ & ~0x7BE0) | (tmp_video_ram_addr_ & 0x7BE0);
            }
            if (cycle_ == 320) {
                load_extra_sprites();
            }
        }
    } else if (cycle_ >= 321 && cycle_ <= 336) {
        load_tile_info();
        if (cycle_ == 321) {
            if (rendering_enabled_) {
                oam_copy_buffer_ = secondary_sprite_ram_[0];
            }
        } else if (prev_rendering_enabled_ && (cycle_ == 328 || cycle_ == 336)) {
            low_bit_shift_ <<= 8;
            high_bit_shift_ <<= 8;
            inc_horizontal_scrolling();
        }
    } else if (cycle_ == 337 || cycle_ == 339) {
        if (rendering_enabled_) {
            tile_.tile_addr = read_vram(get_nametable_addr());
            if (scanline_ == -1 && cycle_ == 339 && (frame_count_ & 0x01)) {
                cycle_ = 340;
            }
        }
    }
}

void NesPpu::load_tile_info() {
    if (rendering_enabled_) {
        switch (cycle_ & 0x07) {
            case 1: {
                previous_tile_palette_ = current_tile_palette_;
                current_tile_palette_ = tile_.palette_offset;
                low_bit_shift_ |= tile_.low_byte;
                high_bit_shift_ |= tile_.high_byte;
                uint8_t tile_index = read_vram(get_nametable_addr());
                tile_.tile_addr = (tile_index << 4) | (video_ram_addr_ >> 12) | control_.background_pattern_addr;
                break;
            }
            case 3: {
                uint8_t shift = ((video_ram_addr_ >> 4) & 0x04) | (video_ram_addr_ & 0x02);
                tile_.palette_offset = ((read_vram(get_attribute_addr()) >> shift) & 0x03) << 2;
                break;
            }
            case 5:
                tile_.low_byte = read_vram(tile_.tile_addr);
                break;
            case 7:
                tile_.high_byte = read_vram(tile_.tile_addr + 8);
                break;
        }
    }
}

void NesPpu::shift_tile_registers() {
    low_bit_shift_ <<= 1;
    high_bit_shift_ <<= 1;
}

uint8_t NesPpu::get_pixel_color() {
    uint8_t offset = x_scroll_;
    uint8_t bg_color = 0;
    uint8_t sprite_bg_color = 0;

    if (cycle_ > minimum_draw_bg_cycle_) {
        sprite_bg_color = (((low_bit_shift_ << offset) & 0x8000) >> 15) | (((high_bit_shift_ << offset) & 0x8000) >> 14);
        if (emulator_bg_enabled_) {
            bg_color = sprite_bg_color;
        }
    }

    if (has_sprite_[cycle_] && cycle_ > minimum_draw_sprite_cycle_) {
        for (uint8_t i = 0; i < sprite_count_; i++) {
            int32_t shift = (int32_t)cycle_ - sprite_tiles_[i].sprite_x - 1;
            if (shift >= 0 && shift < 8) {
                uint8_t sprite_color;
                if (sprite_tiles_[i].horizontal_mirror) {
                    sprite_color = ((sprite_tiles_[i].low_byte >> shift) & 0x01) | (((sprite_tiles_[i].high_byte >> shift) & 0x01) << 1);
                } else {
                    sprite_color = (((sprite_tiles_[i].low_byte << shift) & 0x80) >> 7) | (((sprite_tiles_[i].high_byte << shift) & 0x80) >> 6);
                }

                if (sprite_color != 0) {
                    if (i == 0 && sprite_bg_color != 0 && sprite0_visible_ && cycle_ != 256 && mask_.background_enabled && !status_flags_.sprite_zero_hit && cycle_ > minimum_draw_sprite_standard_cycle_) {
                        status_flags_.sprite_zero_hit = true;
                    }
                    if (emulator_sprites_enabled_ && (bg_color == 0 || !sprite_tiles_[i].background_priority)) {
                        return sprite_tiles_[i].palette_offset + sprite_color;
                    }
                    break;
                }
            }
        }
    }
    return ((offset + ((cycle_ - 1) & 0x07) < 8) ? previous_tile_palette_ : current_tile_palette_) + bg_color;
}

void NesPpu::set_bus_address(uint16_t addr) {
    ppu_bus_address_ = addr;
}

uint8_t NesPpu::read_vram(uint16_t addr) {
    set_bus_address(addr);
    return mapper_->read_vram(addr);
}

void NesPpu::write_vram(uint16_t addr, uint8_t value) {
    set_bus_address(addr);
    mapper_->write_vram(addr, value);
}

uint16_t NesPpu::get_nametable_addr() {
    return 0x2000 | (video_ram_addr_ & 0x0FFF);
}

uint16_t NesPpu::get_attribute_addr() {
    return 0x23C0 | (video_ram_addr_ & 0x0C00) | ((video_ram_addr_ >> 4) & 0x38) | ((video_ram_addr_ >> 2) & 0x07);
}

void NesPpu::inc_horizontal_scrolling() {
    uint16_t addr = video_ram_addr_;
    if ((addr & 0x001F) == 31) {
        addr = (addr & ~0x001F) ^ 0x0400;
    } else {
        addr++;
    }
    video_ram_addr_ = addr;
}

void NesPpu::inc_vertical_scrolling() {
    uint16_t addr = video_ram_addr_;
    if ((addr & 0x7000) != 0x7000) {
        addr += 0x1000;
    } else {
        addr &= ~0x7000;
        int y = (addr & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            addr ^= 0x0800;
        } else if (y == 31) {
            y = 0;
        } else {
            y++;
        }
        addr = (addr & ~0x03E0) | (y << 5);
    }
    video_ram_addr_ = addr;
}

void NesPpu::update_video_ram_addr() {
    if (scanline_ >= 240 || !rendering_enabled_) {
        video_ram_addr_ = (video_ram_addr_ + (control_.vertical_write ? 32 : 1)) & 0x7FFF;
        set_bus_address(video_ram_addr_ & 0x3FFF);
    } else {
        inc_horizontal_scrolling();
        inc_vertical_scrolling();
    }
}

void NesPpu::set_open_bus(uint8_t mask, uint8_t value) {
    if (mask == 0xFF) {
        open_bus_ = value;
        for (int i = 0; i < 8; i++) {
            open_bus_decay_stamp_[i] = frame_count_;
        }
    } else {
        uint16_t ob = (open_bus_ << 8);
        for (int i = 0; i < 8; i++) {
            ob >>= 1;
            if (mask & 0x01) {
                if (value & 0x01) ob |= 0x80;
                else ob &= 0xFF7F;
                open_bus_decay_stamp_[i] = frame_count_;
            } else if (frame_count_ - open_bus_decay_stamp_[i] > 3) {
                ob &= 0xFF7F;
            }
            value >>= 1;
            mask >>= 1;
        }
        open_bus_ = (uint8_t)ob;
    }
}

uint8_t NesPpu::apply_open_bus(uint8_t mask, uint8_t value) {
    set_open_bus(~mask, value);
    return value | (open_bus_ & mask);
}

void NesPpu::process_status_reg_open_bus(uint8_t& open_bus_mask, uint8_t& return_value) {
    (void)open_bus_mask;
    (void)return_value;
}

uint8_t NesPpu::read_ram(uint16_t addr) {
    uint8_t open_bus_mask = 0xFF;
    uint8_t return_value = 0;

    switch (addr & 0x7) {
        case 0x02: { // Status
            write_toggle_ = false;
            return_value = (
                ((uint8_t)status_flags_.sprite_overflow << 5) |
                ((uint8_t)status_flags_.sprite_zero_hit << 6) |
                ((uint8_t)status_flags_.vertical_blank << 7)
            );
            update_status_flag();
            open_bus_mask = 0x1F;
            process_status_reg_open_bus(open_bus_mask, return_value);
            break;
        }
        case 0x04: { // OAMDATA
            if (scanline_ <= 239 && rendering_enabled_) {
                if (cycle_ >= 257 && cycle_ <= 320) {
                    uint8_t step = ((cycle_ - 257) % 8) > 3 ? 3 : ((cycle_ - 257) % 8);
                    secondary_oam_addr_ = (cycle_ - 257) / 8 * 4 + step;
                    oam_copy_buffer_ = secondary_sprite_ram_[secondary_oam_addr_];
                }
                return_value = oam_copy_buffer_;
            } else {
                return_value = read_sprite_ram(sprite_ram_addr_);
            }
            open_bus_mask = 0x00;
            break;
        }
        case 0x07: { // PPUDATA
            if (ignore_vram_read_) {
                open_bus_mask = 0xFF;
            } else {
                return_value = memory_read_buffer_;
                memory_read_buffer_ = read_vram(ppu_bus_address_ & 0x3FFF);

                if ((ppu_bus_address_ & 0x3FFF) >= 0x3F00) {
                    uint8_t pa = ppu_bus_address_ & 0x1F;
                    if (pa == 0x10 || pa == 0x14 || pa == 0x18 || pa == 0x1C) pa &= ~0x10;
                    return_value = (palette_ram_[pa] & palette_ram_mask_) | (open_bus_ & 0xC0);
                    open_bus_mask = 0xC0;
                } else {
                    open_bus_mask = 0x00;
                }
                ignore_vram_read_ = 6;
                need_state_update_ = true;
                need_video_ram_increment_ = true;
            }
            break;
        }
        default:
            break;
    }
    return apply_open_bus(open_bus_mask, return_value);
}

uint8_t NesPpu::peek_ram(uint16_t addr) {
    uint8_t open_bus_mask = 0xFF;
    uint8_t return_value = 0;
    switch (addr & 0x7) {
        case 0x02:
            return_value = ((uint8_t)status_flags_.sprite_overflow << 5) | ((uint8_t)status_flags_.sprite_zero_hit << 6) | ((uint8_t)status_flags_.vertical_blank << 7);
            open_bus_mask = 0x1F;
            break;
        case 0x04:
            return_value = sprite_ram_[sprite_ram_addr_];
            open_bus_mask = 0x00;
            break;
        case 0x07:
            return_value = memory_read_buffer_;
            if ((video_ram_addr_ & 0x3FFF) >= 0x3F00) {
                uint8_t pa = video_ram_addr_ & 0x1F;
                if (pa == 0x10 || pa == 0x14 || pa == 0x18 || pa == 0x1C) pa &= ~0x10;
                return_value = (palette_ram_[pa] & palette_ram_mask_) | (open_bus_ & 0xC0);
                open_bus_mask = 0xC0;
            } else {
                open_bus_mask = 0x00;
            }
            break;
        default: break;
    }
    return return_value | (open_bus_ & open_bus_mask);
}

void NesPpu::write_ram(uint16_t addr, uint8_t value) {
    if (addr == 0x4014) {
        set_open_bus(0xFF, value);
        console_->get_cpu()->run_dma_transfer(value);
        return;
    }

    if ((addr & 0x7) != 0x00) {
        set_open_bus(0xFF, value);
    }

    switch (addr & 0x7) {
        case 0x00: // PPUCTRL
            set_control_register(value);
            break;
        case 0x01: // PPUMASK
            set_mask_register(value);
            break;
        case 0x03: // OAMADDR
            sprite_ram_addr_ = value;
            break;
        case 0x04: // OAMDATA
            if ((scanline_ >= 240) || !rendering_enabled_) {
                if ((sprite_ram_addr_ & 0x03) == 0x02) value &= 0xE3;
                write_sprite_ram(sprite_ram_addr_, value);
                sprite_ram_addr_ = (sprite_ram_addr_ + 1) & 0xFF;
            } else {
                sprite_ram_addr_ = (sprite_ram_addr_ + 4) & 0xFF;
            }
            break;
        case 0x05: // PPUSCROLL
            if (write_toggle_) {
                tmp_video_ram_addr_ = (tmp_video_ram_addr_ & ~0x73E0) | ((value & 0xF8) << 2) | ((value & 0x07) << 12);
            } else {
                x_scroll_ = value & 0x07;
                uint16_t new_addr = (tmp_video_ram_addr_ & ~0x001F) | (value >> 3);
                process_tmp_addr_scroll_glitch(new_addr, console_->get_memory_manager()->get_open_bus() >> 3, 0x001F);
            }
            write_toggle_ = !write_toggle_;
            break;
        case 0x06: // PPUADDR
            if (write_toggle_) {
                tmp_video_ram_addr_ = (tmp_video_ram_addr_ & ~0x00FF) | value;
                need_state_update_ = true;
                update_vram_addr_delay_ = 3;
                update_vram_addr_ = tmp_video_ram_addr_;
            } else {
                uint16_t new_addr = (tmp_video_ram_addr_ & ~0xFF00) | ((value & 0x3F) << 8);
                process_tmp_addr_scroll_glitch(new_addr, console_->get_memory_manager()->get_open_bus() << 8, 0x0C00);
            }
            write_toggle_ = !write_toggle_;
            break;
        case 0x07: // PPUDATA
            if ((ppu_bus_address_ & 0x3FFF) >= 0x3F00) {
                uint8_t pa = ppu_bus_address_ & 0x1F;
                if (pa == 0x00 || pa == 0x10) { palette_ram_[0x00] = value & 0x3F; palette_ram_[0x10] = value & 0x3F; }
                else if (pa == 0x04 || pa == 0x14) { palette_ram_[0x04] = value & 0x3F; palette_ram_[0x14] = value & 0x3F; }
                else if (pa == 0x08 || pa == 0x18) { palette_ram_[0x08] = value & 0x3F; palette_ram_[0x18] = value & 0x3F; }
                else if (pa == 0x0C || pa == 0x1C) { palette_ram_[0x0C] = value & 0x3F; palette_ram_[0x1C] = value & 0x3F; }
                else { palette_ram_[pa] = value & 0x3F; }
            } else {
                if (scanline_ >= 240 || !rendering_enabled_) {
                    write_vram(ppu_bus_address_ & 0x3FFF, value);
                } else {
                    write_vram(ppu_bus_address_ & 0x3FFF, ppu_bus_address_ & 0xFF);
                }
            }
            need_state_update_ = true;
            need_video_ram_increment_ = true;
            break;
    }
}

void NesPpu::set_control_register(uint8_t value) {
    uint8_t name_table = value & 0x03;
    uint16_t normal_addr = (tmp_video_ram_addr_ & ~0x0C00) | (name_table << 10);
    process_tmp_addr_scroll_glitch(normal_addr, console_->get_memory_manager()->get_open_bus() << 10, 0x0400);

    control_.vertical_write = (value & 0x04) != 0;
    control_.sprite_pattern_addr = (value & 0x08) ? 0x1000 : 0x0000;
    control_.background_pattern_addr = (value & 0x10) ? 0x1000 : 0x0000;
    control_.large_sprites = (value & 0x20) != 0;
    control_.nmi_on_vertical_blank = (value & 0x80) != 0;

    if (!control_.nmi_on_vertical_blank) {
        console_->get_cpu()->clear_nmi_flag();
    } else if (control_.nmi_on_vertical_blank && status_flags_.vertical_blank) {
        console_->get_cpu()->set_nmi_flag();
    }
}

void NesPpu::set_mask_register(uint8_t value) {
    mask_.grayscale = (value & 0x01) != 0;
    mask_.background_mask = (value & 0x02) != 0;
    mask_.sprite_mask = (value & 0x04) != 0;
    mask_.background_enabled = (value & 0x08) != 0;
    mask_.sprites_enabled = (value & 0x10) != 0;
    mask_.intensify_red = (value & 0x20) != 0;
    mask_.intensify_green = (value & 0x40) != 0;
    mask_.intensify_blue = (value & 0x80) != 0;

    if (rendering_enabled_ != (mask_.background_enabled | mask_.sprites_enabled)) {
        need_state_update_ = true;
    }

    update_minimum_draw_cycles();
    update_grayscale_and_intensify_bits();
}

void NesPpu::process_tmp_addr_scroll_glitch(uint16_t normal_addr, uint16_t value, uint16_t mask) {
    tmp_video_ram_addr_ = normal_addr;
    if (cycle_ == 257 && scanline_ < 240 && rendering_enabled_) {
        video_ram_addr_ = (video_ram_addr_ & ~mask) | (value & mask);
    }
}

void NesPpu::update_status_flag() {
    status_flags_.vertical_blank = false;
    console_->get_cpu()->clear_nmi_flag();
    if (scanline_ == (int)nmi_scanline_ && cycle_ == 0) {
        prevent_vbl_flag_ = true;
    }
}

void NesPpu::begin_vblank() {
    trigger_nmi();
}

void NesPpu::trigger_nmi() {
    if (control_.nmi_on_vertical_blank) {
        console_->get_cpu()->set_nmi_flag();
    }
}

void NesPpu::send_frame() {
    update_grayscale_and_intensify_bits();
    // Frame is ready in current_output_buffer_
}

void NesPpu::update_state() {
    need_state_update_ = false;

    if (prev_rendering_enabled_ != rendering_enabled_) {
        prev_rendering_enabled_ = rendering_enabled_;
        if (scanline_ < 240) {
            if (prev_rendering_enabled_) {
                process_oam_corruption();
            } else {
                set_oam_corruption_flags();
                set_bus_address(video_ram_addr_ & 0x3FFF);
                if (cycle_ >= 65 && cycle_ <= 256) {
                    sprite_ram_addr_++;
                    sprite_addr_h_ = (sprite_ram_addr_ >> 2) & 0x3F;
                    sprite_addr_l_ = sprite_ram_addr_ & 0x03;
                }
            }
        }
    }

    if (rendering_enabled_ != (mask_.background_enabled | mask_.sprites_enabled)) {
        rendering_enabled_ = mask_.background_enabled | mask_.sprites_enabled;
        need_state_update_ = true;
    }

    if (update_vram_addr_delay_ > 0) {
        update_vram_addr_delay_--;
        if (update_vram_addr_delay_ == 0) {
            if (scanline_ < 240 && rendering_enabled_) {
                if (cycle_ == 257) {
                    video_ram_addr_ &= update_vram_addr_;
                } else if (cycle_ > 0 && (cycle_ & 0x07) == 0 && (cycle_ <= 256 || cycle_ > 320)) {
                    video_ram_addr_ = (update_vram_addr_ & ~0x41F) | (video_ram_addr_ & update_vram_addr_ & 0x41F);
                } else {
                    video_ram_addr_ = update_vram_addr_;
                }
            } else {
                video_ram_addr_ = update_vram_addr_;
            }
            tmp_video_ram_addr_ = video_ram_addr_;

            if (scanline_ >= 240 || !rendering_enabled_) {
                set_bus_address(video_ram_addr_ & 0x3FFF);
            }
        } else {
            need_state_update_ = true;
        }
    }

    if (ignore_vram_read_ > 0) {
        ignore_vram_read_--;
        if (ignore_vram_read_ > 0) need_state_update_ = true;
    }

    if (need_video_ram_increment_) {
        need_video_ram_increment_ = false;
        update_video_ram_addr();
    }
}

void NesPpu::process_sprite_evaluation_start() {
    sprite0_added_ = false;
    sprite_in_range_ = false;
    secondary_oam_addr_ = 0;
    overflow_bug_counter_ = 0;
    oam_copy_done_ = false;
    sprite_addr_h_ = (sprite_ram_addr_ >> 2) & 0x3F;
    sprite_addr_l_ = sprite_ram_addr_ & 0x03;
    first_visible_sprite_addr_ = sprite_addr_h_ * 4;
    last_visible_sprite_addr_ = first_visible_sprite_addr_;
}

void NesPpu::process_sprite_evaluation_end() {
    sprite0_visible_ = sprite0_added_;
    sprite_count_ = ((secondary_oam_addr_ + 3) >> 2);
}

void NesPpu::process_sprite_evaluation() {
    if (rendering_enabled_) {
        if (cycle_ < 65) {
            oam_copy_buffer_ = 0xFF;
            secondary_sprite_ram_[(cycle_ - 1) >> 1] = 0xFF;
        } else {
            if (cycle_ & 0x01) {
                if (cycle_ == 65) {
                    process_sprite_evaluation_start();
                }
                oam_copy_buffer_ = read_sprite_ram(sprite_ram_addr_);
            } else {
                if (cycle_ == 256) {
                    process_sprite_evaluation_end();
                }

                if (oam_copy_done_) {
                    sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                    if (secondary_oam_addr_ >= 0x20) {
                        oam_copy_buffer_ = secondary_sprite_ram_[secondary_oam_addr_ & 0x1F];
                    }
                } else {
                    if (!sprite_in_range_ && scanline_ >= (int)oam_copy_buffer_ && scanline_ < (int)oam_copy_buffer_ + (control_.large_sprites ? 16 : 8)) {
                        sprite_in_range_ = !oam_copy_done_;
                    }

                    if (secondary_oam_addr_ < 0x20) {
                        secondary_sprite_ram_[secondary_oam_addr_] = oam_copy_buffer_;

                        if (sprite_in_range_) {
                            if (cycle_ == 66) {
                                sprite0_added_ = true;
                            }
                            sprite_addr_l_++;
                            secondary_oam_addr_++;

                            if (sprite_addr_l_ >= 4) {
                                sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                                sprite_addr_l_ = 0;
                                if (sprite_addr_h_ == 0) oam_copy_done_ = true;
                            }

                            if ((secondary_oam_addr_ & 0x03) == 0) {
                                sprite_in_range_ = false;
                                last_visible_sprite_addr_ = (sprite_addr_h_ - 1) * 4;
                                if (sprite_addr_l_ != 0) {
                                    bool in_rng = (scanline_ >= (int)oam_copy_buffer_ && scanline_ < (int)oam_copy_buffer_ + (control_.large_sprites ? 16 : 8));
                                    if (!in_rng) sprite_addr_l_ = 0;
                                }
                            }
                        } else {
                            sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                            sprite_addr_l_ = 0;
                            if (sprite_addr_h_ == 0) oam_copy_done_ = true;
                        }
                    } else {
                        oam_copy_buffer_ = secondary_sprite_ram_[secondary_oam_addr_ & 0x1F];

                        if (oam_copy_done_) {
                            sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                            sprite_addr_l_ = 0;
                        } else if (sprite_in_range_) {
                            status_flags_.sprite_overflow = true;
                            sprite_addr_l_++;
                            if (sprite_addr_l_ == 4) {
                                sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                                sprite_addr_l_ = 0;
                            }
                            if (overflow_bug_counter_ == 0) {
                                overflow_bug_counter_ = 3;
                            } else if (overflow_bug_counter_ > 0) {
                                overflow_bug_counter_--;
                                if (overflow_bug_counter_ == 0) {
                                    oam_copy_done_ = true;
                                    sprite_addr_l_ = 0;
                                }
                            }
                        } else {
                            sprite_addr_h_ = (sprite_addr_h_ + 1) & 0x3F;
                            sprite_addr_l_ = (sprite_addr_l_ + 1) & 0x03;
                            if (sprite_addr_h_ == 0) oam_copy_done_ = true;
                        }
                    }
                }
                sprite_ram_addr_ = (sprite_addr_l_ & 0x03) | (sprite_addr_h_ << 2);
            }
        }
    }
}

void NesPpu::load_sprite(uint8_t sprite_y, uint8_t tile_index, uint8_t attributes, uint8_t sprite_x, bool extra_sprite) {
    bool bg_priority = (attributes & 0x20) != 0;
    bool h_mirror = (attributes & 0x40) != 0;
    bool v_mirror = (attributes & 0x80) != 0;

    uint16_t tile_addr;
    uint8_t line_offset;
    if (v_mirror) {
        line_offset = (control_.large_sprites ? 15 : 7) - (scanline_ - sprite_y);
    } else {
        line_offset = scanline_ - sprite_y;
    }

    if (control_.large_sprites) {
        tile_addr = (((tile_index & 0x01) ? 0x1000 : 0x0000) | ((tile_index & ~0x01) << 4)) + (line_offset >= 8 ? line_offset + 8 : line_offset);
    } else {
        tile_addr = (tile_index << 4) | control_.sprite_pattern_addr | line_offset;
    }

    bool fetch_last = true;
    if ((sprite_index_ < sprite_count_ || extra_sprite) && sprite_y < 240) {
        NesSpriteInfo& info = sprite_tiles_[sprite_index_];
        info.background_priority = bg_priority;
        info.horizontal_mirror = h_mirror;
        info.palette_offset = ((attributes & 0x03) << 2) | 0x10;
        if (extra_sprite) {
            info.low_byte = mapper_->read_vram(tile_addr);
            info.high_byte = mapper_->read_vram(tile_addr + 8);
        } else {
            fetch_last = false;
            info.low_byte = read_vram(tile_addr);
            info.high_byte = read_vram(tile_addr + 8);
        }
        info.sprite_x = sprite_x;

        if (scanline_ >= 0) {
            for (int i = 0; i < 8 && sprite_x + i + 1 < 257; i++) {
                has_sprite_[sprite_x + i + 1] = true;
            }
        }
    }

    if (fetch_last) {
        line_offset = 0;
        tile_index = 0xFF;
        if (control_.large_sprites) {
            tile_addr = (((tile_index & 0x01) ? 0x1000 : 0x0000) | ((tile_index & ~0x01) << 4)) + (line_offset >= 8 ? line_offset + 8 : line_offset);
        } else {
            tile_addr = (tile_index << 4) | control_.sprite_pattern_addr | line_offset;
        }
        read_vram(tile_addr);
        read_vram(tile_addr + 8);
    }

    sprite_index_++;
}

void NesPpu::load_sprite_tile_info() {
    uint8_t* sprite_addr = secondary_sprite_ram_ + sprite_index_ * 4;
    load_sprite(*sprite_addr, *(sprite_addr + 1), *(sprite_addr + 2), *(sprite_addr + 3), false);
}

void NesPpu::load_extra_sprites() {
    (void)0; // Simplified - extra sprite loading would go here
}

uint8_t NesPpu::read_sprite_ram(uint8_t addr) {
    if (!enable_oam_decay_) {
        return sprite_ram_[addr];
    }
    uint64_t elapsed = console_->get_cpu()->get_cycle_count() - oam_decay_cycles_[addr >> 3];
    if (elapsed <= 3000) {
        oam_decay_cycles_[addr >> 3] = console_->get_cpu()->get_cycle_count();
    } else {
        for (int i = 0; i < 8; i++) {
            int sa = (addr & 0xF8) | i;
            sprite_ram_[sa] = (sa & 0x03) == 0x02 ? (sa & 0xE3) : sa;
        }
    }
    return sprite_ram_[addr];
}

void NesPpu::write_sprite_ram(uint8_t addr, uint8_t value) {
    sprite_ram_[addr] = value;
    if (enable_oam_decay_) {
        oam_decay_cycles_[addr >> 3] = console_->get_cpu()->get_cycle_count();
    }
}

void NesPpu::set_oam_corruption_flags() {
    if (cycle_ < 64) {
        corrupt_oam_row_[cycle_ >> 1] = true;
    } else if (cycle_ >= 256 && cycle_ < 320) {
        uint8_t base = (cycle_ - 256) >> 3;
        uint8_t offset = (cycle_ - 256) & 0x07;
        if (offset > 3) offset = 3;
        corrupt_oam_row_[base * 4 + offset] = true;
    }
}

void NesPpu::process_oam_corruption() {
    for (int i = 0; i < 32; i++) {
        if (corrupt_oam_row_[i]) {
            if (i > 0) {
                memcpy(sprite_ram_ + i * 8, sprite_ram_, 8);
            }
            corrupt_oam_row_[i] = false;
        }
    }
}

void NesPpu::update_grayscale_and_intensify_bits() {
    update_color_bit_masks();
}

void NesPpu::update_color_bit_masks() {
    palette_ram_mask_ = mask_.grayscale ? 0x30 : 0x3F;
    intensify_color_bits_ = (mask_.intensify_red ? 0x40 : 0x00) | (mask_.intensify_green ? 0x80 : 0x00) | (mask_.intensify_blue ? 0x100 : 0x00);
}

void NesPpu::update_minimum_draw_cycles() {
    minimum_draw_bg_cycle_ = mask_.background_enabled ? (mask_.background_mask ? 0 : 8) : 300;
    minimum_draw_sprite_cycle_ = mask_.sprites_enabled ? (mask_.sprite_mask ? 0 : 8) : 300;
    minimum_draw_sprite_standard_cycle_ = mask_.sprites_enabled ? (mask_.sprite_mask ? 0 : 8) : 300;
    emulator_bg_enabled_ = true;
    emulator_sprites_enabled_ = true;
}

} // namespace ear6::nes
