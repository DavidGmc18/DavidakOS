#include "stdio.h"
#include <arch/sbi.h>

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1);
}

enum length {
    LEN_DEFAULT,
    LEN_LONG,
};

void printf(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            enum length len = LEN_DEFAULT; 
        format:
            fmt++;
            switch (*fmt) {
                case '\0':
                    putchar('%');
                    goto end;

                case '%':
                    putchar('%');
                    break;

                case 'l': {
                    len = LEN_LONG;
                    goto format;
                }

                case 's': {
                    const char* str = va_arg(vargs, const char*);
                    while (*str) putchar(*str++);
                    break;
                }

                case 'd': {
                    i64 value;
                    switch (len) {
                        case LEN_LONG:
                            value = va_arg(vargs, i64);
                            break;

                        case LEN_DEFAULT:
                        default:
                            value = va_arg(vargs, int);
                            break;
                    }

                    u64 magnitude = value;
                    if (value < 0) {
                        putchar('-');
                        magnitude = -magnitude;
                    }

                    u64 divisor = 1;
                    while (magnitude / divisor >= 10)
                        divisor *= 10;

                    while (divisor > 0) {
                        putchar('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }

                    break;
                }

                case 'u': {
                    u64 value;
                    switch (len) {
                        case LEN_LONG:
                            value = va_arg(vargs, u64);
                            break;

                        case LEN_DEFAULT:
                        default:
                            value = va_arg(vargs, unsigned);
                            break;
                    }

                    u64 magnitude = value;

                    u64 divisor = 1;
                    while (magnitude / divisor >= 10)
                        divisor *= 10;

                    while (divisor > 0) {
                        putchar('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }

                    break;
                }

                case 'x': {
                    u64 value;
                    unsigned digits;
                    switch (len) {
                        case LEN_LONG:
                            value = va_arg(vargs, u64);
                            digits = 2 * sizeof(u64); 
                            break;

                        case LEN_DEFAULT:
                        default:
                            value = va_arg(vargs, unsigned);
                            digits = 2 * sizeof(unsigned); 
                            break;
                    }

                    bool flag = false;
                    for (int i = digits-1; i >= 0; i--) {
                        unsigned nibble = (value >> (i * 4)) & 0xF;
                        if (!nibble && !flag && i) continue;
                        putchar("0123456789abcdef"[nibble]);
                        flag = true;
                    }
                }
            }
        } else {
            putchar(*fmt);
        }

        fmt++;
    }

end:
    va_end(vargs);
}