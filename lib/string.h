#pragma once

#include <types.h>

void* memset(void* buf, char c, usize n);
void* memmove(void* dst, const void* src, usize n);

usize strlen(const char* str);
int strcmp(const char* str1, const char* str2);