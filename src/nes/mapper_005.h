#pragma once

#include "base_mapper.h"
#include <vector>

namespace ear6::nes {

class NesPpu;

class Mapper005 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    void reset(bool soft_reset) override;
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;
    bool has_custom_vram_read() override { return true; }
    uint8_t read_vram_custom(uint16_t addr) override;
    uint8_t read_ram(uint16_t addr) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }

protected:
    void write_register(uint16_t addr, uint8_t value) override;
    uint8_t read_register(uint16_t addr) override;
    uint16_t register_start_address() override { return 0x5000; }
    uint16_t register_end_address() override { return 0x5206; }
    bool allow_register_read() override { return true; }

private:
    void update_prg_banks();
    void update_chr_banks(bool force_update = true);
    void update_nametable_mapping();
    void set_extended_ram_mode(uint8_t mode);
    void set_fill_mode_tile(uint8_t tile);
    void set_fill_mode_color(uint8_t color);
    void clock_audio();

    uint8_t prg_mode_ = 3;
    uint8_t chr_mode_ = 0;
    uint8_t chr_upper_bits_ = 0;
    uint8_t prg_ram_protect1_ = 0;
    uint8_t prg_ram_protect2_ = 0;
    uint8_t prg_banks_[5] = {};
    uint16_t chr_banks_[12] = {};

    uint8_t nametable_mapping_ = 0;
    uint8_t extended_ram_mode_ = 0;
    uint8_t fill_mode_tile_ = 0;
    uint8_t fill_mode_color_ = 0;
    uint8_t fill_mode_attr_byte_ = 0;

    bool vertical_split_enabled_ = false;
    bool vertical_split_right_side_ = false;
    uint8_t vertical_split_delimiter_tile_ = 0;
    uint8_t vertical_split_scroll_ = 0;
    uint8_t vertical_split_bank_ = 0;

    uint8_t multiplier_value1_ = 0;
    uint8_t multiplier_value2_ = 0;

    uint8_t irq_counter_target_ = 0;
    bool irq_enabled_ = false;
    bool irq_pending_ = false;
    uint8_t scanline_counter_ = 0;
    bool ppu_in_frame_ = false;
    bool need_in_frame_ = false;
    uint8_t ppu_idle_counter_ = 0;
    uint16_t last_ppu_read_addr_ = 0;
    uint8_t nt_read_counter_ = 0;
    uint32_t split_tile_number_ = 0;
    bool split_in_split_region_ = false;
    uint16_t split_tile_ = 0;

    uint16_t ex_attribute_last_nt_fetch_ = 0;
    int8_t ex_attr_last_fetch_counter_ = 0;
    uint8_t ex_attr_selected_chr_bank_ = 0;

    std::vector<uint8_t> exram_;
    std::vector<uint8_t> wram_;

    uint16_t mmc5_sq_period_[2] = {};
    uint8_t mmc5_sq_duty_[2] = {};
    uint8_t mmc5_sq_duty_pos_[2] = {};
    uint8_t mmc5_sq_volume_[2] = {};
    bool mmc5_sq_constant_volume_[2] = {};
    bool mmc5_sq_halt_[2] = {};
    uint8_t mmc5_sq_env_div_[2] = {};
    uint8_t mmc5_sq_env_ctr_[2] = {};
    bool mmc5_sq_env_start_[2] = {};
    uint8_t mmc5_sq_len_[2] = {};
    uint16_t mmc5_sq_timer_[2] = {};
    int8_t mmc5_sq_output_[2] = {};
    uint8_t pcm_output_ = 0;
    bool pcm_read_mode_ = false;
    bool pcm_irq_enabled_ = false;
    bool pcm_irq_pending_ = false;
    int16_t mmc5_last_mix_ = 0;
    int32_t mmc5_env_clock_divider_ = 0;

    uint16_t last_chr_reg_ = 0;
    bool prev_chr_a_ = false;
};

} // namespace ear6::nes
