#include "../state_stream.h"
#include "apu_envelope.h"
#include "apu_frame_counter.h"
#include "apu_length_counter.h"
#include "apu_timer.h"
#include "base_mapper.h"
#include "delta_modulation_channel.h"
#include "mapper_000.h"
#include "mapper_001.h"
#include "mapper_002.h"
#include "mapper_004.h"
#include "mapper_005.h"
#include "mapper_006.h"
#include "mapper_009.h"
#include "mapper_016.h"
#include "mapper_018.h"
#include "mapper_019.h"
#include "mapper_032.h"
#include "mapper_035.h"
#include "mapper_041.h"
#include "mapper_045.h"
#include "mapper_060.h"
#include "mapper_064.h"
#include "mapper_065.h"
#include "mapper_067.h"
#include "mapper_068.h"
#include "mapper_069.h"
#include "mapper_072.h"
#include "mapper_073.h"
#include "mapper_075.h"
#include "mapper_090.h"
#include "mapper_092.h"
#include "mapper_099.h"
#include "mapper_112.h"
#include "mapper_117.h"
#include "mapper_156.h"
#include "mapper_170.h"
#include "mapper_202.h"
#include "mapper_221.h"
#include "mapper_226.h"
#include "mapper_230.h"
#include "mapper_233.h"
#include "mapper_252.h"
#include "mapper_40.h"
#include "mapper_42.h"
#include "mapper_43.h"
#include "mapper_46.h"
#include "mapper_50.h"
#include "mapper_57.h"
#include "mapper_namco108.h"
#include "mapper_sachen_74ls374n.h"
#include "mapper_vrc2_4.h"
#include "mapper_vrc6.h"
#include "nes_apu.h"
#include "nes_console.h"
#include "nes_control_manager.h"
#include "nes_cpu.h"
#include "nes_memory_manager.h"
#include "nes_ppu.h"
#include "nes_sound_mixer.h"
#include "noise_channel.h"
#include "square_channel.h"
#include "triangle_channel.h"
#include "vs_control_manager.h"
#include "../blip_buf.h"

#include <cstdint>
#include <cstring>

namespace ear6::nes {

namespace {

void sync_cpu_state(StateStream& s, NesCpuState& state) {
    s.sync(state.cycle_count);
    s.sync(state.pc);
    s.sync(state.sp);
    s.sync(state.a);
    s.sync(state.x);
    s.sync(state.y);
    s.sync(state.ps);
    s.sync(state.irq_flag);
    s.sync(state.nmi_flag);
}

void sync_tile(StateStream& s, TileInfo& tile) {
    s.sync(tile.tile_addr);
    s.sync(tile.low_byte);
    s.sync(tile.high_byte);
    s.sync(tile.palette_offset);
}

void sync_sprite(StateStream& s, NesSpriteInfo& sprite) {
    s.sync(sprite.sprite_x);
    s.sync(sprite.low_byte);
    s.sync(sprite.high_byte);
    s.sync(sprite.palette_offset);
    s.sync(sprite.horizontal_mirror);
    s.sync(sprite.background_priority);
}

void sync_control(StateStream& s, PpuControlFlags& control) {
    s.sync(control.background_pattern_addr);
    s.sync(control.sprite_pattern_addr);
    s.sync(control.vertical_write);
    s.sync(control.large_sprites);
    s.sync(control.nmi_on_vertical_blank);
}

void sync_mask(StateStream& s, PpuMaskFlags& mask) {
    s.sync(mask.grayscale);
    s.sync(mask.background_mask);
    s.sync(mask.sprite_mask);
    s.sync(mask.background_enabled);
    s.sync(mask.sprites_enabled);
    s.sync(mask.intensify_red);
    s.sync(mask.intensify_green);
    s.sync(mask.intensify_blue);
}

void sync_status(StateStream& s, PPUStatusFlags& status) {
    s.sync(status.sprite_overflow);
    s.sync(status.sprite_zero_hit);
    s.sync(status.vertical_blank);
}

bool locate_vector_memory(
    uint8_t* pointer, const std::vector<uint8_t>& values, uint32_t& offset) {
    if (!pointer || values.empty()) return false;
    uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    uintptr_t begin = reinterpret_cast<uintptr_t>(values.data());
    if (address < begin || address >= begin + values.size()) return false;
    offset = static_cast<uint32_t>(address - begin);
    return true;
}

template<typename T, size_t Size>
void sync_std_array(StateStream& s, std::array<T, Size>& values) {
    for (T& value : values) s.sync(value);
}

template<typename T>
void sync_fixed_vector(StateStream& s, std::vector<T>& values) {
    uint64_t size = values.size();
    s.sync(size);
    if (s.has_error()) return;
    if (s.is_loading() && size != values.size()) {
        s.fail();
        return;
    }
    for (T& value : values) s.sync(value);
}

template<typename T>
void sync_optional_fixed_vector(
    StateStream& s,
    std::vector<T>& values,
    uint64_t fixed_size
) {
    uint64_t size = values.size();
    s.sync(size);
    if (s.has_error()) return;
    if (s.is_loading()) {
        if (size != 0 && size != fixed_size) {
            s.fail();
            return;
        }
        values.resize(static_cast<size_t>(size));
    }
    for (T& value : values) s.sync(value);
}

} // namespace

void NesCpu::serialize(StateStream& s) {
    s.sync(master_clock_);
    s.sync(ppu_offset_);
    s.sync(start_clock_count_);
    s.sync(end_clock_count_);
    s.sync(operand_);
    s.sync(inst_addr_mode_);
    s.sync(need_halt_);
    s.sync(sprite_dma_transfer_);
    s.sync(dmc_dma_running_);
    s.sync(abort_dmc_dma_);
    s.sync(need_dummy_read_);
    s.sync(sprite_dma_offset_);
    s.sync(cpu_write_);
    s.sync(irq_mask_);
    sync_cpu_state(s, state_);
    s.sync(prev_run_irq_);
    s.sync(run_irq_);
    s.sync(prev_nmi_flag_);
    s.sync(prev_need_nmi_);
    s.sync(need_nmi_);
    s.sync(is_dmc_dma_read_);
}

void NesPpu::serialize(StateStream& s) {
    s.sync(master_clock_);
    s.sync(master_clock_divider_);
    s.sync(cycle_);
    s.sync(scanline_);
    s.sync(frame_count_);
    s.sync(nmi_scanline_);
    s.sync(vblank_end_);
    s.sync(no_odd_frame_skip_);

    uint8_t current_buffer = current_output_buffer_ == output_buffers_[1] ? 1 : 0;
    uint8_t frame_buffer = framebuffer_ == output_buffers_[1] ? 1 : 0;
    s.sync(current_buffer);
    s.sync(frame_buffer);
    s.sync_span(output_buffers_[0], 256 * 240);
    s.sync_span(output_buffers_[1], 256 * 240);
    s.sync(last_updated_pixel_);

    s.sync(video_ram_addr_);
    s.sync(tmp_video_ram_addr_);
    s.sync(high_bit_shift_);
    s.sync(low_bit_shift_);
    s.sync(sprite_ram_addr_);
    s.sync(x_scroll_);
    s.sync(open_bus_);
    s.sync_array(open_bus_decay_stamp_);
    s.sync(enable_oam_decay_);
    s.sync(need_state_update_);
    s.sync(rendering_enabled_);
    s.sync(prev_rendering_enabled_);
    s.sync(sprite0_visible_);
    s.sync(sprite_count_);
    s.sync(secondary_oam_addr_);
    s.sync(oam_copy_buffer_);
    s.sync(sprite_in_range_);
    s.sync(sprite0_added_);
    s.sync(sprite_addr_h_);
    s.sync(sprite_addr_l_);
    s.sync(overflow_bug_counter_);
    s.sync(oam_copy_done_);
    s.sync(minimum_draw_bg_cycle_);
    s.sync(minimum_draw_sprite_cycle_);
    s.sync(minimum_draw_sprite_standard_cycle_);

    s.sync_array(palette_ram_);
    s.sync_array(sprite_ram_);
    s.sync_array(secondary_sprite_ram_);
    sync_tile(s, tile_);
    s.sync(ppu_bus_address_);
    s.sync(current_tile_palette_);
    s.sync(previous_tile_palette_);
    s.sync(intensify_color_bits_);
    s.sync(palette_ram_mask_);
    s.sync(update_vram_addr_delay_);

    s.sync(sprite_index_);
    s.sync_array(has_sprite_);
    for (NesSpriteInfo& sprite : sprite_tiles_) sync_sprite(s, sprite);
    sync_control(s, control_);
    sync_mask(s, mask_);
    sync_status(s, status_flags_);

    s.sync(update_vram_addr_);
    s.sync(prevent_vbl_flag_);
    s.sync(write_toggle_);
    s.sync(need_video_ram_increment_);
    s.sync(allow_full_ppu_access_);
    s.sync(memory_read_buffer_);
    s.sync(ignore_vram_read_);
    s.sync_array(oam_decay_cycles_);
    s.sync_array(corrupt_oam_row_);
    s.sync(first_visible_sprite_addr_);
    s.sync(last_visible_sprite_addr_);
    s.sync(emulator_bg_enabled_);
    s.sync(emulator_sprites_enabled_);

    if (s.is_loading()) {
        if (current_buffer > 1 || frame_buffer > 1) {
            s.fail();
            return;
        }
        current_output_buffer_ = output_buffers_[current_buffer];
        framebuffer_ = output_buffers_[frame_buffer];
    }
}

void ApuLengthCounter::serialize(StateStream& s) {
    s.sync(channel_);
    s.sync(new_halt_value_);
    s.sync(enabled_);
    s.sync(halt_);
    s.sync(counter_);
    s.sync(reload_value_);
    s.sync(previous_value_);
}

void ApuEnvelope::serialize(StateStream& s) {
    length_counter.serialize(s);
    s.sync(constant_volume_);
    s.sync(volume_);
    s.sync(start_);
    s.sync(divider_);
    s.sync(counter_);
}

void ApuTimer::serialize(StateStream& s) {
    s.sync(previous_cycle_);
    s.sync(timer_);
    s.sync(period_);
    s.sync(last_output_);
    s.sync(channel_);
}

void SquareChannel::serialize(StateStream& s) {
    envelope_.serialize(s);
    timer_.serialize(s);
    s.sync(channel_);
    s.sync(is_channel1_);
    s.sync(duty_);
    s.sync(duty_pos_);
    s.sync(sweep_enabled_);
    s.sync(sweep_period_);
    s.sync(sweep_negate_);
    s.sync(sweep_shift_);
    s.sync(reload_sweep_);
    s.sync(sweep_divider_);
    s.sync(sweep_target_period_);
    s.sync(real_period_);
}

void TriangleChannel::serialize(StateStream& s) {
    length_counter_.serialize(s);
    timer_.serialize(s);
    s.sync(linear_counter_);
    s.sync(linear_counter_reload_);
    s.sync(linear_reload_flag_);
    s.sync(linear_control_flag_);
    s.sync(sequence_position_);
}

void NoiseChannel::serialize(StateStream& s) {
    envelope_.serialize(s);
    timer_.serialize(s);
    s.sync(shift_register_);
    s.sync(mode_flag_);
}

void DeltaModulationChannel::serialize(StateStream& s) {
    timer_.serialize(s);
    s.sync(sample_addr_);
    s.sync(sample_length_);
    s.sync(output_level_);
    s.sync(irq_enabled_);
    s.sync(loop_flag_);
    s.sync(current_addr_);
    s.sync(bytes_remaining_);
    s.sync(read_buffer_);
    s.sync(buffer_empty_);
    s.sync(shift_register_);
    s.sync(bits_remaining_);
    s.sync(silence_flag_);
    s.sync(need_to_run_flag_);
    s.sync(disable_delay_);
    s.sync(transfer_start_delay_);
}

void ApuFrameCounter::serialize(StateStream& s) {
    for (auto& mode : step_cycles_) s.sync_array(mode);
    s.sync(previous_cycle_);
    s.sync(current_step_);
    s.sync(step_mode_);
    s.sync(inhibit_irq_);
    s.sync(block_frame_counter_tick_);
    s.sync(new_value_);
    s.sync(write_delay_counter_);
    s.sync(irq_flag_);
    s.sync(irq_flag_clear_clock_);
}

void NesSoundMixer::serialize(StateStream& s) {
    s.sync(previous_output_left_);
    s.sync_vector(timestamps_);
    for (auto& channel : channel_output_) s.sync_array(channel);
    s.sync_array(current_output_);
    s.sync(initialized_);
    s.sync(clock_rate_);

    uint64_t blip_factor = blip_get_factor(blip_buf_left_);
    uint64_t blip_offset = blip_get_offset(blip_buf_left_);
    int32_t blip_avail = blip_get_avail(blip_buf_left_);
    int32_t blip_integrator = blip_get_integrator(blip_buf_left_);
    int32_t blip_buffer_count = blip_get_size(blip_buf_left_) + 18;
    std::vector<int32_t> blip_buffer;
    if (s.is_saving()) {
        const int* source = blip_get_buffer(blip_buf_left_);
        blip_buffer.assign(source, source + blip_buffer_count);
    }
    s.sync(blip_factor);
    s.sync(blip_offset);
    s.sync(blip_avail);
    s.sync(blip_integrator);
    s.sync(blip_buffer_count);
    s.sync_vector(blip_buffer);

    uint64_t sample_count = sample_count_;
    s.sync(sample_count);
    if (s.is_loading()) {
        if (sample_count > output_buffer_size_ / 2) {
            s.fail();
            return;
        }
        sample_count_ = static_cast<size_t>(sample_count);
        if (blip_buffer_count != blip_get_size(blip_buf_left_) + 18
            || blip_buffer.size() != static_cast<size_t>(blip_buffer_count)
            || blip_restore_state(blip_buf_left_, blip_factor, blip_offset,
                                  blip_avail, blip_integrator, blip_buffer.data(),
                                  blip_buffer_count) != 0) {
            s.fail();
            return;
        }
    }
    s.sync_span(output_buffer_, sample_count_ * 2);
}

void NesApu::serialize(StateStream& s) {
    s.sync(apu_enabled_);
    s.sync(need_to_run_);
    s.sync(previous_cycle_);
    s.sync(current_cycle_);
    square1_->serialize(s);
    square2_->serialize(s);
    triangle_->serialize(s);
    noise_->serialize(s);
    dmc_->serialize(s);
    frame_counter_->serialize(s);

    for (AudioFrame& frame : audio_ring_) {
        s.sync_vector(frame.data);
        uint64_t samples = frame.samples;
        s.sync(samples);
        if (s.is_loading()) {
            if (samples != frame.data.size() / 2) s.fail();
            frame.samples = static_cast<size_t>(samples);
        }
    }

    uint64_t write_index = write_index_;
    uint64_t read_index = read_index_;
    uint64_t available_frames = available_frames_;
    s.sync(write_index);
    s.sync(read_index);
    s.sync(available_frames);
    s.sync_vector(frame_accumulator_);
    if (s.is_loading()) {
        if (write_index >= MAX_AUDIO_FRAMES || read_index >= MAX_AUDIO_FRAMES
            || available_frames > MAX_AUDIO_FRAMES) {
            s.fail();
            return;
        }
        write_index_ = static_cast<size_t>(write_index);
        read_index_ = static_cast<size_t>(read_index);
        available_frames_ = static_cast<size_t>(available_frames);
    }
}

void OpenBusHandler::serialize(StateStream& s) {
    s.sync(external_open_bus_);
    s.sync(internal_open_bus_);
}

void NesMemoryManager::serialize(StateStream& s) {
    s.sync_span(internal_ram_, internal_ram_size_);
    open_bus_handler_.serialize(s);
}

void NesControlManager::serialize(StateStream& s) {
    s.sync(write_pending_);
    s.sync(write_addr_);
    s.sync(write_value_);
    s.sync_array(controller_state_);
    s.sync_array(controller_read_pos_);
    s.sync(strobe_);
    s.sync(port2_zapper_enabled_);
    s.sync(cli_exp_bit3_mode_);
    s.sync(kb_row_);
    s.sync(kb_column_);
    s.sync(kb_enabled_);
}

void VsControlManager::serialize(StateStream& s) {
    NesControlManager::serialize(s);
    s.sync(dip_switches_);
    s.sync(protection_counter_);
    s.sync(prg_chr_select_bit_);
}

void BaseMapper::serialize(StateStream& s) {
    sync_optional_fixed_vector(s, chr_ram_, get_chr_ram_size());
    if (s.is_loading() && !s.has_error()) {
        chr_ram_size_ = static_cast<uint32_t>(chr_ram_.size());
    }
    sync_fixed_vector(s, work_ram_);
    sync_fixed_vector(s, save_ram_);
    s.sync_span(nametable_ram_, nt_ram_size_);
    s.sync(mirroring_type_);

    enum Region : uint8_t {
        NONE = 0,
        PRG_ROM = 1,
        CHR_ROM = 2,
        CHR_RAM = 3,
        WORK_RAM = 4,
        SAVE_RAM = 5,
        NAMETABLE_RAM = 6,
        MAPPER_REGION_START = 0x80,
        UNKNOWN = 0xFF,
    };

    auto locate = [](uint8_t* pointer, uint8_t* base, size_t size, uint32_t& offset) {
        if (!pointer || !base || size == 0) return false;
        uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
        uintptr_t begin = reinterpret_cast<uintptr_t>(base);
        if (address < begin || address >= begin + size) return false;
        offset = static_cast<uint32_t>(address - begin);
        return true;
    };

    auto sync_page = [&](uint8_t*& pointer, bool ppu_page) {
        uint8_t region = NONE;
        uint32_t offset = 0;
        if (s.is_saving() && pointer) {
            if (locate(pointer, prg_rom_.data(), prg_rom_.size(), offset)) region = PRG_ROM;
            else if (locate(pointer, chr_rom_.data(), chr_rom_.size(), offset)) region = CHR_ROM;
            else if (locate(pointer, chr_ram_.data(), chr_ram_.size(), offset)) region = CHR_RAM;
            else if (locate(pointer, work_ram_.data(), work_ram_.size(), offset)) region = WORK_RAM;
            else if (locate(pointer, save_ram_.data(), save_ram_.size(), offset)) region = SAVE_RAM;
            else if (locate(pointer, nametable_ram_, nt_ram_size_, offset)) region = NAMETABLE_RAM;
            else {
                uint8_t mapper_region = 0;
                if (locate_state_memory(pointer, mapper_region, offset)
                    && mapper_region < UNKNOWN - MAPPER_REGION_START) {
                    region = static_cast<uint8_t>(MAPPER_REGION_START + mapper_region);
                } else {
                    region = UNKNOWN;
                    s.fail();
                }
            }
        }
        s.sync(region);
        s.sync(offset);
        if (s.is_loading()) {
            auto restore = [&](std::vector<uint8_t>& values) -> uint8_t* {
                return offset < values.size() ? values.data() + offset : nullptr;
            };
            switch (region) {
                case NONE: pointer = nullptr; break;
                case PRG_ROM: pointer = restore(prg_rom_); break;
                case CHR_ROM: pointer = restore(chr_rom_); break;
                case CHR_RAM: pointer = restore(chr_ram_); break;
                case WORK_RAM: pointer = restore(work_ram_); break;
                case SAVE_RAM: pointer = restore(save_ram_); break;
                case NAMETABLE_RAM:
                    pointer = offset < nt_ram_size_ ? nametable_ram_ + offset : nullptr;
                    break;
                case UNKNOWN:
                    pointer = nullptr;
                    s.fail();
                    break;
                default:
                    if (region >= MAPPER_REGION_START) {
                        pointer = restore_state_memory(
                            static_cast<uint8_t>(region - MAPPER_REGION_START), offset);
                    } else {
                        pointer = nullptr;
                    }
                    if (!pointer) s.fail();
                    break;
            }
            if (!pointer && region != NONE && region != UNKNOWN) s.fail();
        }
        (void)ppu_page;
    };

    for (uint8_t*& page : prg_pages_) sync_page(page, false);
    for (uint8_t*& page : chr_pages_) sync_page(page, true);
    s.sync_array(prg_memory_access_);
    s.sync_array(chr_memory_access_);
    s.sync_array(chr_memory_type_);
}

void Mapper000::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(chr_is_ram_);
}

bool Mapper001::locate_state_memory(
    uint8_t* pointer, uint8_t& region, uint32_t& offset) const {
    if (!locate_vector_memory(pointer, work_ram_, offset)) return false;
    region = 0;
    return true;
}

uint8_t* Mapper001::restore_state_memory(uint8_t region, uint32_t offset) {
    return region == 0 && offset < work_ram_.size() ? work_ram_.data() + offset : nullptr;
}

void Mapper001::serialize(StateStream& s) {
    sync_fixed_vector(s, work_ram_);
    BaseMapper::serialize(s);
    s.sync(write_buffer_);
    s.sync(shift_count_);
    s.sync(wram_disable_);
    s.sync(chr_mode_);
    s.sync(prg_mode_);
    s.sync(slot_select_);
    s.sync(chr_reg0_);
    s.sync(chr_reg1_);
    s.sync(prg_reg_);
    s.sync(last_write_cycle_);
    s.sync(last_chr_reg_);
}

bool Mapper002::locate_state_memory(
    uint8_t* pointer, uint8_t& region, uint32_t& offset) const {
    if (!locate_vector_memory(pointer, work_ram_, offset)) return false;
    region = 0;
    return true;
}

uint8_t* Mapper002::restore_state_memory(uint8_t region, uint32_t offset) {
    return region == 0 && offset < work_ram_.size() ? work_ram_.data() + offset : nullptr;
}

void Mapper002::serialize(StateStream& s) {
    sync_fixed_vector(s, work_ram_);
    BaseMapper::serialize(s);
}

bool Mapper004::locate_state_memory(
    uint8_t* pointer, uint8_t& region, uint32_t& offset) const {
    if (!locate_vector_memory(pointer, work_ram_, offset)) return false;
    region = 0;
    return true;
}

uint8_t* Mapper004::restore_state_memory(uint8_t region, uint32_t offset) {
    return region == 0 && offset < work_ram_.size() ? work_ram_.data() + offset : nullptr;
}

void Mapper004::serialize(StateStream& s) {
    sync_fixed_vector(s, work_ram_);
    BaseMapper::serialize(s);
    s.sync(irq_reload_value_);
    s.sync(irq_counter_);
    s.sync(irq_reload_);
    s.sync(irq_enabled_);
    s.sync(force_mmc3_rev_a_irqs_);
    s.sync(prg_mode_);
    s.sync(chr_mode_);
    s.sync(current_register_);
    s.sync_array(registers_);
    s.sync(wram_enabled_);
    s.sync(wram_write_protected_);
    s.sync(reg_a000_);
    s.sync(a12_low_clock_);
}

void Mapper045::serialize(StateStream& s) {
    Mapper004::serialize(s);
    sync_std_array(s, outer_registers_);
    s.sync(outer_register_index_);
}

bool Mapper005::locate_state_memory(
    uint8_t* pointer, uint8_t& region, uint32_t& offset) const {
    if (locate_vector_memory(pointer, exram_, offset)) {
        region = 0;
        return true;
    }
    if (locate_vector_memory(pointer, wram_, offset)) {
        region = 1;
        return true;
    }
    return false;
}

uint8_t* Mapper005::restore_state_memory(uint8_t region, uint32_t offset) {
    std::vector<uint8_t>* values = nullptr;
    if (region == 0) values = &exram_;
    if (region == 1) values = &wram_;
    return values && offset < values->size() ? values->data() + offset : nullptr;
}

void Mapper005::serialize(StateStream& s) {
    sync_fixed_vector(s, exram_);
    sync_fixed_vector(s, wram_);
    BaseMapper::serialize(s);
    s.sync(prg_mode_);
    s.sync(chr_mode_);
    s.sync(chr_upper_bits_);
    s.sync(prg_ram_protect1_);
    s.sync(prg_ram_protect2_);
    s.sync_array(prg_banks_);
    s.sync_array(chr_banks_);
    s.sync(nametable_mapping_);
    s.sync(extended_ram_mode_);
    s.sync(fill_mode_tile_);
    s.sync(fill_mode_color_);
    s.sync(fill_mode_attr_byte_);
    s.sync(vertical_split_enabled_);
    s.sync(vertical_split_right_side_);
    s.sync(vertical_split_delimiter_tile_);
    s.sync(vertical_split_scroll_);
    s.sync(vertical_split_bank_);
    s.sync(multiplier_value1_);
    s.sync(multiplier_value2_);
    s.sync(irq_counter_target_);
    s.sync(irq_enabled_);
    s.sync(irq_pending_);
    s.sync(scanline_counter_);
    s.sync(ppu_in_frame_);
    s.sync(need_in_frame_);
    s.sync(ppu_idle_counter_);
    s.sync(last_ppu_read_addr_);
    s.sync(nt_read_counter_);
    s.sync(split_tile_number_);
    s.sync(split_in_split_region_);
    s.sync(split_tile_);
    s.sync(ex_attribute_last_nt_fetch_);
    s.sync(ex_attr_last_fetch_counter_);
    s.sync(ex_attr_selected_chr_bank_);
    s.sync_array(mmc5_sq_period_);
    s.sync_array(mmc5_sq_duty_);
    s.sync_array(mmc5_sq_duty_pos_);
    s.sync_array(mmc5_sq_volume_);
    s.sync_array(mmc5_sq_constant_volume_);
    s.sync_array(mmc5_sq_halt_);
    s.sync_array(mmc5_sq_env_div_);
    s.sync_array(mmc5_sq_env_ctr_);
    s.sync_array(mmc5_sq_env_start_);
    s.sync_array(mmc5_sq_len_);
    s.sync_array(mmc5_sq_timer_);
    s.sync_array(mmc5_sq_output_);
    s.sync(pcm_output_);
    s.sync(pcm_read_mode_);
    s.sync(pcm_irq_enabled_);
    s.sync(pcm_irq_pending_);
    s.sync(mmc5_last_mix_);
    s.sync(mmc5_env_clock_divider_);
    s.sync(last_chr_reg_);
    s.sync(prev_chr_a_);
}

void BaseEeprom24C0X::serialize(StateStream& s) {
    s.sync(mode_);
    s.sync(next_mode_);
    s.sync(chip_address_);
    s.sync(address_);
    s.sync(data_);
    s.sync(counter_);
    s.sync(output_);
    s.sync(prev_scl_);
    s.sync(prev_sda_);
    s.sync_array(rom_data_);
}

bool Mapper016::locate_state_memory(
    uint8_t* pointer, uint8_t& region, uint32_t& offset) const {
    if (!locate_vector_memory(pointer, sram_, offset)) return false;
    region = 0;
    return true;
}

uint8_t* Mapper016::restore_state_memory(uint8_t region, uint32_t offset) {
    return region == 0 && offset < sram_.size() ? sram_.data() + offset : nullptr;
}

void Mapper016::serialize(StateStream& s) {
    sync_fixed_vector(s, sram_);
    BaseMapper::serialize(s);
    s.sync_array(chr_regs_);
    s.sync(prg_page_);
    s.sync(prg_bank_select_);
    s.sync(irq_enabled_);
    s.sync(irq_counter_);
    s.sync(irq_reload_);
    s.sync(submapper_);

    auto sync_eeprom = [&](std::unique_ptr<BaseEeprom24C0X>& eeprom) {
        bool present = static_cast<bool>(eeprom);
        s.sync(present);
        if (present != static_cast<bool>(eeprom)) {
            s.fail();
        } else if (eeprom) {
            eeprom->serialize(s);
        }
    };
    sync_eeprom(standard_eeprom_);
    sync_eeprom(extra_eeprom_);

    bool has_barcode_reader = static_cast<bool>(barcode_reader_);
    s.sync(has_barcode_reader);
    if (has_barcode_reader != static_cast<bool>(barcode_reader_)) s.fail();
}

void Mapper006::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_enabled_);
    s.sync(ffe_alt_mode_);
}

void Mapper009::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(left_latch_);
    s.sync(right_latch_);
    s.sync(prg_page_);
    s.sync_array(left_chr_page_);
    s.sync_array(right_chr_page_);
}

void Mapper018::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(prg_banks_);
    s.sync_array(chr_banks_);
    s.sync_array(irq_reload_value_);
    s.sync(irq_counter_);
    s.sync(irq_counter_size_);
    s.sync(irq_enabled_);
}

void Mapper019::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(variant_);
    s.sync(auto_detect_variant_);
    s.sync(not_namco340_);
    s.sync(write_protect_);
    s.sync(low_chr_nt_mode_);
    s.sync(high_chr_nt_mode_);
    s.sync(irq_counter_);
    sync_std_array(s, audio_ram_);
    sync_std_array(s, channel_output_);
    s.sync(audio_ram_position_);
    s.sync(audio_auto_increment_);
    s.sync(audio_update_counter_);
    s.sync(current_audio_channel_);
    s.sync(last_audio_output_);
    s.sync(audio_disabled_);
}

void Mapper032::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(prg_regs_);
    s.sync(prg_mode_);
}

void Mapper035::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_enabled_);
    s.sync(a12_low_clock_);
}

void Mapper041::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(prg_bank_);
    s.sync(chr_bank_);
}

void Mapper060::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(reset_counter_);
}

void Mapper064::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    sync_std_array(s, registers_);
    s.sync(current_register_);
    s.sync(irq_enabled_);
    s.sync(irq_cycle_mode_);
    s.sync(need_reload_);
    s.sync(irq_counter_);
    s.sync(irq_reload_value_);
    s.sync(cpu_clock_counter_);
    s.sync(irq_delay_);
    s.sync(force_clock_);
    s.sync(a12_low_clock_);
}

void Mapper065::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_enabled_);
    s.sync(irq_counter_);
    s.sync(irq_reload_value_);
}

void Mapper067::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_latch_);
    s.sync(irq_enabled_);
}

void Mapper068::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    sync_std_array(s, nametable_registers_);
    s.sync(use_chr_nametables_);
    s.sync(prg_ram_enabled_);
    s.sync(licensing_timer_);
    s.sync(using_external_rom_);
    s.sync(external_page_);
}

void Mapper069::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(command_);
    s.sync(work_ram_value_);
    s.sync(irq_enabled_);
    s.sync(irq_counter_enabled_);
    s.sync(irq_counter_);
    sync_std_array(s, audio_volume_lut_);
    sync_std_array(s, audio_registers_);
    sync_std_array(s, audio_timers_);
    sync_std_array(s, audio_tone_steps_);
    s.sync(current_audio_register_);
    s.sync(last_audio_output_);
    s.sync(process_audio_tick_);
}

void Mapper072::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(prg_flag_);
    s.sync(chr_flag_);
}

void Mapper073::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_enable_on_ack_);
    s.sync(irq_enabled_);
    s.sync(small_counter_);
    s.sync(irq_reload_);
    s.sync(irq_counter_);
}

void Mapper075::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(chr_banks_);
}

void Mapper090::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    sync_std_array(s, prg_registers_);
    sync_std_array(s, chr_low_registers_);
    sync_std_array(s, chr_high_registers_);
    s.sync(prg_mode_);
    s.sync(enable_prg_at_6000_);
    s.sync(chr_mode_);
    s.sync(chr_block_mode_);
    s.sync(chr_block_);
    s.sync(mirror_chr_);
    s.sync(mirroring_register_);
    s.sync(irq_enabled_);
    s.sync(irq_source_);
    s.sync(irq_count_direction_);
    s.sync(irq_small_prescaler_);
    s.sync(irq_prescaler_);
    s.sync(irq_counter_);
    s.sync(irq_xor_register_);
    s.sync(last_ppu_address_);
    s.sync(multiply_value_1_);
    s.sync(multiply_value_2_);
    s.sync(register_ram_value_);
}

void Mapper092::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(prg_flag_);
    s.sync(chr_flag_);
}

void Mapper099::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(prg_chr_select_bit_);
}

void Mapper112::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(current_reg_);
    s.sync(outer_chr_bank_);
    s.sync_array(registers_);
}

void Mapper117::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_reload_value_);
    s.sync(irq_enabled_);
    s.sync(irq_enabled_alt_);
    s.sync(a12_low_clock_);
}

void Mapper156::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(chr_low_);
    s.sync_array(chr_high_);
}

void Mapper170::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(reg_);
}

void Mapper202::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(prg_mode1_);
}

void Mapper221::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(mode_);
    s.sync(prg_reg_);
}

void Mapper226::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(registers_);
}

void Mapper230::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(contra_mode_);
}

void Mapper233::serialize(StateStream& s) {
    Mapper226::serialize(s);
    s.sync(reset_flag_);
}

void Mapper40::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
}

void Mapper42::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_enabled_);
    s.sync(prg_reg_);
}

void Mapper43::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(reg_);
    s.sync(swap_);
    s.sync(irq_counter_);
    s.sync(irq_enabled_);
}

void Mapper46::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(regs_);
}

void Mapper50::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(irq_counter_);
    s.sync(irq_enabled_);
}

void Mapper57::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync_array(registers_);
}

void MapperNamco108::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(current_register_);
    s.sync_array(registers_);
}

void MapperSachen74LS374N::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(current_register_);
    s.sync_array(registers_);
}

void VrcIrq::serialize(StateStream& s) {
    s.sync(reload_value_);
    s.sync(counter_);
    s.sync(prescaler_counter_);
    s.sync(enabled_);
    s.sync(enabled_after_ack_);
    s.sync(cycle_mode_);
}

void MapperVRC2_4::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(variant_);
    s.sync(use_heuristics_);
    s.sync(use_microwire_);
    s.sync(prg_reg_0_);
    s.sync(prg_reg_1_);
    s.sync(prg_mode_);
    sync_std_array(s, chr_high_);
    sync_std_array(s, chr_low_);
    s.sync(latch_);
    irq_.serialize(s);
}

void MapperVRC6::Pulse::serialize(StateStream& s) {
    s.sync(volume);
    s.sync(duty_cycle);
    s.sync(ignore_duty);
    s.sync(frequency);
    s.sync(enabled);
    s.sync(timer);
    s.sync(step);
    s.sync(frequency_shift);
}

void MapperVRC6::Saw::serialize(StateStream& s) {
    s.sync(accumulator_rate);
    s.sync(accumulator);
    s.sync(frequency);
    s.sync(enabled);
    s.sync(timer);
    s.sync(step);
    s.sync(frequency_shift);
}

void MapperVRC6::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    s.sync(swap_address_lines_);
    s.sync(banking_mode_);
    sync_std_array(s, chr_registers_);
    irq_.serialize(s);
    pulse_1_.serialize(s);
    pulse_2_.serialize(s);
    saw_.serialize(s);
    s.sync(halt_audio_);
    s.sync(last_audio_output_);
}

void Mapper252::serialize(StateStream& s) {
    BaseMapper::serialize(s);
    sync_std_array(s, chr_registers_);
    irq_.serialize(s);
}

void NesConsole::serialize(StateStream& s) {
    if (!cpu_ || !ppu_ || !apu_ || !sound_mixer_ || !memory_manager_
        || !control_manager_ || !mapper_) {
        s.fail();
        return;
    }
    s.sync(last_completed_ppu_frame_);
    cpu_->serialize(s);
    ppu_->serialize(s);
    memory_manager_->serialize(s);
    sound_mixer_->serialize(s);
    apu_->serialize(s);
    mapper_->serialize(s);
    control_manager_->serialize(s);
}

} // namespace ear6::nes
