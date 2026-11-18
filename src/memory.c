#include "memory.h"
#include <types.h>
#include "common.h"
#include "dtb.h"
#include <stdio.h>
#include <string.h>
#include "panic.h"

#define PAGE_SIZE 4096

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

void map_memory() {
    parse_memory_devices();

    for (u32 i = 0; i < node_count; i++) {
        printf("@%lx:%lx\n", nodes[i].addr, nodes[i].addr + nodes[i].size -1);
    }
}