#include <types.h>
#include <string.h>
#include <stdio.h>
#include "dtb.h"

extern char __bss[], __bss_end[], __stack_top[];

void main([[maybe_unused]] usize hartid, [[maybe_unused]] usize dtb_ptr) {
    memset(__bss, 0, __bss_end - __bss);

    dtb_init(dtb_ptr);
    dtb_print_fdt();

    u32 i = dtb_enter_node(DTB_ROOT);
    while (true) {
        i = dtb_find_node_with_prop(i, "device_type");
        if (i == DTB_FAIL) break;
        struct dtb_prop type = dtb_get_prop(i, "device_type");
        if (type.addr && strcmp(type.addr, "memory") == 0) {
            printf("%s\n", dtb_get_node_name(i));
        }
        i = dtb_find_node_end(i);
    }

    i = dtb_enter_node(DTB_ROOT);
    i = dtb_find_node_with_name(i, "reserved-memory");
    i = dtb_enter_node(i);
    while (true) {
        i = dtb_find_node_with_prop(i, "reg");
        if (i == DTB_FAIL) break;
        printf("%s\n", dtb_get_node_name(i));
        i = dtb_find_node_end(i);
    }

    while (true) __asm__ __volatile__("wfi");
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