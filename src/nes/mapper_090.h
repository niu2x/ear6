#pragma once

#include "base_mapper.h"

#include <array>

namespace ear6::nes {

class Mapper090 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    uint16_t get_prg_page_size() override { return 0x2000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    bool has_cpu_clock_hook() override { return true; }
    bool has_vram_address_hook() override { return true; }
    bool has_custom_vram_read() override { return true; }
    void process_cpu_clock() override;
    void notify_vram_address_change(uint16_t addr) override;
    uint8_t read_vram_custom(uint16_t addr) override;
    void setup_default_work_ram() override;

protected:
    bool allow_register_read() override { return true; }
    uint8_t read_register(uint16_t addr) override;
    void write_register(uint16_t addr, uint8_t value) override;

private:
    enum class IrqSource : uint8_t {
        CPU_CLOCK = 0,
        PPU_A12_RISE = 1,
        PPU_READ = 2,
        CPU_WRITE = 3,
    };

    void update_state();
    void update_prg_state();
    void update_chr_state();
    void update_mirroring_state();
    uint8_t invert_prg_bits(uint8_t value, bool invert) const;
    uint16_t get_chr_register(uint8_t index) const;
    void tick_irq_counter();

    std::array<uint8_t, 4> prg_registers_ = {};
    std::array<uint8_t, 8> chr_low_registers_ = {};
    std::array<uint8_t, 8> chr_high_registers_ = {};
    uint8_t prg_mode_ = 0;
    bool enable_prg_at_6000_ = false;
    uint8_t chr_mode_ = 0;
    bool chr_block_mode_ = false;
    uint8_t chr_block_ = 0;
    bool mirror_chr_ = false;
    uint8_t mirroring_register_ = 0;

    bool irq_enabled_ = false;
    IrqSource irq_source_ = IrqSource::CPU_CLOCK;
    uint8_t irq_count_direction_ = 0;
    bool irq_small_prescaler_ = false;
    uint8_t irq_prescaler_ = 0;
    uint8_t irq_counter_ = 0;
    uint8_t irq_xor_register_ = 0;
    uint16_t last_ppu_address_ = 0;

    uint8_t multiply_value_1_ = 0;
    uint8_t multiply_value_2_ = 0;
    uint8_t register_ram_value_ = 0;
};

} // namespace ear6::nes
