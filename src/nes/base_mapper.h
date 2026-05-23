#pragma once

#include "ines_memory_handler.h"
#include "nes_types.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace ear6::nes {

class NesConsole;

class BaseMapper : public INesMemoryHandler {
public:
    static constexpr uint32_t NAMETABLE_SIZE = 0x400;

    BaseMapper();
    virtual ~BaseMapper();

    void initialize(NesConsole* console);

    virtual void init(const RomInfo& info,
                      const std::vector<uint8_t>& prg_rom,
                      const std::vector<uint8_t>& chr_rom) = 0;
    virtual void reset(bool) {}
    virtual void process_cpu_clock() {}
    virtual bool has_cpu_clock_hook() { return false; }
    virtual bool has_vram_address_hook() { return false; }
    virtual void notify_vram_address_change(uint16_t addr) { (void)addr; }
    virtual bool has_custom_vram_read() { return false; }
    virtual uint8_t read_vram_custom(uint16_t addr) { return read_vram(addr); }

    // CPU memory
    uint8_t read_ram(uint16_t addr) override;
    void write_ram(uint16_t addr, uint8_t value) override;
    void get_memory_ranges(MemoryRanges& ranges) override;

    // PRG page management
    void select_prg_page(uint16_t slot, uint16_t page, PrgMemoryType type = PrgMemoryType::PRG_ROM);
    void select_prg_page_2x(uint16_t slot, uint16_t page, PrgMemoryType type = PrgMemoryType::PRG_ROM);
    void select_prg_page_4x(uint16_t slot, uint16_t page, PrgMemoryType type = PrgMemoryType::PRG_ROM);
    void set_cpu_memory_mapping(uint16_t start, uint16_t end, uint8_t* source,
                                    uint32_t source_offset, uint32_t source_size,
                                    int8_t access_type = -1);
    void set_cpu_memory_mapping(uint16_t start, uint16_t end, int16_t page_number,
                                PrgMemoryType type = PrgMemoryType::PRG_ROM,
                                int8_t access_type = -1);

    // CHR/PPU page management
    void select_chr_page(uint16_t slot, uint16_t page, ChrMemoryType type = ChrMemoryType::DEFAULT);
    void select_chr_page_2x(uint16_t slot, uint16_t page, ChrMemoryType type = ChrMemoryType::DEFAULT);
    void select_chr_page_4x(uint16_t slot, uint16_t page, ChrMemoryType type = ChrMemoryType::DEFAULT);
    void select_chr_page_8x(uint16_t slot, uint16_t page, ChrMemoryType type = ChrMemoryType::DEFAULT);

    void set_ppu_memory_mapping(uint16_t start, uint16_t end, uint16_t page_number,
                                ChrMemoryType type = ChrMemoryType::DEFAULT,
                                int8_t access_type = -1);
    void set_ppu_memory_mapping(uint16_t start, uint16_t end,
                                ChrMemoryType type, uint32_t source_offset,
                                int8_t access_type = -1);
    void set_ppu_memory_mapping(uint16_t start, uint16_t end, uint8_t* source,
                                    uint32_t source_offset, uint32_t source_size,
                                    int8_t access_type = -1);

    // VRAM read/write (PPU calls these)
    uint8_t read_vram(uint16_t addr);
    void write_vram(uint16_t addr, uint8_t value);

    // Nametable management
    uint8_t* get_nametable(uint8_t index);
    void set_nametable(uint8_t index, uint8_t nametable_index);
    void set_mirroring_type(MirroringType type);

    virtual uint16_t get_prg_page_size() = 0;
    virtual uint16_t get_chr_page_size() = 0;

    // CHR RAM support (matches Mesen2's BaseMapper interface)
    virtual uint32_t get_chr_ram_size() { return 0x2000; }
    virtual uint16_t get_chr_ram_page_size() { return get_chr_page_size(); }
    bool has_chr_rom() const { return chr_rom_size_ > 0; }
    bool has_chr_ram() const { return chr_ram_size_ > 0; }
    void initialize_chr_ram(int32_t chr_ram_size = -1);

    // Work RAM / Save RAM support (matches Mesen2's BaseMapper interface)
    virtual uint32_t get_work_ram_size() { return 0x2000; }
    virtual uint32_t get_save_ram_size() { return 0x2000; }
    void setup_default_work_ram();
    void apply_trainer_data(const std::vector<uint8_t>& trainer_data);
    void init_work_ram(const RomInfo& info);

    void set_has_bus_conflicts(bool enabled) { has_bus_conflicts_ = enabled; }

protected:
    NesConsole* console_ = nullptr;
    RomInfo rom_info_ = {};
    MirroringType mirroring_type_ = MirroringType::HORIZONTAL;

    // PRG data
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> chr_rom_;
    std::vector<uint8_t> chr_ram_;
    std::vector<uint8_t> work_ram_;
    std::vector<uint8_t> save_ram_;
    uint32_t prg_size_ = 0;
    uint32_t chr_rom_size_ = 0;
    uint32_t chr_ram_size_ = 0;
    uint32_t work_ram_size_ = 0;
    uint32_t save_ram_size_ = 0;
    bool has_default_work_ram_ = false;

    // Page tables (256 entries each)
    uint8_t* prg_pages_[0x100] = {};
    uint8_t* chr_pages_[0x100] = {};
    MemoryAccessType chr_memory_access_[0x100] = {};
    MemoryAccessType prg_memory_access_[0x100] = {};
    ChrMemoryType chr_memory_type_[0x100] = {};

    // Nametable RAM
    uint8_t* nametable_ram_ = nullptr;
    uint8_t nametable_count_ = 2;
    uint32_t nt_ram_size_ = 0;

    // Register address tracking
    bool is_write_register_addr_[0x10000] = {};
    bool is_read_register_addr_[0x10000] = {};
    bool has_bus_conflicts_ = false;

    virtual void write_register(uint16_t addr, uint8_t value);
    virtual uint8_t read_register(uint16_t addr) { return addr >> 8; }
    virtual uint16_t register_start_address() { return 0x8000; }
    virtual uint16_t register_end_address() { return 0xFFFF; }
    virtual bool allow_register_read() { return false; }

    void add_register_range(uint16_t start, uint16_t end, MemoryOperation operation = MemoryOperation::ANY);
};

} // namespace ear6::nes
