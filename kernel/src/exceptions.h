#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stddef.h>

void except_div0(size_t fault_rip, size_t fault_cs);
void except_debug(size_t fault_rip, size_t fault_cs);
void except_nmi(size_t fault_rip, size_t fault_cs);
void except_breakpoint(size_t fault_rip, size_t fault_cs);
void except_overflow(size_t fault_rip, size_t fault_cs);
void except_bound_range_exceeded(size_t fault_rip, size_t fault_cs);
void except_invalid_opcode(size_t fault_rip, size_t fault_cs);
void except_device_not_available(size_t fault_rip, size_t fault_cs);
void except_double_fault(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_coprocessor_segment_overrun(size_t fault_rip, size_t fault_cs);
void except_invalid_tss(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_segment_not_present(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_stack_segment_fault(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_gen_prot_fault(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_page_fault(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_x87_exception(size_t fault_rip, size_t fault_cs);
void except_alignment_check(size_t error_code, size_t fault_rip, size_t fault_cs);
void except_machine_check(size_t fault_rip, size_t fault_cs);
void except_simd_exception(size_t fault_rip, size_t fault_cs);
void except_virtualisation_exception(size_t fault_rip, size_t fault_cs);
void except_security_exception(size_t error_code, size_t fault_rip, size_t fault_cs);

#endif
