#include "mapper_227.h"

namespace ear6::nes {

void Mapper227::init(const RomInfo& info, const std::vector<uint8_t>& prg_rom, const std::vector<uint8_t>& chr_rom) {
    rom_info_ = info; prg_rom_ = prg_rom; chr_rom_ = chr_rom;
    prg_size_ = (uint32_t)prg_rom.size(); chr_rom_size_ = (uint32_t)chr_rom.size();
    set_mirroring_type(info.mirroring);
    add_register_range(0x8000, 0xFFFF, MemoryOperation::WRITE);
    write_register(0x8000, 0);
}

void Mapper227::write_register(uint16_t addr, uint8_t value) {
    (void)value;
    uint16_t prg_bank = ((addr >> 2) & 0x1F) | ((addr & 0x100) >> 3);
    bool s_flag = (addr & 0x01) != 0;
    bool l_flag = ((addr >> 9) & 0x01) != 0;
    bool prg_mode = ((addr >> 7) & 0x01) != 0;

    if (prg_mode) {
        if (s_flag) {
            select_prg_page_2x(0, prg_bank & 0xFE);
        } else {
            select_prg_page(0, prg_bank);
            select_prg_page(1, prg_bank);
        }
    } else {
        if (s_flag) {
            if (l_flag) {
                select_prg_page(0, prg_bank & 0x3E);
                select_prg_page(1, prg_bank | 0x07);
            } else {
                select_prg_page(0, prg_bank & 0x3E);
                select_prg_page(1, prg_bank & 0x38);
            }
        } else {
            if (l_flag) {
                select_prg_page(0, prg_bank);
                select_prg_page(1, prg_bank | 0x07);
            } else {
                select_prg_page(0, prg_bank);
                select_prg_page(1, prg_bank & 0x38);
            }
        }
    }
    set_mirroring_type((addr & 0x02) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
}

} // namespace ear6::nes
