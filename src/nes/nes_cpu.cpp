#include "nes_console.h"
#include "nes_memory_manager.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include "nes_control_manager.h"
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include "nes_cpu.h"

namespace ear6::nes {

#ifdef EAR6_ENABLE_CPU_TRACE
static bool is_cpu_trace_enabled() {
    return std::getenv("EAR6_TRACE_CPU") != nullptr;
}

void NesCpu::trace_cpu(const char* fmt, ...) {
    if (!is_cpu_trace_enabled()) {
        return;
    }

    fprintf(stderr, "[CPU] %12llu ", (unsigned long long)state_.cycle_count);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#else
void NesCpu::trace_cpu(const char* fmt, ...) { (void)fmt; }
#endif

#if defined(EAR6_ENABLE_CPU8448_TRACE)
static bool is_cpu8448_trace_enabled() {
    return std::getenv("EAR6_TRACE_CPU8448") != nullptr;
}

static NesCpu* g_probe_cpu = nullptr;
static uint16_t g_probe_addr = 0;
static uint16_t g_exec_pc = 0;
static uint8_t g_exec_op = 0;
static bool g_exec_trace_rw = false;
#define TRACE_CPU8448(tag, pcv, opv) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)pcv; (void)opv; } } while (0)
#define TRACE_CPU8448_STATE(tag, pcv, opv) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)pcv; (void)opv; } } while (0)
#define TRACE_CPU8448_MEM(tag, addrv, valv) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)addrv; (void)valv; } } while (0)
#define TRACE_CPU8448_MEMW(addrv, valv) do { if (is_cpu8448_trace_enabled()) { (void)addrv; (void)valv; } } while (0)
#define TRACE_CPU8448_X(tag, pcv, opv, xb, xa) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)pcv; (void)opv; (void)xb; (void)xa; } } while (0)
#define TRACE_AD_PROBE_ADDR(pcv, opv, addrv) do { if (is_cpu8448_trace_enabled()) { (void)pcv; (void)opv; (void)addrv; } } while (0)
#define TRACE_AD_PROBE_VAL(addrv, valv) do { if (is_cpu8448_trace_enabled()) { (void)addrv; (void)valv; } } while (0)
#define TRACE_RW_PROBE(tag, addrv, valv) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)addrv; (void)valv; } } while (0)
#define TRACE_BADSTEP(tag, pcv, opv) do { if (is_cpu8448_trace_enabled()) { (void)tag; (void)pcv; (void)opv; } } while (0)
#else
static NesCpu* g_probe_cpu = nullptr;
static uint16_t g_probe_addr = 0;
static uint16_t g_exec_pc = 0;
static uint8_t g_exec_op = 0;
static bool g_exec_trace_rw = false;
#define TRACE_CPU8448(tag, pcv, opv) do {} while (0)
#define TRACE_CPU8448_STATE(tag, pcv, opv) do {} while (0)
#define TRACE_CPU8448_MEM(tag, addrv, valv) do {} while (0)
#define TRACE_CPU8448_MEMW(addrv, valv) do {} while (0)
#define TRACE_CPU8448_X(tag, pcv, opv, xb, xa) do {} while (0)
#define TRACE_AD_PROBE_ADDR(pcv, opv, addrv) do {} while (0)
#define TRACE_AD_PROBE_VAL(addrv, valv) do {} while (0)
#define TRACE_RW_PROBE(tag, addrv, valv) do {} while (0)
#define TRACE_BADSTEP(tag, pcv, opv) do {} while (0)
#endif

NesCpu::NesCpu(NesConsole* console) {
    console_ = console;
    memory_manager_ = console->get_memory_manager();

    Func opTable[256] = {
        &NesCpu::op_brk, &NesCpu::ora_op,    &NesCpu::op_hlt, &NesCpu::op_slo,  &NesCpu::op_nop,  &NesCpu::ora_op,    &NesCpu::op_asl_mem, &NesCpu::op_slo,
        &NesCpu::op_php, &NesCpu::ora_op,    &NesCpu::op_asl_acc,&NesCpu::op_aac,&NesCpu::op_nop,  &NesCpu::ora_op,    &NesCpu::op_asl_mem, &NesCpu::op_slo,
        &NesCpu::op_bpl, &NesCpu::ora_op,    &NesCpu::op_hlt, &NesCpu::op_slo,  &NesCpu::op_nop,  &NesCpu::ora_op,    &NesCpu::op_asl_mem, &NesCpu::op_slo,
        &NesCpu::op_clc, &NesCpu::ora_op,    &NesCpu::op_nop, &NesCpu::op_slo,  &NesCpu::op_nop,  &NesCpu::ora_op,    &NesCpu::op_asl_mem, &NesCpu::op_slo,
        &NesCpu::op_jsr, &NesCpu::and_op,    &NesCpu::op_hlt, &NesCpu::op_rla,  &NesCpu::op_bit,  &NesCpu::and_op,    &NesCpu::op_rol_mem, &NesCpu::op_rla,
        &NesCpu::op_plp, &NesCpu::and_op,    &NesCpu::op_rol_acc,&NesCpu::op_aac,&NesCpu::op_bit,  &NesCpu::and_op,    &NesCpu::op_rol_mem, &NesCpu::op_rla,
        &NesCpu::op_bmi, &NesCpu::and_op,    &NesCpu::op_hlt, &NesCpu::op_rla,  &NesCpu::op_nop,  &NesCpu::and_op,    &NesCpu::op_rol_mem, &NesCpu::op_rla,
        &NesCpu::op_sec, &NesCpu::and_op,    &NesCpu::op_nop, &NesCpu::op_rla,  &NesCpu::op_nop,  &NesCpu::and_op,    &NesCpu::op_rol_mem, &NesCpu::op_rla,
        &NesCpu::op_rti, &NesCpu::eor_op,    &NesCpu::op_hlt, &NesCpu::op_sre,  &NesCpu::op_nop,  &NesCpu::eor_op,    &NesCpu::op_lsr_mem, &NesCpu::op_sre,
        &NesCpu::op_pha, &NesCpu::eor_op,    &NesCpu::op_lsr_acc,&NesCpu::op_asr,&NesCpu::op_jmp_abs,&NesCpu::eor_op,  &NesCpu::op_lsr_mem, &NesCpu::op_sre,
        &NesCpu::op_bvc, &NesCpu::eor_op,    &NesCpu::op_hlt, &NesCpu::op_sre,  &NesCpu::op_nop,  &NesCpu::eor_op,    &NesCpu::op_lsr_mem, &NesCpu::op_sre,
        &NesCpu::op_cli, &NesCpu::eor_op,    &NesCpu::op_nop, &NesCpu::op_sre,  &NesCpu::op_nop,  &NesCpu::eor_op,    &NesCpu::op_lsr_mem, &NesCpu::op_sre,
        &NesCpu::op_rts, &NesCpu::adc_op,    &NesCpu::op_hlt, &NesCpu::op_rra,  &NesCpu::op_nop,  &NesCpu::adc_op,    &NesCpu::op_ror_mem, &NesCpu::op_rra,
        &NesCpu::op_pla, &NesCpu::adc_op,    &NesCpu::op_ror_acc,&NesCpu::op_arr,&NesCpu::op_jmp_ind,&NesCpu::adc_op,  &NesCpu::op_ror_mem, &NesCpu::op_rra,
        &NesCpu::op_bvs, &NesCpu::adc_op,    &NesCpu::op_hlt, &NesCpu::op_rra,  &NesCpu::op_nop,  &NesCpu::adc_op,    &NesCpu::op_ror_mem, &NesCpu::op_rra,
        &NesCpu::op_sei, &NesCpu::adc_op,    &NesCpu::op_nop, &NesCpu::op_rra,  &NesCpu::op_nop,  &NesCpu::adc_op,    &NesCpu::op_ror_mem, &NesCpu::op_rra,
        &NesCpu::op_nop, &NesCpu::op_sta,    &NesCpu::op_nop, &NesCpu::op_sax,  &NesCpu::op_sty,  &NesCpu::op_sta,    &NesCpu::op_stx,     &NesCpu::op_sax,
        &NesCpu::op_dey, &NesCpu::op_nop,    &NesCpu::op_txa, &NesCpu::op_ane,  &NesCpu::op_sty,  &NesCpu::op_sta,    &NesCpu::op_stx,     &NesCpu::op_sax,
        &NesCpu::op_bcc, &NesCpu::op_sta,    &NesCpu::op_hlt, &NesCpu::op_shaz, &NesCpu::op_sty,  &NesCpu::op_sta,    &NesCpu::op_stx,     &NesCpu::op_sax,
        &NesCpu::op_tya, &NesCpu::op_sta,    &NesCpu::op_txs, &NesCpu::op_tas,  &NesCpu::op_shy,  &NesCpu::op_sta,    &NesCpu::op_shx,     &NesCpu::op_shaa,
        &NesCpu::op_ldy, &NesCpu::op_lda,    &NesCpu::op_ldx, &NesCpu::op_lax,  &NesCpu::op_ldy,  &NesCpu::op_lda,    &NesCpu::op_ldx,     &NesCpu::op_lax,
        &NesCpu::op_tay, &NesCpu::op_lda,    &NesCpu::op_tax, &NesCpu::op_atx,  &NesCpu::op_ldy,  &NesCpu::op_lda,    &NesCpu::op_ldx,     &NesCpu::op_lax,
        &NesCpu::op_bcs, &NesCpu::op_lda,    &NesCpu::op_hlt, &NesCpu::op_lax,  &NesCpu::op_ldy,  &NesCpu::op_lda,    &NesCpu::op_ldx,     &NesCpu::op_lax,
        &NesCpu::op_clv, &NesCpu::op_lda,    &NesCpu::op_tsx, &NesCpu::op_las,  &NesCpu::op_ldy,  &NesCpu::op_lda,    &NesCpu::op_ldx,     &NesCpu::op_lax,
        &NesCpu::cpy, &NesCpu::cpa,       &NesCpu::op_nop, &NesCpu::op_dcp,  &NesCpu::cpy,  &NesCpu::cpa,       &NesCpu::dec_op,     &NesCpu::op_dcp,
        &NesCpu::op_iny, &NesCpu::cpa,       &NesCpu::op_dex, &NesCpu::op_axs,  &NesCpu::cpy,  &NesCpu::cpa,       &NesCpu::dec_op,     &NesCpu::op_dcp,
        &NesCpu::op_bne, &NesCpu::cpa,       &NesCpu::op_hlt, &NesCpu::op_dcp,  &NesCpu::op_nop,  &NesCpu::cpa,       &NesCpu::dec_op,     &NesCpu::op_dcp,
        &NesCpu::op_cld, &NesCpu::cpa,       &NesCpu::op_nop, &NesCpu::op_dcp,  &NesCpu::op_nop,  &NesCpu::cpa,       &NesCpu::dec_op,     &NesCpu::op_dcp,
        &NesCpu::cpx, &NesCpu::sbc_op,    &NesCpu::op_nop, &NesCpu::op_isb,  &NesCpu::cpx,  &NesCpu::sbc_op,    &NesCpu::inc_op,     &NesCpu::op_isb,
        &NesCpu::op_inx, &NesCpu::sbc_op,    &NesCpu::op_nop, &NesCpu::sbc_op,  &NesCpu::cpx,  &NesCpu::sbc_op,    &NesCpu::inc_op,     &NesCpu::op_isb,
        &NesCpu::op_beq, &NesCpu::sbc_op,    &NesCpu::op_hlt, &NesCpu::op_isb,  &NesCpu::op_nop,  &NesCpu::sbc_op,    &NesCpu::inc_op,     &NesCpu::op_isb,
        &NesCpu::op_sed, &NesCpu::sbc_op,    &NesCpu::op_nop, &NesCpu::op_isb,  &NesCpu::op_nop,  &NesCpu::sbc_op,    &NesCpu::inc_op,     &NesCpu::op_isb,
    };

    NesAddrMode addrMode[256] = {
        NesAddrMode::IMP, NesAddrMode::IND_X,   NesAddrMode::NONE, NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::ACC,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
        NesAddrMode::OTHER,NesAddrMode::IND_X,  NesAddrMode::NONE, NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::ACC,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
        NesAddrMode::IMP, NesAddrMode::IND_X,   NesAddrMode::NONE, NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::ACC,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
        NesAddrMode::IMP, NesAddrMode::IND_X,   NesAddrMode::NONE, NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::ACC,  NesAddrMode::IMM,    NesAddrMode::IND,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
        NesAddrMode::IMM, NesAddrMode::IND_X,   NesAddrMode::IMM,  NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::IMP,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_YW,  NesAddrMode::NONE, NesAddrMode::OTHER,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_Y,NesAddrMode::ZERO_Y,
        NesAddrMode::IMP, NesAddrMode::ABS_YW,  NesAddrMode::IMP,  NesAddrMode::OTHER,  NesAddrMode::OTHER,NesAddrMode::ABS_XW,NesAddrMode::OTHER,NesAddrMode::OTHER,
        NesAddrMode::IMM, NesAddrMode::IND_X,   NesAddrMode::IMM,  NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::IMP,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_Y,   NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_Y,NesAddrMode::ZERO_Y,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_Y,   NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_Y, NesAddrMode::ABS_Y,
        NesAddrMode::IMM, NesAddrMode::IND_X,   NesAddrMode::IMM,  NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::IMP,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
        NesAddrMode::IMM, NesAddrMode::IND_X,   NesAddrMode::IMM,  NesAddrMode::IND_X,   NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO, NesAddrMode::ZERO,
        NesAddrMode::IMP, NesAddrMode::IMM,    NesAddrMode::IMP,  NesAddrMode::IMM,    NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,  NesAddrMode::ABS,
        NesAddrMode::REL, NesAddrMode::IND_Y,   NesAddrMode::NONE, NesAddrMode::IND_YW,  NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,NesAddrMode::ZERO_X,
        NesAddrMode::IMP, NesAddrMode::ABS_Y,   NesAddrMode::IMP,  NesAddrMode::ABS_YW,  NesAddrMode::ABS_X, NesAddrMode::ABS_X, NesAddrMode::ABS_XW,NesAddrMode::ABS_XW,
    };

    memcpy(op_table_, opTable, sizeof(opTable));
    memcpy(addr_mode_, addrMode, sizeof(addrMode));

}

void NesCpu::reset(bool soft_reset) {
    (void)soft_reset;
    state_.nmi_flag = false;
    state_.irq_flag = 0;
    sprite_dma_transfer_ = false;
    sprite_dma_offset_ = 0;
    need_halt_ = false;
    dmc_dma_running_ = false;
    abort_dmc_dma_ = false;
    cpu_write_ = false;

    state_.pc = memory_manager_->read(RESET_VECTOR) | (memory_manager_->read(RESET_VECTOR + 1) << 8);

    state_.a = 0;
    state_.sp = 0xFD;
    state_.x = 0;
    state_.y = 0;
    state_.ps = PSFlags::INTERRUPT;
    run_irq_ = false;
    prev_run_irq_ = false;
    need_nmi_ = false;
    prev_need_nmi_ = false;
    prev_nmi_flag_ = false;

    state_.cycle_count = (uint64_t)-1;
    master_clock_ = 0;
    master_clock_ += 12; // NTSC: 12 master clocks per CPU cycle

    // 8 dummy cycles after reset
    for (int i = 0; i < 8; i++) {
        start_cpu_cycle(true);
        end_cpu_cycle(true);
    }
}

void NesCpu::start_cpu_cycle(bool for_read) {
    master_clock_ += for_read ? (start_clock_count_ - 1) : (start_clock_count_ + 1);
    state_.cycle_count++;
    console_->get_ppu()->run(master_clock_ - ppu_offset_);
    console_->process_cpu_clock();
}

void NesCpu::end_cpu_cycle(bool for_read) {
    master_clock_ += for_read ? (end_clock_count_ + 1) : (end_clock_count_ - 1);
    console_->get_ppu()->run(master_clock_ - ppu_offset_);

    prev_need_nmi_ = need_nmi_;
    if (!prev_nmi_flag_ && state_.nmi_flag) {
        need_nmi_ = true;
    }
    prev_nmi_flag_ = state_.nmi_flag;

    prev_run_irq_ = run_irq_;
    run_irq_ = ((state_.irq_flag & irq_mask_) > 0 && !check_flag(PSFlags::INTERRUPT));

    trace_cpu("IRQ_FLAGS irq=%02X run_irq=%d prev_run=%d nmi=%d prev_nmi=%d need_nmi=%d\n",
        state_.irq_flag, run_irq_, prev_run_irq_, state_.nmi_flag, prev_nmi_flag_, need_nmi_);

}

uint8_t NesCpu::get_op_code() {
    uint8_t op = memory_read(state_.pc);
    state_.pc++;
    return op;
}

void NesCpu::dummy_read() {
    memory_read(state_.pc);
}

uint8_t NesCpu::read_byte() {
    uint8_t v = memory_read(state_.pc);
    state_.pc++;
    return v;
}

uint16_t NesCpu::read_word() {
    uint8_t lo = read_byte();
    uint8_t hi = read_byte();
    return (hi << 8) | lo;
}

uint16_t NesCpu::fetch_operand() {
    switch (inst_addr_mode_) {
        case NesAddrMode::ACC:
        case NesAddrMode::IMP: dummy_read(); return 0;
        case NesAddrMode::IMM:
        case NesAddrMode::REL: return get_immediate();
        case NesAddrMode::ZERO: return get_zero_addr();
        case NesAddrMode::ZERO_X: return get_zero_x_addr();
        case NesAddrMode::ZERO_Y: return get_zero_y_addr();
        case NesAddrMode::IND: return get_ind_addr();
        case NesAddrMode::IND_X: return get_ind_x_addr();
        case NesAddrMode::IND_Y: return get_ind_y_addr(false);
        case NesAddrMode::IND_YW: return get_ind_y_addr(true);
        case NesAddrMode::ABS: return get_abs_addr();
        case NesAddrMode::ABS_X: return get_abs_x_addr(false);
        case NesAddrMode::ABS_XW: return get_abs_x_addr(true);
        case NesAddrMode::ABS_Y: return get_abs_y_addr(false);
        case NesAddrMode::ABS_YW: return get_abs_y_addr(true);
        case NesAddrMode::OTHER: return 0;
        default: return 0;
    }
}

uint8_t NesCpu::memory_read(uint16_t addr) {
    process_pending_dma(addr);
    start_cpu_cycle(true);
    uint8_t v = memory_manager_->read(addr);
    end_cpu_cycle(true);
    if (std::getenv("EAR6_TRACE_D131") != nullptr && g_exec_pc == 0xD131) {
        NesPpu* p = console_->get_ppu();
        fprintf(stderr,
            "[EAR6_D131_READ] pc=%04X op=%02X addr=%04X val=%02X f=%u sl=%d cy=%d cpu=%llu\n",
            g_exec_pc,
            g_exec_op,
            addr,
            v,
            p ? p->get_frame_count() : 0,
            p ? p->get_scanline() : 0,
            p ? p->get_cycle() : 0,
            (unsigned long long)state_.cycle_count);
    }
    if (std::getenv("EAR6_TRACE_C294") != nullptr && g_exec_pc == 0xC294) {
        NesPpu* p = console_->get_ppu();
        fprintf(stderr,
            "[EAR6_C294_READ] pc=%04X op=%02X addr=%04X val=%02X f=%u sl=%d cy=%d cpu=%llu a=%02X x=%02X y=%02X\n",
            g_exec_pc,
            g_exec_op,
            addr,
            v,
            p ? p->get_frame_count() : 0,
            p ? p->get_scanline() : 0,
            p ? p->get_cycle() : 0,
            (unsigned long long)state_.cycle_count,
            state_.a,
            state_.x,
            state_.y);
    }
    TRACE_AD_PROBE_VAL(addr, v);
    TRACE_RW_PROBE("R", addr, v);
    TRACE_CPU8448_MEM("READ", addr, v);
    return v;
}

void NesCpu::memory_write(uint16_t addr, uint8_t value) {
    TRACE_CPU8448_MEMW(addr, value);
    TRACE_RW_PROBE("W", addr, value);
    cpu_write_ = true;
    start_cpu_cycle(false);
    memory_manager_->write(addr, value);
    end_cpu_cycle(false);
    cpu_write_ = false;
}

uint16_t NesCpu::memory_read_word(uint16_t addr) {
    uint8_t lo = memory_read(addr);
    uint8_t hi = memory_read(addr + 1);
    return lo | (hi << 8);
}

void NesCpu::set_register(uint8_t& reg, uint8_t value) {
    clear_flags(PSFlags::ZERO | PSFlags::NEGATIVE);
    set_zn_flags(value);
    reg = value;
}

void NesCpu::set_zn_flags(uint8_t value) {
    if (value == 0) set_flags(PSFlags::ZERO);
    else if (value & 0x80) set_flags(PSFlags::NEGATIVE);
}

void NesCpu::push(uint8_t value) {
    memory_write(0x100 + state_.sp, value);
    state_.sp--;
}

void NesCpu::push_word(uint16_t value) {
    push((uint8_t)(value >> 8));
    push((uint8_t)value);
}

uint8_t NesCpu::pop() {
    state_.sp++;
    return memory_read(0x100 + state_.sp);
}

uint16_t NesCpu::pop_word() {
    uint8_t lo = pop();
    uint8_t hi = pop();
    return lo | (hi << 8);
}

uint8_t NesCpu::get_operand_value() {
    if (inst_addr_mode_ >= NesAddrMode::ZERO) {
        return memory_read(operand_);
    } else {
        return (uint8_t)operand_;
    }
}

void NesCpu::exec() {
    uint16_t pc_before = state_.pc;
    uint8_t opcode = get_op_code();
#ifdef EAR6_ENABLE_CPU_SEQ_TRACE
    if (std::getenv("EAR6_TRACE_CPU_SEQ") != nullptr) {
        NesPpu* p = console_->get_ppu();
        fprintf(stderr,
            "[EAR6_CPU_SEQ] f=%u sl=%d cy=%d cpu=%llu pc=%04X op=%02X a=%02X x=%02X y=%02X sp=%02X ps=%02X\n",
            p ? p->get_frame_count() : 0,
            p ? p->get_scanline() : 0,
            p ? p->get_cycle() : 0,
            (unsigned long long)state_.cycle_count,
            pc_before,
            opcode,
            state_.a,
            state_.x,
            state_.y,
            state_.sp,
            state_.ps);
    }
#endif
    #if defined(EAR6_ENABLE_MINIMAL_BAD_WINDOW_TRACE)
    const bool minimal_bad_window_trace_enabled = std::getenv("EAR6_TRACE_MINIMAL_BAD_WINDOW") != nullptr;
    if (minimal_bad_window_trace_enabled) {
    if (pc_before == 0xBADD) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[BADD] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if (pc_before == 0xB84A) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[B84A] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if (pc_before == 0xB85B) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[B85B] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if (pc_before == 0xB87D) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[B87D] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if (pc_before == 0xB880) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[B880] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if (pc_before == 0xB885) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[B885] cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
    }
    if ((pc_before == 0xBAD2 || pc_before == 0xBAD5 || pc_before == 0xBAD7 || pc_before == 0xBAD9 || pc_before == 0xBADB) &&
        console_->get_ppu()->get_frame_count() == 29) {
        uint8_t* iram = memory_manager_->get_internal_ram();
        NesPpu* p = console_->get_ppu();
        fprintf(stderr, "[BADSEQ] BEFORE pc=%04X cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X zf5=%02X zf6=%02X zf7=%02X zf8=%02X f=%u sl=%d cy=%d\n",
            pc_before, (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
            iram[0xF5], iram[0xF6], iram[0xF7], iram[0xF8],
            p->get_frame_count(), p->get_scanline(), p->get_cycle());
    }
    }
    #endif
    g_exec_pc = pc_before;
    g_exec_op = opcode;
    {
        NesPpu* p = console_->get_ppu();
        uint32_t f = p ? p->get_frame_count() : 0;
        g_exec_trace_rw = (pc_before >= 0xBAD0 && pc_before <= 0xBAE0 && f >= 16 && f <= 18);
    }
    TRACE_CPU8448("EXEC", pc_before, opcode);
    TRACE_CPU8448_STATE("BEFORE", pc_before, opcode);
    TRACE_BADSTEP("BEFORE", pc_before, opcode);
    inst_addr_mode_ = addr_mode_[opcode];
    operand_ = fetch_operand();
    TRACE_AD_PROBE_ADDR(pc_before, opcode, operand_);
    if (pc_before == 0xB84C && opcode == 0xAD) {
        g_probe_cpu = this;
        g_probe_addr = operand_;
    }
    (this->*op_table_[opcode])();
    #if defined(EAR6_ENABLE_MINIMAL_BAD_WINDOW_TRACE)
    if (minimal_bad_window_trace_enabled) {
    if ((pc_before == 0xBAD2 || pc_before == 0xBAD5 || pc_before == 0xBAD7 || pc_before == 0xBAD9 || pc_before == 0xBADB) &&
        console_->get_ppu()->get_frame_count() == 29) {
        uint8_t* iram = memory_manager_->get_internal_ram();
        NesPpu* p = console_->get_ppu();
        fprintf(stderr, "[BADSEQ] AFTER  pc=%04X cpu=%llu a=%02X x=%02X y=%02X sp=%02X ps=%02X zf5=%02X zf6=%02X zf7=%02X zf8=%02X f=%u sl=%d cy=%d\n",
            pc_before, (unsigned long long)state_.cycle_count, state_.a, state_.x, state_.y, state_.sp, state_.ps,
            iram[0xF5], iram[0xF6], iram[0xF7], iram[0xF8],
            p->get_frame_count(), p->get_scanline(), p->get_cycle());
    }
    }
    #endif
    if (g_probe_cpu == this) {
        g_probe_cpu = nullptr;
    }
    g_exec_trace_rw = false;
    TRACE_CPU8448_X("EXEC", pc_before, opcode, state_.x, state_.x);
    TRACE_CPU8448_STATE("AFTER", pc_before, opcode);
    TRACE_BADSTEP("AFTER", pc_before, opcode);

    if (prev_run_irq_ || prev_need_nmi_) {
        #if defined(EAR6_ENABLE_CYCLE_ALIGN_TRACE)
        if (std::getenv("EAR6_TRACE_CYCLE_ALIGN") != nullptr) {
        NesPpu* p = console_->get_ppu();
        if (p) {
            fprintf(stderr, "[EAR6_EVT] IRQ_ENTRY cpu=%llu pc=%04X nmi=%d need_nmi=%d run_irq=%d prev_run=%d f=%u sl=%d cy=%d\n",
                (unsigned long long)state_.cycle_count, state_.pc, (int)state_.nmi_flag, (int)need_nmi_, (int)run_irq_, (int)prev_run_irq_,
                p->get_frame_count(), p->get_scanline(), p->get_cycle());
        }
        }
        #endif
        TRACE_CPU8448("IRQ_ENTRY", state_.pc, opcode);
        irq();
    }
}

void NesCpu::irq() {
    dummy_read();
    dummy_read();
    push_word(state_.pc);

    if (need_nmi_) {
        need_nmi_ = false;
        push(ps() | PSFlags::RESERVED);
        set_flags(PSFlags::INTERRUPT);
        set_pc(memory_read_word(NMI_VECTOR));
    } else {
        push(ps() | PSFlags::RESERVED);
        set_flags(PSFlags::INTERRUPT);
        set_pc(memory_read_word(IRQ_VECTOR));
    }
}

void NesCpu::process_pending_dma(uint16_t read_address) {
    if (!need_halt_) return;

    uint16_t prev_read_address = read_address;
    bool enable_internal_reg_reads = (read_address & 0xFFE0) == 0x4000;
    bool skip_first_input_clock = false;
    if (enable_internal_reg_reads && dmc_dma_running_ && (read_address == 0x4016 || read_address == 0x4017)) {
        uint16_t dmc_address = console_->get_apu()->get_dmc_read_address();
        if ((dmc_address & 0x1F) == (read_address & 0x1F)) {
            skip_first_input_clock = true;
        }
    }

    bool is_nes_behavior = true;
    bool skip_dummy_reads = is_nes_behavior && (read_address == 0x4016 || read_address == 0x4017);

    auto process_dma_read = [&](uint16_t addr) -> uint8_t {
        if (!enable_internal_reg_reads) {
            uint8_t val;
            if (addr >= 0x4000 && addr <= 0x401F) {
                val = memory_manager_->get_open_bus();
            } else {
                val = memory_manager_->read(addr);
            }
            prev_read_address = addr;
            return val;
        }

        uint16_t internal_addr = 0x4000 | (addr & 0x1F);
        bool is_same_address = internal_addr == addr;
        uint8_t val = 0;

        switch (internal_addr) {
            case 0x4015:
                val = memory_manager_->read(internal_addr);
                if (!is_same_address) {
                    memory_manager_->read(addr);
                }
                break;

            case 0x4016:
            case 0x4017: {
                if (is_nes_behavior && prev_read_address == internal_addr) {
                    val = memory_manager_->get_open_bus();
                } else {
                    val = memory_manager_->read(internal_addr);
                }

                if (!is_same_address) {
                    uint8_t ob_mask = console_->get_control_manager()->get_open_bus_mask((uint8_t)(internal_addr - 0x4016));
                    uint8_t external_value = memory_manager_->read(addr);
                    val = (external_value & ob_mask) | ((val & ~ob_mask) & (external_value & ~ob_mask));
                }
                break;
            }

            default:
                val = memory_manager_->read(addr);
                break;
        }

        prev_read_address = internal_addr;
        return val;
    };

    need_halt_ = false;

    trace_cpu("DMA_START offset=%02X\n", sprite_dma_offset_);

    uint64_t dma_start_cycle = 0;
    (void)dma_start_cycle;

    start_cpu_cycle(true);
    if (!(abort_dmc_dma_ && is_nes_behavior && (read_address == 0x4016 || read_address == 0x4017)) && !skip_first_input_clock) {
        memory_manager_->read(read_address);
    }
    end_cpu_cycle(true);

    if (abort_dmc_dma_) {
        dmc_dma_running_ = false;
        abort_dmc_dma_ = false;
        if (!sprite_dma_transfer_) {
            need_dummy_read_ = false;
            return;
        }
    }

    uint16_t sprite_counter = 0;
    uint8_t sprite_read_addr = 0;
    uint8_t read_value = 0;

    while (dmc_dma_running_ || sprite_dma_transfer_) {
        bool get_cycle = (state_.cycle_count & 0x01) == 0;
        if (get_cycle) {
            if (dmc_dma_running_ && !need_halt_ && !need_dummy_read_) {
                if (abort_dmc_dma_) {
                    dmc_dma_running_ = false;
                    abort_dmc_dma_ = false;
                    need_dummy_read_ = false;
                    need_halt_ = false;
                } else if (need_halt_) {
                    need_halt_ = false;
                } else if (need_dummy_read_) {
                    need_dummy_read_ = false;
                }
                start_cpu_cycle(true);
                is_dmc_dma_read_ = true;
                uint16_t dmc_addr = console_->get_apu()->get_dmc_read_address();
                read_value = process_dma_read(dmc_addr);
                console_->get_apu()->set_dmc_read_buffer(read_value);
                is_dmc_dma_read_ = false;
                end_cpu_cycle(true);
                dmc_dma_running_ = false;
                abort_dmc_dma_ = false;
            } else if (sprite_dma_transfer_) {
                if (abort_dmc_dma_) {
                    dmc_dma_running_ = false;
                    abort_dmc_dma_ = false;
                    need_dummy_read_ = false;
                    need_halt_ = false;
                } else if (need_halt_) {
                    need_halt_ = false;
                } else if (need_dummy_read_) {
                    need_dummy_read_ = false;
                }
                start_cpu_cycle(true);
                read_value = process_dma_read((uint16_t)(sprite_dma_offset_ * 0x100 + sprite_read_addr));
                end_cpu_cycle(true);
                sprite_read_addr++;
                sprite_counter++;
            } else {
                if (abort_dmc_dma_) {
                    dmc_dma_running_ = false;
                    abort_dmc_dma_ = false;
                    need_dummy_read_ = false;
                    need_halt_ = false;
                } else if (need_halt_) {
                    need_halt_ = false;
                } else if (need_dummy_read_) {
                    need_dummy_read_ = false;
                }
                start_cpu_cycle(true);
                if (!skip_dummy_reads) {
                    memory_manager_->read(read_address);
                }
                end_cpu_cycle(true);
            }
        } else {
            if (sprite_dma_transfer_ && (sprite_counter & 0x01)) {
                if (abort_dmc_dma_) {
                    dmc_dma_running_ = false;
                    abort_dmc_dma_ = false;
                    need_dummy_read_ = false;
                    need_halt_ = false;
                } else if (need_halt_) {
                    need_halt_ = false;
                } else if (need_dummy_read_) {
                    need_dummy_read_ = false;
                }
                start_cpu_cycle(true);
                memory_manager_->write(0x2004, read_value);
                end_cpu_cycle(true);
                sprite_counter++;
                if (sprite_counter == 0x200) {
                    sprite_dma_transfer_ = false;
                }
            } else {
                if (abort_dmc_dma_) {
                    dmc_dma_running_ = false;
                    abort_dmc_dma_ = false;
                    need_dummy_read_ = false;
                    need_halt_ = false;
                } else if (need_halt_) {
                    need_halt_ = false;
                } else if (need_dummy_read_) {
                    need_dummy_read_ = false;
                }
                start_cpu_cycle(true);
                if (!skip_dummy_reads) {
                    memory_manager_->read(read_address);
                }
                end_cpu_cycle(true);
            }
        }
    }

    if (std::getenv("EAR6_TRACE_DMA401X") != nullptr) {
        fprintf(stderr,
            "[EAR6_DMA401X] rd=%04X en=%d skip1=%d skipd=%d prev=%04X\n",
            read_address,
            enable_internal_reg_reads ? 1 : 0,
            skip_first_input_clock ? 1 : 0,
            skip_dummy_reads ? 1 : 0,
            prev_read_address);
    }
    trace_cpu("DMA_END sprite_dma=%d dmc_dma=%d\n", sprite_dma_transfer_, dmc_dma_running_);
}

void NesCpu::run_dma_transfer(uint8_t offset_value) {
    sprite_dma_transfer_ = true;
    sprite_dma_offset_ = offset_value;
    need_halt_ = true;
}

void NesCpu::start_dmc_transfer() {
    dmc_dma_running_ = true;
    need_dummy_read_ = true;
    need_halt_ = true;
}

void NesCpu::stop_dmc_transfer() {
    if (dmc_dma_running_) {
        if (need_halt_) {
            dmc_dma_running_ = false;
            need_dummy_read_ = false;
            need_halt_ = false;
        } else {
            abort_dmc_dma_ = true;
        }
    }
}

// Addressing mode helpers
uint16_t NesCpu::get_ind_addr() { return read_word(); }
uint8_t NesCpu::get_immediate() { return read_byte(); }
uint8_t NesCpu::get_zero_addr() { return read_byte(); }
uint8_t NesCpu::get_zero_x_addr() {
    uint8_t v = read_byte();
    memory_read(v); // dummy read
    return v + x();
}
uint8_t NesCpu::get_zero_y_addr() {
    uint8_t v = read_byte();
    memory_read(v);
    return v + y();
}
uint16_t NesCpu::get_abs_addr() { return read_word(); }
uint16_t NesCpu::get_abs_x_addr(bool dr) {
    uint16_t base = read_word();
    bool crossed = check_page_crossed(base, x());
    if (crossed || dr) {
        memory_read(base + x() - (crossed ? 0x100 : 0));
    }
    return base + x();
}
uint16_t NesCpu::get_abs_y_addr(bool dr) {
    uint16_t base = read_word();
    bool crossed = check_page_crossed(base, y());
    if (crossed || dr) {
        memory_read(base + y() - (crossed ? 0x100 : 0));
    }
    return base + y();
}
uint16_t NesCpu::get_ind() {
    uint16_t addr = operand_;
    if ((addr & 0xFF) == 0xFF) {
        uint8_t lo = memory_read(addr);
        uint8_t hi = memory_read(addr - 0xFF);
        return lo | (hi << 8);
    } else {
        return memory_read_word(addr);
    }
}
uint16_t NesCpu::get_ind_x_addr() {
    uint8_t zero = read_byte();
    memory_read(zero);
    zero += x();
    if (zero == 0xFF) {
        uint8_t lo = memory_read(0xFF);
        uint8_t hi = memory_read(0x00);
        return lo | (hi << 8);
    }
    return memory_read_word(zero);
}
uint16_t NesCpu::get_ind_y_addr(bool dr) {
    uint8_t zero = read_byte();
    uint16_t addr;
    if (zero == 0xFF) {
        uint8_t lo = memory_read(0xFF);
        uint8_t hi = memory_read(0x00);
        addr = lo | (hi << 8);
    } else {
        addr = memory_read_word(zero);
    }
    bool crossed = check_page_crossed(addr, y());
    if (crossed || dr) {
        memory_read(addr + y() - (crossed ? 0x100 : 0));
    }
    return addr + y();
}

// ALU operations
void NesCpu::and_op() { set_a(a() & get_operand_value()); }
void NesCpu::eor_op() { set_a(a() ^ get_operand_value()); }
void NesCpu::ora_op() { set_a(a() | get_operand_value()); }

void NesCpu::add(uint8_t v) {
    uint16_t result = (uint16_t)a() + (uint16_t)v + (check_flag(PSFlags::CARRY) ? 1 : 0);
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::OVERFLOW_FLAG | PSFlags::ZERO);
    set_zn_flags((uint8_t)result);
    if (~(a() ^ v) & (a() ^ result) & 0x80) set_flags(PSFlags::OVERFLOW_FLAG);
    if (result > 0xFF) set_flags(PSFlags::CARRY);
    set_a((uint8_t)result);
}

void NesCpu::adc_op() { add(get_operand_value()); }
void NesCpu::sbc_op() { add(get_operand_value() ^ 0xFF); }

void NesCpu::cmp(uint8_t r, uint8_t v) {
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::ZERO);
    auto result = r - v;
    if (r >= v) set_flags(PSFlags::CARRY);
    if (r == v) set_flags(PSFlags::ZERO);
    if (result & 0x80) set_flags(PSFlags::NEGATIVE);
}

void NesCpu::cpa() { cmp(a(), get_operand_value()); }
void NesCpu::cpx() { cmp(x(), get_operand_value()); }
void NesCpu::cpy() { cmp(y(), get_operand_value()); }

void NesCpu::inc_op() {
    uint16_t addr = operand_;
    clear_flags(PSFlags::NEGATIVE | PSFlags::ZERO);
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    v++;
    set_zn_flags(v);
    memory_write(addr, v);
}

void NesCpu::dec_op() {
    uint16_t addr = operand_;
    clear_flags(PSFlags::NEGATIVE | PSFlags::ZERO);
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    v--;
    set_zn_flags(v);
    memory_write(addr, v);
}

uint8_t NesCpu::asl(uint8_t v) {
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::ZERO);
    if (v & 0x80) set_flags(PSFlags::CARRY);
    uint8_t r = v << 1;
    set_zn_flags(r);
    return r;
}

uint8_t NesCpu::lsr(uint8_t v) {
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::ZERO);
    if (v & 0x01) set_flags(PSFlags::CARRY);
    uint8_t r = v >> 1;
    set_zn_flags(r);
    return r;
}

uint8_t NesCpu::rol(uint8_t v) {
    bool carry = check_flag(PSFlags::CARRY);
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::ZERO);
    if (v & 0x80) set_flags(PSFlags::CARRY);
    uint8_t r = (v << 1) | (carry ? 0x01 : 0x00);
    set_zn_flags(r);
    return r;
}

uint8_t NesCpu::ror(uint8_t v) {
    bool carry = check_flag(PSFlags::CARRY);
    clear_flags(PSFlags::CARRY | PSFlags::NEGATIVE | PSFlags::ZERO);
    if (v & 0x01) set_flags(PSFlags::CARRY);
    uint8_t r = (v >> 1) | (carry ? 0x80 : 0x00);
    set_zn_flags(r);
    return r;
}

void NesCpu::asl_addr() {
    uint16_t addr = operand_;
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    memory_write(addr, asl(v));
}

void NesCpu::lsr_addr() {
    uint16_t addr = operand_;
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    memory_write(addr, lsr(v));
}

void NesCpu::rol_addr() {
    uint16_t addr = operand_;
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    memory_write(addr, rol(v));
}

void NesCpu::ror_addr() {
    uint16_t addr = operand_;
    uint8_t v = memory_read(addr);
    memory_write(addr, v);
    memory_write(addr, ror(v));
}

void NesCpu::branch_relative(bool branch) {
    int8_t offset = (int8_t)(operand_ & 0xFF);
    if (branch) {
        if (run_irq_ && !prev_run_irq_) run_irq_ = false;
        dummy_read();
        if (check_page_crossed(pc(), offset)) dummy_read();
        set_pc(pc() + offset);
    }
}

void NesCpu::op_brk() {
    push_word(pc() + 1);
    uint8_t flags = ps() | PSFlags::BREAK | PSFlags::RESERVED;
    if (need_nmi_) {
        need_nmi_ = false;
        push(flags);
        set_flags(PSFlags::INTERRUPT);
        set_pc(memory_read_word(NMI_VECTOR));
    } else {
        push(flags);
        set_flags(PSFlags::INTERRUPT);
        set_pc(memory_read_word(IRQ_VECTOR));
    }
    prev_need_nmi_ = false;
}

void NesCpu::op_nop() { get_operand_value(); }
void NesCpu::op_hlt() { state_.pc -= 1; prev_run_irq_ = false; prev_need_nmi_ = false; }

void NesCpu::op_bit() {
    uint8_t v = get_operand_value();
    clear_flags(PSFlags::ZERO | PSFlags::OVERFLOW_FLAG | PSFlags::NEGATIVE);
    if ((a() & v) == 0) set_flags(PSFlags::ZERO);
    if (v & 0x40) set_flags(PSFlags::OVERFLOW_FLAG);
    if (v & 0x80) set_flags(PSFlags::NEGATIVE);
}

void NesCpu::op_pha() { push(a()); }
void NesCpu::op_php() { push(ps() | PSFlags::BREAK | PSFlags::RESERVED); }
void NesCpu::op_pla() { dummy_read(); set_a(pop()); }
void NesCpu::op_plp() { dummy_read(); set_ps(pop()); }
void NesCpu::op_jsr() {
    uint8_t lo = read_byte();
    dummy_read();
    push_word(pc());
    uint16_t addr = (read_byte() << 8) | lo;
    jmp(addr);
}
void NesCpu::op_rts() {
    dummy_read();
    uint16_t addr = pop_word();
    dummy_read();
    set_pc(addr + 1);
}
void NesCpu::op_rti() {
    dummy_read();
    set_ps(pop());
    set_pc(pop_word());
}

// Unofficial opcodes
void NesCpu::op_slo() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    uint8_t sv = asl(v);
    set_a(a() | sv);
    memory_write(operand_, sv);
}
void NesCpu::op_sre() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    uint8_t sv = lsr(v);
    set_a(a() ^ sv);
    memory_write(operand_, sv);
}
void NesCpu::op_rla() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    uint8_t sv = rol(v);
    set_a(a() & sv);
    memory_write(operand_, sv);
}
void NesCpu::op_rra() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    uint8_t sv = ror(v);
    add(sv);
    memory_write(operand_, sv);
}
void NesCpu::op_sax() { memory_write(operand_, a() & x()); }
void NesCpu::op_lax() {
    uint8_t v = get_operand_value();
    set_x(v);
    set_a(v);
}
void NesCpu::op_dcp() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    v--;
    cmp(a(), v);
    memory_write(operand_, v);
}
void NesCpu::op_isb() {
    uint8_t v = get_operand_value();
    memory_write(operand_, v);
    v++;
    add(v ^ 0xFF);
    memory_write(operand_, v);
}
void NesCpu::op_aac() {
    set_a(a() & get_operand_value());
    clear_flags(PSFlags::CARRY);
    if (check_flag(PSFlags::NEGATIVE)) set_flags(PSFlags::CARRY);
}
void NesCpu::op_asr() {
    clear_flags(PSFlags::CARRY);
    set_a(a() & get_operand_value());
    if (a() & 0x01) set_flags(PSFlags::CARRY);
    set_a(a() >> 1);
}
void NesCpu::op_arr() {
    set_a(((a() & get_operand_value()) >> 1) | (check_flag(PSFlags::CARRY) ? 0x80 : 0x00));
    clear_flags(PSFlags::CARRY | PSFlags::OVERFLOW_FLAG);
    if (a() & 0x40) set_flags(PSFlags::CARRY);
    if ((check_flag(PSFlags::CARRY) ? 1 : 0) ^ ((a() >> 5) & 1)) set_flags(PSFlags::OVERFLOW_FLAG);
}
void NesCpu::op_atx() {
    uint8_t v = get_operand_value();
    set_a(v);
    set_x(a());
    set_a(a());
}
void NesCpu::op_axs() {
    uint8_t ov = get_operand_value();
    uint8_t v = (a() & x()) - ov;
    clear_flags(PSFlags::CARRY);
    if ((a() & x()) >= ov) set_flags(PSFlags::CARRY);
    set_x(v);
}
void NesCpu::op_shy() {
    uint16_t base = read_word();
    bool crossed = check_page_crossed(base, x());
    memory_read(base + x() - (crossed ? 0x100 : 0));
    uint16_t addr = base + x();
    uint8_t ah = addr >> 8;
    uint8_t al = addr & 0xFF;
    if (crossed) ah &= y();
    memory_write((ah << 8) | al, y() & ((base >> 8) + 1));
}
void NesCpu::op_shx() {
    uint16_t base = read_word();
    bool crossed = check_page_crossed(base, y());
    memory_read(base + y() - (crossed ? 0x100 : 0));
    uint16_t addr = base + y();
    uint8_t ah = addr >> 8;
    uint8_t al = addr & 0xFF;
    if (crossed) ah &= x();
    memory_write((ah << 8) | al, x() & ((base >> 8) + 1));
}
void NesCpu::op_shaa() {
    uint16_t base = read_word();
    bool crossed = check_page_crossed(base, y());
    memory_read(base + y() - (crossed ? 0x100 : 0));
    uint16_t addr = base + y();
    uint8_t ah = addr >> 8;
    uint8_t al = addr & 0xFF;
    if (crossed) ah &= (x() & a());
    memory_write((ah << 8) | al, (x() & a()) & ((base >> 8) + 1));
}
void NesCpu::op_shaz() {
    uint8_t zero = read_byte();
    uint16_t base_addr;
    if (zero == 0xFF) {
        uint8_t lo = memory_read(0xFF);
        uint8_t hi = memory_read(0x00);
        base_addr = lo | (hi << 8);
    } else {
        base_addr = memory_read_word(zero);
    }
    bool crossed = check_page_crossed(base_addr, y());
    memory_read(base_addr + y() - (crossed ? 0x100 : 0));
    uint16_t addr = base_addr + y();
    uint8_t ah = addr >> 8;
    uint8_t al = addr & 0xFF;
    if (crossed) ah &= (x() & a());
    memory_write((ah << 8) | al, (x() & a()) & ((base_addr >> 8) + 1));
}
void NesCpu::op_tas() {
    op_shaa();
    set_sp(x() & a());
}
void NesCpu::op_ane() {
    uint8_t imm = get_operand_value();
    set_a((a() | 0xEE) & x() & imm);
}
void NesCpu::op_las() {
    uint8_t v = get_operand_value();
    set_a(v & sp());
    set_x(a());
    set_sp(a());
}

} // namespace ear6::nes
