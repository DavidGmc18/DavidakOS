#include "memory.h"
#include <types.h>
#include "common.h"
#include "dtb.h"
#include <stdio.h>
#include <string.h>
#include "panic.h"

#define PAGE_SIZE 4096

extern char __kernel[], __kernel_end[];

struct memory_node {
    uptr addr;
    usize size;
    struct page* map;
};

#define MAX_NODES 8
static u32 node_count = 0;
static struct memory_node nodes[MAX_NODES];

static void add_memory_device(u32 device) {
    const char* name = dtb_get_node_name(device);

    if (node_count >= MAX_NODES) {
        printf("[MEM] Discarding device %s: node limit reached\n", name);
        return;
    }

    struct dtb_prop reg = dtb_get_prop(device, "reg");
    if (!reg.addr) {
        printf("[MEM] Discarding device %s: parsing failed\n", name);
        return;
    }

    const u32 nr_address_cells = dtb_get_nr_address_cells();
    const u32 nr_size_cells = dtb_get_nr_size_cells();
    if (reg.len != 4 * (nr_address_cells + nr_size_cells)) {
        printf("[MEM] Discarding device %s: parsing failed\n", name);
        return;
    }

    u64 addr = dtb_read_cells(reg.addr, nr_address_cells);
    u64 size = dtb_read_cells(reg.addr + nr_address_cells, nr_size_cells);

    if (!size) {
        printf("[MEM] Discarding device %s: size is 0 bytes\n", name);
        return;
    }

    u64 last;
    if (add_overflow(addr, size-1, &last)) {
        printf("[MEM] Discarding device %s: address range overflow\n", name);
        return;
    }
    
    // Align addr
    if (addr % PAGE_SIZE) {
        printf("[MEM] Aligning device %s: adjusting start address to page boundary\n", name);
        if (add_overflow((addr / PAGE_SIZE) * PAGE_SIZE, PAGE_SIZE, &addr)) {
            printf("[MEM] Discarding device %s: address range overflow\n", name);
            return;
        }
    }

    // Align last
    usize rem = (last + 1) % PAGE_SIZE;
    if (rem) {
        if (rem > last) {
            printf("[MEM] Discarding device %s: size is 0 bytes after aligning\n", name);
            return;
        }
        printf("[MEM] Aligning device %s: adjusting end address to page boundary\n", name);
        last -= rem;
    }

    // Calculate new size
    if (last < addr) {
        printf("[MEM] Discarding device %s: size is 0 bytes after aligning\n", name);
        return;
    }
    size = last + 1 - addr;

    if (addr > UPTR_MAX || last > UPTR_MAX || size > USIZE_MAX) {
        printf("[MEM] Discarding device %s: address range overflow\n", name);
        return;
    }

    u32 i = 0;
    while (i < node_count && nodes[i].addr < addr) i++;
    
    if (((i < node_count) && (last >= nodes[i].addr))
    || ((i > 0) && (nodes[i-1].addr + nodes[i-1].size - 1 >= addr))) {
        printf("[MEM] Discarding device %s: collision with another device\n", name);
        return;
    }

    memmove(&nodes[i+1], &nodes[i], (node_count - i) * sizeof(struct memory_node));

    nodes[i].addr = addr;
    nodes[i].size = size;

    node_count++;
}

static void parse_memory_devices() {
    u32 next = dtb_enter_node(DTB_ROOT);
    while (true) {
        u32 device = dtb_find_node_with_prop(next, "device_type");
        if (device == DTB_FAIL) break;

        struct dtb_prop type = dtb_get_prop(device, "device_type");
        if (type.addr && strcmp((const char*)type.addr, "memory") == 0) {
            add_memory_device(device);
        }

        next = dtb_find_node_end(device);
    }

    if (!node_count) PANIC("[MEM] Found no memory\n");

    usize size = nodes[0].size;
    for (u32 i = 1; i < node_count; i++) size += nodes[i].size;

    printf("[MEM] Registered %d nodes with total memory of %lu KiB\n", node_count, size / 1024);
}

struct region {
    uptr addr;
    usize size;
};

#define MAX_RESERVED_REGIONS 64
struct reserved_regions {
    struct region regions[MAX_RESERVED_REGIONS];
    u32 count;
};

static void add_reserved_region(struct reserved_regions* rsv, u64 addr, u64 size, const char* name) {
    if (!size) return;

    uptr last;
    if (__builtin_add_overflow(addr, size-1, &last) || size > USIZE_MAX)
        PANIC("[MEM] Failed to register reserved region %s: address range overflow\n", name);

    u32 i = 0;
    while (i < rsv->count && rsv->regions[i].addr <= addr) i++;

    u32 j = 0;
    if (i > 0 && addr - rsv->regions[i-1].addr <= rsv->regions[i-1].size) {
        j = i - 1;
        uptr cur_last = rsv->regions[j].addr + rsv->regions[j].size - 1;
        if (last > cur_last) rsv->regions[j].size = last + 1 - rsv->regions[j].addr;

    } else if (i < rsv->count && rsv->regions[i].addr - addr <= size) {
        j = i;
        uptr cur_last = rsv->regions[j].addr + rsv->regions[j].size - 1;
        uptr new_last = (last > cur_last) ? last : cur_last;
        rsv->regions[j].addr = addr;
        rsv->regions[j].size = new_last + 1 - addr;

    } else {
        if (rsv->count >= MAX_RESERVED_REGIONS) PANIC("[MEM] Failed to register reserved region %s: too many reserved regions (max: %d)\n", name, MAX_RESERVED_REGIONS);
        j = i;
        memmove(&rsv->regions[i+1], &rsv->regions[i], (rsv->count - i) * sizeof(*rsv->regions));
        rsv->regions[j].addr = addr;
        rsv->regions[j].size = size;
        rsv->count++;
        return;
    }

    uptr j_last = rsv->regions[j].addr + rsv->regions[j].size - 1;

    u32 k = j + 1;
    while (k < rsv->count && rsv->regions[k].addr - 1 <= j_last) k++;

    if (k > (j + 1)) {
        uptr k_last = rsv->regions[k-1].addr + rsv->regions[k-1].size - 1;
        if (k_last > j_last) rsv->regions[j].size = k_last + 1 - rsv->regions[j].addr;

        memmove(&rsv->regions[j+1], &rsv->regions[k], (rsv->count - k) * sizeof(*rsv->regions));
        rsv->count -= k - (j + 1);
    }
}

static void add_fdt_reserved_regions(struct reserved_regions* rsv) {
    u32 i = dtb_enter_node(DTB_ROOT);
    i = dtb_find_node_with_name(i, "reserved-memory");
    i = dtb_enter_node(i);
    if (i == DTB_FAIL) PANIC("[MEM] Failed to find/parse reserved-memory node\n");

    while (true) {
        i = dtb_find_node_with_prop(i, "reg");
        if (i == DTB_FAIL) break;

        const char* name = dtb_get_node_name(i);

        struct dtb_prop reg = dtb_get_prop(i, "reg");
        if (!reg.addr) PANIC("[MEM] Failed to register reserved region %s: parsing failed\n", name);

        const u32 nr_address_cells = dtb_get_nr_address_cells();
        const u32 nr_size_cells = dtb_get_nr_size_cells();
        u32 expected_len = 4 * (nr_address_cells + nr_size_cells);
        if (reg.len != expected_len)
            PANIC("[MEM] Failed to register reserved region %s: unexpected len of %u (expected: %u)\n", name, reg.len, expected_len);

        u64 addr = dtb_read_cells(reg.addr, nr_address_cells);
        u64 size = dtb_read_cells(reg.addr + nr_address_cells, nr_size_cells);

        add_reserved_region(rsv, addr, size, name);

        i = dtb_find_node_end(i);
        if (i == DTB_FAIL) break;
    }
}

static void add_mem_rsvmap_regions(struct reserved_regions* rsv) {
    const u32* p = dtb_get_mem_rsvmap();
    if (!p) PANIC("[MEM] Failed to get mem_rsvmap\n");
    while (true) {
        u64 addr = dtb_read_cells(p, 2);
        u64 size = dtb_read_cells(p+2, 2);
        if (!addr && !size) break;
        add_reserved_region(rsv, addr, size, "mem_rsv");
        p += 4;
    }
}

static void parse_reserved_regions(struct reserved_regions* rsv) {
    add_fdt_reserved_regions(rsv);
    add_mem_rsvmap_regions(rsv);
    add_reserved_region(rsv, (u64)__kernel, (u64)__kernel_end - (u64)__kernel, "kernel");
    add_reserved_region(rsv, dtb_get_dtb_addr(), dtb_get_dtb_size(), "dtb");

    printf("[MEM] Registered %u effective reserved regions\n", rsv->count);
}

void map_memory() {
    parse_memory_devices();
    for (u32 i = 0; i < node_count; i++) {
        printf("@%lx:%lx\n", nodes[i].addr, nodes[i].addr + nodes[i].size -1);
    }

    struct reserved_regions rsv = {.count = 0};
    parse_reserved_regions(&rsv);
    for (u32 i = 0; i < rsv.count; i++) {
        printf("@%lx:%lx\n", rsv.regions[i].addr, rsv.regions[i].addr + rsv.regions[i].size -1);
    }
}