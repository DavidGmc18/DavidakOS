#include <stdio.h>
#include <types.h>
#include <string.h>
#include <common.h>
#include "dtb.h"
#include "memory.h"

extern char __bss[], __bss_end[], __stack_top[];

void main([[maybe_unused]] usize hartid, [[maybe_unused]] usize dtb_ptr) {
    memset(__bss, 0, __bss_end - __bss);

    dtb_init(dtb_ptr);
    dtb_print_fdt();

    mem_init();

    struct page* p1 = alloc_pages(0);
    if (p1) printf("Allocation 1: 0x%lx order=%u\n", page_to_phys(p1), p1->order);

    struct page* p2 = alloc_pages(0);
    if (p2) printf("Allocation 2: 0x%lx order=%u\n", page_to_phys(p2), p2->order);

    struct page* p3 = alloc_pages(10);
    if (p3) printf("Allocation 3: 0x%lx order=%u\n", page_to_phys(p3), p3->order);

    free_pages(p1);
    printf("Freed 1\n");

    struct page* p4 = alloc_pages(0);
    if (p4) printf("Allocation 4: 0x%lx order=%u\n", page_to_phys(p4), p4->order);

    mem_debug();

    free_pages(p2);
    printf("Freed 2\n");

    free_pages(p3);
    printf("Freed 3\n");

    free_pages(p4);
    printf("Freed 4\n");

    mem_debug();

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