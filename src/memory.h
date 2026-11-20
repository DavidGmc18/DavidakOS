#pragma once

#include <types.h>

enum page_status : u8 {
    PAGE_TAIL = 0x0,
    PAGE_FREE = 0x1,
    PAGE_USED = 0x2,
    PAGE_RSVD = 0x3,
    PAGE_DIRTY = 0xFF
};

struct page {
    enum page_status status;
    u8 order;
    struct page* prev;
    struct page* next;
};

void mem_init();

struct page* alloc_pages(u8 order);
void free_pages(struct page* page);

const struct page* phys_to_page(usize phys);
usize page_to_phys(const struct page* page);

void mem_debug();