/**
 * =============================================================================
 * Chanux OS - String Functions Header
 * =============================================================================
 * Standard C string and memory manipulation functions for kernel use.
 * =============================================================================
 */

#ifndef CHANUX_STRING_H
#define CHANUX_STRING_H

#include "types.h"

/* =============================================================================
 * Memory Functions
 * =============================================================================
 */

/**
 * Fill memory with a constant byte
 * @param dest  Pointer to the block of memory to fill
 * @param val   Value to be set (converted to unsigned char)
 * @param count Number of bytes to fill
 * @return      Pointer to dest
 */
void* memset(void* dest, int val, size_t count);

/**
 * Copy memory area (non-overlapping)
 * @param dest  Pointer to destination
 * @param src   Pointer to source
 * @param count Number of bytes to copy
 * @return      Pointer to dest
 */
void* memcpy(void* dest, const void* src, size_t count);

/**
 * Copy memory area (handles overlapping regions)
 * @param dest  Pointer to destination
 * @param src   Pointer to source
 * @param count Number of bytes to copy
 * @return      Pointer to dest
 */
void* memmove(void* dest, const void* src, size_t count);

/**
 * Compare two memory areas
 * @param s1    First memory area
 * @param s2    Second memory area
 * @param count Number of bytes to compare
 * @return      0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t count);

/* =============================================================================
 * String Functions
 * =============================================================================
 */

/**
 * Get string length
 * @param str   Null-terminated string
 * @return      Length of string (not including null terminator)
 */
size_t strlen(const char* str);

/**
 * Copy string
 * @param dest  Destination buffer
 * @param src   Source string
 * @return      Pointer to dest
 */
char* strcpy(char* dest, const char* src);

/**
 * Copy string with length limit
 * @param dest  Destination buffer
 * @param src   Source string
 * @param n     Maximum number of characters to copy
 * @return      Pointer to dest
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Compare two strings
 * @param s1    First string
 * @param s2    Second string
 * @return      0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char* s1, const char* s2);

/**
 * Compare two strings with length limit
 * @param s1    First string
 * @param s2    Second string
 * @param n     Maximum number of characters to compare
 * @return      0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strncmp(const char* s1, const char* s2, size_t n);

#endif /* CHANUX_STRING_H */
