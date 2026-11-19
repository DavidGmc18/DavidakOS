#include "memory.h"
#include <types.h>
#include "common.h"
#include "dtb.h"
#include <stdio.h>
#include <string.h>
#include "panic.h"

extern char __kernel[], __kernel_end[];

#define PAGE_SIZE 4096
#define PAGE_MASK ((uptr)PAGE_SIZE-1)
#define MAX_ORDER 11

enum page_status : u8 {
    PAGE_TAIL = 0x0,
    PAGE_FREE = 0x1,
    PAGE_USED = 0x2,
    PAGE_RSVD = 0xFF,
};

struct page {
    enum page_status status;
    u8 order;
    struct page* next;
};

struct memory_node {
    uptr addr;
    usize size;
    struct page* map;
};

#define MAX_NODES 8
static u32 node_count = 0;
static struct memory_node nodes[MAX_NODES];

static usize free_count[MAX_ORDER] = {};
static struct page* free_list[MAX_ORDER] = {};

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

static bool next_free_gap(const struct memory_node* node, const struct reserved_regions* rsv, u32* idx, uptr* start, uptr* last) {
    uptr node_last = node->addr + node->size - 1;

    while (*idx <= rsv->count) {
        u32 i = (*idx)++;
        *start = node->addr;
        *last = node_last;

        if (i > 0) {
            uptr prev_last;
            if (add_overflow(rsv->regions[i-1].addr, rsv->regions[i-1].size - 1, &prev_last)) break;
            if (prev_last >= *last) break;
            if (prev_last >= *start) *start = prev_last + 1;
        }

        if (i < rsv->count) {
            if (rsv->regions[i].addr <= *start) continue;
            if (rsv->regions[i].addr <= *last) *last = rsv->regions[i].addr - 1;
        }

        return true;
    }

    return false;
}

static void* boot_alloc(usize bytes, u32 align, struct memory_node* node, struct reserved_regions* rsv) {
    if (!bytes || !align || (align & (align-1))) return NULL;
    uptr mask = align - 1;

    u32 idx = 0;
    uptr start, last;
    while (next_free_gap(node, rsv, &idx, &start, &last)) {
        if ((start & mask) && add_overflow(start & ~mask, align, &start)) continue;
        if (start > last) continue;
        if (last - start < bytes - 1) continue;

        add_reserved_region(rsv, start, bytes, "boot_alloc");
        return (void*)start;
    }

    return NULL;
}

static u8 get_order_from_pfn(usize pfn) {
    if (pfn == 0) return MAX_ORDER-1;
    int order = __builtin_ctzll(pfn);
    return (order < MAX_ORDER) ? order : MAX_ORDER-1;
}

static u8 get_order_from_count(usize count) {
    if (count == 0) return 0;
    int order = 63 - __builtin_clzll(count);
    return (order < MAX_ORDER) ? order : MAX_ORDER-1;
}

static void add_free_buddy(struct page* page) {
    if (page->status != PAGE_FREE) PANIC("[MEM] Failed to add buddy to free list: status is not PAGE_FREE\n");
    if (page->order >= MAX_ORDER) PANIC("[MEM] Failed to add buddy to free list: order %u exceeds max allowed (%u)\n", page->order, MAX_ORDER-1);
    if (page->next) PANIC("[MEM] Failed to add buddy to free list: page is already linked\n");
    page->next = free_list[page->order];
    free_list[page->order] = page;
    free_count[page->order]++;
}

static void map_node(struct memory_node* node, struct reserved_regions* rsv) {
    usize page_count = node->size / PAGE_SIZE;
    usize map_size = page_count * sizeof(struct page);

    node->map = boot_alloc(map_size, alignof(struct page), node, rsv);
    if (!node->map) PANIC("[MEM] Failed to allocate page map for memory node\n");
    
    for (usize i = 0; i < page_count; i++) {
        node->map[i] = (struct page){
            .status = PAGE_RSVD,
            .order = 0,
        };
    }

    usize node_pfn = node->addr / PAGE_SIZE;

    u32 idx = 0;
    uptr start, last;
    while (next_free_gap(node, rsv, &idx, &start, &last)) {
        if ((start & PAGE_MASK) && add_overflow(start & ~PAGE_MASK, PAGE_SIZE, &start)) continue;
        if (((last + 1) & PAGE_MASK) && sub_overflow(last & ~PAGE_MASK, 1, &last)) continue;
        if (start > last) continue;

        usize pfn = start / PAGE_SIZE;
        usize last_pfn = last / PAGE_SIZE;
        while (pfn <= last_pfn) {
            u8 order = MIN(
                get_order_from_pfn(pfn),
                get_order_from_count(last_pfn - pfn + 1)
            );
            usize count = (usize)1 << order;

            usize loc_pfn = pfn - node_pfn;
            node->map[loc_pfn] = (struct page){
                .status = PAGE_FREE,
                .order = order,
            };

            for (usize j = 1; j < count; j++) {
                node->map[loc_pfn+j] = (struct page){
                    .status = PAGE_TAIL,
                    .order = 0,
                };
            }

            add_free_buddy(&node->map[loc_pfn]);

            pfn += count;
        }
    }
}

void map_memory() {
    parse_memory_devices();

    struct reserved_regions rsv = {.count = 0};
    parse_reserved_regions(&rsv);

    for (u32 i = 0; i < node_count; i++) {
        map_node(&nodes[i], &rsv);
    }

    printf("[MEM] Buddy allocator free blocks:\n");
    usize bytes = 0;
    for (u32 i = 0; i < MAX_ORDER; i++) {
        bytes += free_count[i] * ((usize)4096 << i);
        printf("  [Order %u]: %lu blocks\n", i, free_count[i]);
    }
    printf("[MEM] Total free memory: %lu KiB\n", bytes / 1024);
}