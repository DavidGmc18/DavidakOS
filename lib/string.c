#include "string.h"

void* memset(void* buf, char c, usize n) {
    char* p = buf;
    while (n--) *p++ = c;
    return buf;
}