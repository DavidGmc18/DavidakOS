#pragma once

// Signed
typedef __INT8_TYPE__ i8;
typedef __INT16_TYPE__ i16;
typedef __INT32_TYPE__ i32;
typedef __INT64_TYPE__ i64;

// Unsigned
typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

// Size
typedef __PTRDIFF_TYPE__ isize;
typedef __SIZE_TYPE__ usize;
#define USIZE_MAX ((usize)-1)

// Pointers
typedef __INTPTR_TYPE__ iptr;
typedef __UINTPTR_TYPE__ uptr;
#define UPTR_MAX ((uptr)-1)
#define NULL ((void*)0)

// VA
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg