#include "string.h"

void* memset(void* buf, char c, usize n) {
    char* p = buf;
    while (n--) *p++ = c;
    return buf;
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