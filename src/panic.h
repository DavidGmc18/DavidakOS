#pragma once

#define PANIC(fmt, ...) \
do { \
    printf("PANIC: %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
    __asm__ volatile("csrc sstatus, 2"); \
    while (1) __asm__ volatile("wfi"); \
    __builtin_unreachable(); \
} while (0)