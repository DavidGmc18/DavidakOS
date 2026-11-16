#include <types.h>
#include <string.h>
#include <stdio.h>
#include "panic.h"

extern char __bss[], __bss_end[], __stack_top[];

void main([[maybe_unused]] usize hartid, [[maybe_unused]] usize dtb_ptr) {
    memset(__bss, 0, __bss_end - __bss);

    printf("Hello Kernel!\n");
    PANIC("No kernel :(\n");

    for (;;) __asm__ __volatile__("wfi");
}

[[gnu::section(".text.entry"), gnu::naked]]
void entry(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "j main\n"
        ::
        [stack_top] "r" (__stack_top)
    );
}