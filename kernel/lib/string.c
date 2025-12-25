/**
 * =============================================================================
 * Chanux OS - String Functions Implementation
 * =============================================================================
 * Standard C string and memory manipulation functions for kernel use.
 * =============================================================================
 */

#include "../include/string.h"

/* =============================================================================
 * Memory Functions
 * =============================================================================
 */

void* memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    uint8_t byte = (uint8_t)val;

    /* Optimize for large fills using 64-bit writes */
    if (count >= 8) {
        uint64_t pattern = byte;
        pattern |= pattern << 8;
        pattern |= pattern << 16;
        pattern |= pattern << 32;

        /* Align to 8-byte boundary */
        while (((uintptr_t)ptr & 7) && count) {
            *ptr++ = byte;
            count--;
        }

        /* Fill 8 bytes at a time */
        uint64_t* ptr64 = (uint64_t*)ptr;
        while (count >= 8) {
            *ptr64++ = pattern;
            count -= 8;
        }
        ptr = (uint8_t*)ptr64;
    }

    /* Fill remaining bytes */
    while (count--) {
        *ptr++ = byte;
    }

    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    /* Optimize for large copies using 64-bit reads/writes */
    if (count >= 8 && ((uintptr_t)d & 7) == ((uintptr_t)s & 7)) {
        /* Align to 8-byte boundary */
        while (((uintptr_t)d & 7) && count) {
            *d++ = *s++;
            count--;
        }

        /* Copy 8 bytes at a time */
        uint64_t* d64 = (uint64_t*)d;
        const uint64_t* s64 = (const uint64_t*)s;
        while (count >= 8) {
            *d64++ = *s64++;
            count -= 8;
        }
        d = (uint8_t*)d64;
        s = (const uint8_t*)s64;
    }

    /* Copy remaining bytes */
    while (count--) {
        *d++ = *s++;
    }

    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s || count == 0) {
        return dest;
    }

    /* If dest is before src or no overlap, copy forward */
    if (d < s || d >= s + count) {
        return memcpy(dest, src, count);
    }

    /* Copy backward to handle overlap */
    d += count;
    s += count;
    while (count--) {
        *--d = *--s;
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t count) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    while (count--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

/* =============================================================================
 * String Functions
 * =============================================================================
 */

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* ret = dest;

    while (n && (*dest++ = *src++)) {
        n--;
    }

    /* Pad with zeros if needed */
    while (n--) {
        *dest++ = '\0';
    }

    return ret;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(*(unsigned char*)s1) - (int)(*(unsigned char*)s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (int)(*(unsigned char*)s1) - (int)(*(unsigned char*)s2);
}
