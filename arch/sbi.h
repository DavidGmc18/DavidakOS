#pragma once

#include <types.h>

struct sbi_ret {
    isize error;
    isize value;
};

[[clang::always_inline]]
static inline struct sbi_ret sbi_call(usize arg0, usize arg1, usize arg2, usize arg3, usize arg4, usize arg5, usize fid, usize eid) {
    register usize a0 __asm__("a0") = arg0;
    register usize a1 __asm__("a1") = arg1;
    register usize a2 __asm__("a2") = arg2;
    register usize a3 __asm__("a3") = arg3;
    register usize a4 __asm__("a4") = arg4;
    register usize a5 __asm__("a5") = arg5;
    register usize a6 __asm__("a6") = fid;
    register usize a7 __asm__("a7") = eid;

    __asm__ __volatile__(
        "ecall"
        : "=r"(a0), "=r"(a1)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );

    return (struct sbi_ret){.error = a0, .value = a1};
}