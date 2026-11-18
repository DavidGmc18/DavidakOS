#pragma once

#include <types.h>

void dtb_init(uptr dtb_ptr);

#define DTB_ROOT ((u32)(0))
#define DTB_FAIL ((u32)(-1))

u32 dtb_get_nr_address_cells();
u32 dtb_get_nr_size_cells();

u32 dtb_find_node_with_name(u32 idx, const char* trgt);
u32 dtb_find_node_with_prop(u32 start, const char* prop);
u32 dtb_find_node_end(u32 start);

u32 dtb_enter_node(u32 node);
const char* dtb_get_node_name(u32 node);

struct dtb_prop {
    const u32* addr;
    u32 len;
};
struct dtb_prop dtb_get_prop(u32 start, const char* name);
u64 dtb_read_cells(const u32* p, u32 n);

void dtb_print_fdt();