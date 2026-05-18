#pragma once

#include "base_mapper.h"
#include "eeprom_24c0x.h"
#include "datach_barcode_reader.h"
#include <memory>

namespace ear6::nes {

class Mapper016 : public BaseMapper {
public:
    void init(const RomInfo& info,
              const std::vector<uint8_t>& prg_rom,
              const std::vector<uint8_t>& chr_rom) override;
    void reset(bool soft_reset) override;
    uint16_t get_prg_page_size() override { return 0x4000; }
    uint16_t get_chr_page_size() override { return 0x0400; }
    uint32_t get_chr_ram_size() override { return 0x2000; }
    bool has_cpu_clock_hook() override { return true; }
    void process_cpu_clock() override;
    bool allow_register_read() override { return true; }

protected:
    void write_register(uint16_t addr, uint8_t value) override;
    uint8_t read_register(uint16_t addr) override;

private:
    void update_prg();

    uint8_t chr_regs_[8] = {};
    uint8_t prg_page_ = 0;
    uint8_t prg_bank_select_ = 0;
    bool irq_enabled_ = false;
    uint16_t irq_counter_ = 0;
    uint16_t irq_reload_ = 0;
    uint8_t submapper_ = 0;

    // Variant-specific
    std::unique_ptr<BaseEeprom24C0X> standard_eeprom_;
    std::unique_ptr<BaseEeprom24C0X> extra_eeprom_;
    std::unique_ptr<DatachBarcodeReader> barcode_reader_;
    std::vector<uint8_t> sram_;
};

} // namespace ear6::nes
