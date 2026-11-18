#pragma once

#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)

#define add_overflow(a, b, res) __builtin_add_overflow(a, b, res)
#define sub_overflow(a, b, res) __builtin_sub_overflow(a, b, res)