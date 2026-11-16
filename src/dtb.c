#include "dtb.h"
#include <stdio.h>
#include <types.h>
#include <string.h>
#include <common.h>

struct fdt_header {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
};

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

struct fdt_prop {
    u32 len;
    u32 nameoff;
};

static const struct fdt_header* header = NULL;
static const u32* dt_struct = NULL;
static const char* dt_strings = NULL;

void dtb_init(uptr dtb_ptr) {
    header = (const struct fdt_header*)dtb_ptr;

    usize off_dt_struct = bswap32(header->off_dt_struct);
    usize off_dt_strings = bswap32(header->off_dt_strings);

    dt_struct = (const u32*)((uptr)header + off_dt_struct);
    dt_strings = (const char*)((uptr)header + off_dt_strings);
}

u32 dtb_find_node_with_name(u32 idx, const char* trgt) {
    if (idx == DTB_FAIL) return DTB_FAIL;

    i32 depth = 0;
    while (true) {
        u32 token = bswap32(dt_struct[idx++]);
        switch (token) {
            case FDT_BEGIN_NODE:
                const char* name = (const char*)&dt_struct[idx];
                if (depth == 0 && strcmp(name, trgt) == 0) return idx-1;
                idx += (strlen(name) + 1 + 3) / 4;
                depth++;
                break;

            case FDT_END_NODE:
                depth--;
                if (depth < 0) return DTB_FAIL;
                break;

            case FDT_PROP:
                u32 len = bswap32(dt_struct[idx++]);
                idx++; // nameoff
                idx += (len + 3) / 4;
                break;

            case FDT_NOP:
                break;

            default:
                return DTB_FAIL;
        }
    }
}

u32 dtb_find_node_with_prop(u32 idx, const char* trgt) {
    if (idx == DTB_FAIL) return DTB_FAIL;

    u32 node = DTB_FAIL;
    i32 depth = 0;
    while (true) {
        u32 token = bswap32(dt_struct[idx++]);
        switch (token) {
            case FDT_BEGIN_NODE:
                if (depth == 0) node = idx-1;
                const char* name = (const char*)&dt_struct[idx];
                idx += (strlen(name) + 1 + 3) / 4;
                depth++;
                break;

            case FDT_END_NODE:
                depth--;
                if (depth < 0) return DTB_FAIL;
                break;

            case FDT_PROP:
                u32 len = bswap32(dt_struct[idx++]);
                u32 nameoff = bswap32(dt_struct[idx++]);
                if (depth == 1 && strcmp(&dt_strings[nameoff], trgt) == 0) return node;
                idx += (len + 3) / 4;
                break;

            case FDT_NOP:
                break;

            default:
                return DTB_FAIL;
        }
    }
}

u32 dtb_find_node_end(u32 idx) {
    if (idx == DTB_FAIL) return DTB_FAIL;

    i32 depth = 0;
    while (true) {
        u32 token = bswap32(dt_struct[idx++]);
        switch (token) {
            case FDT_BEGIN_NODE:
                const char* name = (const char*)&dt_struct[idx];
                idx += (strlen(name) + 1 + 3) / 4;
                depth++;
                break;

            case FDT_END_NODE:
                depth--;
                if (depth == 0) return idx;
                break;

            case FDT_PROP:
                u32 len = bswap32(dt_struct[idx++]);
                idx++; // nameoff
                idx += (len + 3) / 4;
                break;

            case FDT_NOP:
                break;

            default:
                return DTB_FAIL;
        }
    }
}

u32 dtb_enter_node(u32 idx) {
    if (idx == DTB_FAIL) return DTB_FAIL;
    u32 token = bswap32(dt_struct[idx++]);
    if (token != FDT_BEGIN_NODE) return DTB_FAIL;
    const char* name = (const char*)&dt_struct[idx];
    idx += (strlen(name) + 1 + 3) / 4;
    return idx;
}

const char* dtb_get_node_name(u32 idx) {
    if (idx == DTB_FAIL) return NULL;
    u32 token = bswap32(dt_struct[idx++]);
    if (token != FDT_BEGIN_NODE) return NULL;
    return (const char*)&dt_struct[idx];
}

struct dtb_prop dtb_get_prop(u32 idx, const char* trgt) {
    if (idx == DTB_FAIL) return (struct dtb_prop){.addr=NULL, .len=0};

    i32 depth = 0;
    while (true) {
        u32 token = bswap32(dt_struct[idx++]);
        switch (token) {
            case FDT_BEGIN_NODE:
                const char* name = (const char*)&dt_struct[idx];
                idx += (strlen(name) + 1 + 3) / 4;
                depth++;
                break;

            case FDT_END_NODE:
                depth--;
                if (depth < 1) return (struct dtb_prop){.addr=NULL, .len=0};
                break;

            case FDT_PROP:
                u32 len = bswap32(dt_struct[idx++]);
                u32 nameoff = bswap32(dt_struct[idx++]);
                if (depth == 1 && strcmp(&dt_strings[nameoff], trgt) == 0) return (struct dtb_prop){.addr=&dt_struct[idx], .len=len};
                idx += (len + 3) / 4;
                break;

            case FDT_NOP:
                break;

            default:
                return (struct dtb_prop){.addr=NULL, .len=0};
        }
    }
}

#define INDENT(n) do{for(int i=0;i<n;i++)putchar(' ');}while(0)

void dtb_print_fdt() {
    u32 idx = 0;

    i32 depth = 0;
    while (true) {
        u32 token = bswap32(dt_struct[idx++]);
        switch (token) {
            case FDT_BEGIN_NODE:
                const char* name = (const char*)&dt_struct[idx];
                INDENT(2*depth);
                if (*name) printf("%s {\n", name); else printf("{\n");
                idx += (strlen(name) + 1 + 3) / 4;
                depth++;
                break;

            case FDT_END_NODE:
                depth--;
                if (depth < 0) return;
                INDENT(2*depth);
                printf("}\n");
                break;

            case FDT_PROP:
                u32 len = bswap32(dt_struct[idx++]);
                u32 nameoff = bswap32(dt_struct[idx++]);
                INDENT(2*depth);
                if (len) printf("%s: <%u bytes>\n", &dt_strings[nameoff], len); else printf("%s\n", &dt_strings[nameoff]);
                idx += (len + 3) / 4;
                break;

            case FDT_NOP:
                break;

            default:
                return;
        }
    }
}

// u32 dtb_walk(u32 idx) {
//     if (idx == DTB_FAIL) return DTB_FAIL;

//     i32 depth = 0;
//     while (true) {
//         u32 token = bswap32(dt_struct[idx++]);
//         switch (token) {
//             case FDT_BEGIN_NODE:
//                 const char* name = (const char*)&dt_struct[idx];
//                 idx += (strlen(name) + 1 + 3) / 4;
//                 depth++;
//                 break;

//             case FDT_END_NODE:
//                 depth--;
//                 if (depth < 0) return DTB_FAIL;
//                 break;

//             case FDT_PROP:
//                 u32 len = bswap32(dt_struct[idx++]);
//                 u32 nameoff = bswap32(dt_struct[idx++]);
//                 idx += (len + 3) / 4;
//                 break;

//             case FDT_NOP:
//                 break;

//             default:
//                 return DTB_FAIL;
//         }
//     }
// }