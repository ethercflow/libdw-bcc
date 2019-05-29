#ifndef __PTRACE_H_
#define __PTRACE_H_

#include "types.h"
#include <assert.h>
#include <stddef.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

struct pt_regs {
/*
 * C ABI says these regs are callee-preserved. They aren't saved on kernel entry
 * unless syscall needs a complete, fully filled "struct pt_regs".
 */
        unsigned long r15;
        unsigned long r14;
        unsigned long r13;
        unsigned long r12;
        unsigned long bp;
        unsigned long bx;
/* These regs are callee-clobbered. Always saved on kernel entry. */
        unsigned long r11;
        unsigned long r10;
        unsigned long r9;
        unsigned long r8;
        unsigned long ax;
        unsigned long cx;
        unsigned long dx;
        unsigned long si;
        unsigned long di;
/*
 * On syscall entry, this is syscall#. On CPU exception, this is error code.
 * On hw interrupt, it's IRQ number:
 */
        unsigned long orig_ax;
/* Return frame for iretq */
        unsigned long ip;
        unsigned long cs;
        unsigned long flags;
        unsigned long sp;
        unsigned long ss;
/* top of stack page */
};

#define X86_MAX X86_64_MAX

#define PT_REGS_OFFSET(id, r) [id] = offsetof(struct pt_regs, r)

enum x86_regs {
        X86_AX,
        X86_BX,
        X86_CX,
        X86_DX,
        X86_SI,
        X86_DI,
        X86_BP,
        X86_SP,
        X86_IP,
        X86_FLAGS,
        X86_CS,
        X86_SS,
        X86_DS,
        X86_ES,
        X86_FS,
        X86_GS,
        X86_R8,
        X86_R9,
        X86_R10,
        X86_R11,
        X86_R12,
        X86_R13,
        X86_R14,
        X86_R15,

        X86_64_MAX = X86_R15 + 1,
};

static unsigned int pt_regs_offset[X86_MAX] = {
        PT_REGS_OFFSET(X86_AX, ax),
        PT_REGS_OFFSET(X86_BX, bx),
        PT_REGS_OFFSET(X86_CX, cx),
        PT_REGS_OFFSET(X86_DX, dx),
        PT_REGS_OFFSET(X86_SI, si),
        PT_REGS_OFFSET(X86_DI, di),
        PT_REGS_OFFSET(X86_BP, bp),
        PT_REGS_OFFSET(X86_SP, sp),
        PT_REGS_OFFSET(X86_IP, ip),
        PT_REGS_OFFSET(X86_FLAGS, flags),
        PT_REGS_OFFSET(X86_CS, cs),
        PT_REGS_OFFSET(X86_SS, ss),
        /*
         * The pt_regs struct does not store
         * ds, es, fs, gs in 64 bit mode.
         */
        (unsigned int) -1,
        (unsigned int) -1,
        (unsigned int) -1,
        (unsigned int) -1,
        PT_REGS_OFFSET(X86_R8, r8),
        PT_REGS_OFFSET(X86_R9, r9),
        PT_REGS_OFFSET(X86_R10, r10),
        PT_REGS_OFFSET(X86_R11, r11),
        PT_REGS_OFFSET(X86_R12, r12),
        PT_REGS_OFFSET(X86_R13, r13),
        PT_REGS_OFFSET(X86_R14, r14),
        PT_REGS_OFFSET(X86_R15, r15),
};

static inline u64 reg_value(struct pt_regs *regs, int idx)
{
        unsigned int offset;

        assert(idx < ARRAY_SIZE(pt_regs_offset));
        offset = pt_regs_offset[idx];
        return *(u64*)((u64)regs + offset);
}

#endif // __PTRACE_H_
