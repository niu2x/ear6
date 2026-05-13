#pragma once

#include "nes_types.h"
#include <cstdint>

namespace ear6::nes {

class NesConsole;
class NesMemoryManager;

enum class NesAddrMode {
    None, Acc, Imp, Imm, Rel,
    Zero, Abs, ZeroX, ZeroY,
    Ind, IndX, IndY, IndYW,
    AbsX, AbsXW, AbsY, AbsYW,
    Other
};

class NesCpu {
public:
    static constexpr uint16_t NMIVector = 0xFFFA;
    static constexpr uint16_t ResetVector = 0xFFFC;
    static constexpr uint16_t IRQVector = 0xFFFE;

    NesCpu(NesConsole* console);
    ~NesCpu() = default;

    void reset(bool soft_reset);
    void exec();
    void set_nmi_flag() { state_.nmi_flag = true; }
    void clear_nmi_flag() { state_.nmi_flag = false; }
    void set_irq_source(uint8_t source) { state_.irq_flag |= source; }
    void clear_irq_source(uint8_t source) { state_.irq_flag &= ~source; }

    uint64_t get_cycle_count() const { return state_.cycle_count; }
    NesCpuState& get_state() { return state_; }

    void run_dma_transfer(uint8_t offset_value);
    void start_dmc_transfer();
    void stop_dmc_transfer();

private:
    typedef void (NesCpu::*Func)();

    uint64_t master_clock_ = 0;
    uint8_t ppu_offset_ = 1;
    uint8_t start_clock_count_ = 6;
    uint8_t end_clock_count_ = 6;
    uint16_t operand_ = 0;

    Func op_table_[256];
    NesAddrMode addr_mode_[256];
    NesAddrMode inst_addr_mode_ = NesAddrMode::None;

    bool need_halt_ = false;
    bool sprite_dma_transfer_ = false;
    bool dmc_dma_running_ = false;
    bool abort_dmc_dma_ = false;
    bool need_dummy_read_ = false;
    uint8_t sprite_dma_offset_ = 0;

    bool cpu_write_ = false;

    uint8_t irq_mask_ = 0;

    NesCpuState state_;
    NesConsole* console_ = nullptr;
    NesMemoryManager* memory_manager_ = nullptr;

    bool prev_run_irq_ = false;
    bool run_irq_ = false;
    bool prev_nmi_flag_ = false;
    bool prev_need_nmi_ = false;
    bool need_nmi_ = false;
    bool is_dmc_dma_read_ = false;

    void start_cpu_cycle(bool for_read);
    void end_cpu_cycle(bool for_read);
    void process_pending_dma(uint16_t read_address);
    void irq();

    uint8_t get_op_code();
    void dummy_read();
    uint8_t read_byte();
    uint16_t read_word();
    uint16_t fetch_operand();

    uint8_t memory_read(uint16_t addr);
    void memory_write(uint16_t addr, uint8_t value);
    uint16_t memory_read_word(uint16_t addr);

    void set_register(uint8_t& reg, uint8_t value);
    void push(uint8_t value);
    void push_word(uint16_t value);
    uint8_t pop();
    uint16_t pop_word();

    uint8_t a() { return state_.a; }
    void set_a(uint8_t v) { set_register(state_.a, v); }
    uint8_t x() { return state_.x; }
    void set_x(uint8_t v) { set_register(state_.x, v); }
    uint8_t y() { return state_.y; }
    void set_y(uint8_t v) { set_register(state_.y, v); }
    uint8_t sp() { return state_.sp; }
    void set_sp(uint8_t v) { state_.sp = v; }
    uint8_t ps() { return state_.ps; }
    void set_ps(uint8_t v) { state_.ps = v & 0xCF; }
    uint16_t pc() { return state_.pc; }
    void set_pc(uint16_t v) { state_.pc = v; }

    void clear_flags(uint8_t f) { state_.ps &= ~f; }
    void set_flags(uint8_t f) { state_.ps |= f; }
    bool check_flag(uint8_t f) { return (state_.ps & f) != 0; }
    void set_zn_flags(uint8_t v);
    bool check_page_crossed(uint16_t a, int8_t b) { return ((a + b) & 0xFF00) != (a & 0xFF00); }
    bool check_page_crossed(uint16_t a, uint8_t b) { return ((a + b) & 0xFF00) != (a & 0xFF00); }

    uint8_t get_operand_value();
    uint16_t get_ind_addr();
    uint8_t get_immediate();
    uint8_t get_zero_addr();
    uint8_t get_zero_x_addr();
    uint8_t get_zero_y_addr();
    uint16_t get_abs_addr();
    uint16_t get_abs_x_addr(bool dr);
    uint16_t get_abs_y_addr(bool dr);
    uint16_t get_ind();
    uint16_t get_ind_x_addr();
    uint16_t get_ind_y_addr(bool dr);

    void and_op();
    void eor_op();
    void ora_op();
    void add(uint8_t v);
    void adc_op();
    void sbc_op();
    void cmp(uint8_t r, uint8_t v);
    void cpa();
    void cpx();
    void cpy();
    void inc_op();
    void dec_op();
    uint8_t asl(uint8_t v);
    uint8_t lsr(uint8_t v);
    uint8_t rol(uint8_t v);
    uint8_t ror(uint8_t v);
    void asl_addr();
    void lsr_addr();
    void rol_addr();
    void ror_addr();
    void jmp(uint16_t a) { set_pc(a); }
    void branch_relative(bool branch);

    // Opcodes
    void op_lda() { set_a(get_operand_value()); }
    void op_ldx() { set_x(get_operand_value()); }
    void op_ldy() { set_y(get_operand_value()); }
    void op_sta() { memory_write(operand_, a()); }
    void op_stx() { memory_write(operand_, x()); }
    void op_sty() { memory_write(operand_, y()); }
    void op_tax() { set_x(a()); }
    void op_tay() { set_y(a()); }
    void op_tsx() { set_x(sp()); }
    void op_txa() { set_a(x()); }
    void op_txs() { set_sp(x()); }
    void op_tya() { set_a(y()); }
    void op_pha();
    void op_php();
    void op_pla();
    void op_plp();
    void op_inx() { set_x(x() + 1); }
    void op_iny() { set_y(y() + 1); }
    void op_dex() { set_x(x() - 1); }
    void op_dey() { set_y(y() - 1); }
    void op_asl_acc() { set_a(asl(a())); }
    void op_asl_mem() { asl_addr(); }
    void op_lsr_acc() { set_a(lsr(a())); }
    void op_lsr_mem() { lsr_addr(); }
    void op_rol_acc() { set_a(rol(a())); }
    void op_rol_mem() { rol_addr(); }
    void op_ror_acc() { set_a(ror(a())); }
    void op_ror_mem() { ror_addr(); }
    void op_jmp_abs() { jmp(operand_); }
    void op_jmp_ind() { jmp(get_ind()); }
    void op_jsr();
    void op_rts();
    void op_rti();
    void op_bcc() { branch_relative(!check_flag(PSFlags::CARRY)); }
    void op_bcs() { branch_relative(check_flag(PSFlags::CARRY)); }
    void op_beq() { branch_relative(check_flag(PSFlags::ZERO)); }
    void op_bmi() { branch_relative(check_flag(PSFlags::NEGATIVE)); }
    void op_bne() { branch_relative(!check_flag(PSFlags::ZERO)); }
    void op_bpl() { branch_relative(!check_flag(PSFlags::NEGATIVE)); }
    void op_bvc() { branch_relative(!check_flag(PSFlags::OVERFLOW_FLAG)); }
    void op_bvs() { branch_relative(check_flag(PSFlags::OVERFLOW_FLAG)); }
    void op_clc() { clear_flags(PSFlags::CARRY); }
    void op_cld() { clear_flags(PSFlags::DECIMAL); }
    void op_cli() { clear_flags(PSFlags::INTERRUPT); }
    void op_clv() { clear_flags(PSFlags::OVERFLOW_FLAG); }
    void op_sec() { set_flags(PSFlags::CARRY); }
    void op_sed() { set_flags(PSFlags::DECIMAL); }
    void op_sei() { set_flags(PSFlags::INTERRUPT); }
    void op_brk();
    void op_nop();
    void op_bit();
    void op_slo();
    void op_sre();
    void op_rla();
    void op_rra();
    void op_sax();
    void op_lax();
    void op_dcp();
    void op_isb();
    void op_aac();
    void op_asr();
    void op_arr();
    void op_atx();
    void op_axs();
    void op_shy();
    void op_shx();
    void op_shaa();
    void op_shaz();
    void op_tas();
    void op_ane();
    void op_las();
    void op_hlt();
};

} // namespace ear6::nes
