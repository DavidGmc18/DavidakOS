#include "string.h"

void* memset(void* buf, char c, usize n) {
    char* p = buf;
    while (n--) *p++ = c;
    return buf;
}

void* memmove(void* dst, const void* src, usize n) {
    char* d = dst;
    const char* s = src;

    if (d == s || n == 0) return dst;

    if (d < s) {
        for (usize i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (usize i = n; i-- > 0;) d[i] = s[i];
    }

    return dst;
}

usize strlen(const char* str) {
    if (!str) return 0;
    const char* ptr = str;
    while (*ptr) ptr++;
    return ptr - str;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && *str2) {
        if (*str1 != *str2) break;
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}