#pragma once

#include "ines_memory_handler.h"
#include "nes_types.h"
#include <cstdint>
#include <vector>

namespace ear6::nes {

class NesConsole;
class BaseMapper;

class NesPpu : public INesMemoryHandler {
public:
    NesPpu(NesConsole* console);
    ~NesPpu();

    // INesMemoryHandler
    void get_memory_ranges(MemoryRanges& ranges) override;
    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;

    // Core execution
    void reset(bool soft_reset);
    void run(uint64_t run_to);
    void exec();

    // Frame control
    uint32_t get_frame_count() const { return frame_count_; }
    int get_scanline() const { return scanline_; }
    int get_cycle() const { return cycle_; }
    uint8_t get_palette_ram0() const { return palette_ram_[0]; }
    uint16_t* get_framebuffer() { return framebuffer_; }
    const uint16_t* get_framebuffer() const { return framebuffer_; }
    uint8_t* get_oam() { return sprite_ram_; }

    uint8_t peek_ram(uint16_t addr);
    void set_no_odd_frame_skip() { no_odd_frame_skip_ = true; }

private:
    NesConsole* console_ = nullptr;
    BaseMapper* mapper_ = nullptr;

    // Clock
    uint64_t master_clock_ = 0;
    uint8_t master_clock_divider_ = 4;
    uint32_t cycle_ = 340;
    int16_t scanline_ = -1;
    uint32_t frame_count_ = 1;
    uint32_t nmi_scanline_ = 241;
    uint16_t vblank_end_ = 260;
    bool no_odd_frame_skip_ = false;


    // Framebuffer
    uint16_t* framebuffer_ = nullptr;
    uint16_t* output_buffers_[2] = {};
    uint16_t* current_output_buffer_ = nullptr;
    int last_updated_pixel_ = -1;

    // Registers
    uint16_t video_ram_addr_ = 0;        // v
    uint16_t tmp_video_ram_addr_ = 0;    // t
    uint16_t high_bit_shift_ = 0;
    uint16_t low_bit_shift_ = 0;
    uint8_t sprite_ram_addr_ = 0;
    uint8_t x_scroll_ = 0;
    uint8_t open_bus_ = 0;
    int open_bus_decay_stamp_[8] = {};
    bool enable_oam_decay_ = false;
    bool need_state_update_ = false;
    bool rendering_enabled_ = false;
    bool prev_rendering_enabled_ = false;
    bool sprite0_visible_ = false;
    uint8_t sprite_count_ = 0;
    uint8_t secondary_oam_addr_ = 0;
    uint8_t oam_copy_buffer_ = 0;
    bool sprite_in_range_ = false;
    bool sprite0_added_ = false;
    uint8_t sprite_addr_h_ = 0;
    uint8_t sprite_addr_l_ = 0;
    uint8_t overflow_bug_counter_ = 0;
    bool oam_copy_done_ = false;
    uint16_t minimum_draw_bg_cycle_ = 0;
    uint16_t minimum_draw_sprite_cycle_ = 0;
    uint16_t minimum_draw_sprite_standard_cycle_ = 0;

    // Memory
    uint8_t palette_ram_[0x20] = {};
    uint8_t sprite_ram_[0x100] = {};
    uint8_t secondary_sprite_ram_[0x20] = {};

    // Tile info
    TileInfo tile_ = {};
    uint16_t ppu_bus_address_ = 0;
    uint8_t current_tile_palette_ = 0;
    uint8_t previous_tile_palette_ = 0;
    uint16_t intensify_color_bits_ = 0;
    uint8_t palette_ram_mask_ = 0x3F;
    uint8_t update_vram_addr_delay_ = 0;

    // Sprite state
    uint32_t sprite_index_ = 0;
    bool has_sprite_[257] = {};
    NesSpriteInfo sprite_tiles_[64] = {};

    // Control/mask flags
    PpuControlFlags control_ = {};
    PpuMaskFlags mask_ = {};
    PPUStatusFlags status_flags_ = {};

    // Deferred state
    uint16_t update_vram_addr_ = 0;
    bool prevent_vbl_flag_ = false;
    bool write_toggle_ = false;
    bool need_video_ram_increment_ = false;
    bool allow_full_ppu_access_ = false;
    uint8_t memory_read_buffer_ = 0;
    uint32_t ignore_vram_read_ = 0;

    // OAM corruption
    uint64_t oam_decay_cycles_[0x40] = {};
    bool corrupt_oam_row_[32] = {};

    // Sprite evaluation extra
    uint8_t first_visible_sprite_addr_ = 0;
    uint8_t last_visible_sprite_addr_ = 0;

    // Internal state
    bool emulator_bg_enabled_ = true;
    bool emulator_sprites_enabled_ = true;

    // Methods
    void update_state();
    void update_status_flag();
    void set_control_register(uint8_t value);
    void set_mask_register(uint8_t value);
    void process_tmp_addr_scroll_glitch(uint16_t normal_addr, uint16_t value, uint16_t mask);
    void set_open_bus(uint8_t mask, uint8_t value);
    uint8_t apply_open_bus(uint8_t mask, uint8_t value);
    void process_status_reg_open_bus(uint8_t& open_bus_mask, uint8_t& return_value);
    void update_video_ram_addr();
    void inc_vertical_scrolling();
    void inc_horizontal_scrolling();
    uint16_t get_nametable_addr();
    uint16_t get_attribute_addr();
    void process_scanline_first_cycle();
    void process_scanline_impl();
    void process_sprite_evaluation();
    void process_sprite_evaluation_start();
    void process_sprite_evaluation_end();
    void begin_vblank();
    void trigger_nmi();
    void load_tile_info();
    void load_sprite(uint8_t sprite_y, uint8_t tile_index, uint8_t attributes, uint8_t sprite_x, bool extra_sprite);
    void load_sprite_tile_info();
    void load_extra_sprites();
    void shift_tile_registers();
    uint8_t get_pixel_color();
    uint8_t read_sprite_ram(uint8_t addr);
    void write_sprite_ram(uint8_t addr, uint8_t value);
    void set_oam_corruption_flags();
    void process_oam_corruption();
    void send_frame();
    void set_bus_address(uint16_t addr);
    uint8_t read_vram(uint16_t addr);
    void write_vram(uint16_t addr, uint8_t value);
    void update_grayscale_and_intensify_bits();
    void update_color_bit_masks();
    void update_minimum_draw_cycles();

    // Tracing (requires EAR6_ENABLE_PPU_TRACE and EAR6_TRACE_PPU)
    void trace_ppu(const char* fmt, ...);
};

} // namespace ear6::nes
