/**
 * @file string.h
 * @brief Memory manipulation function declarations for ChanUX kernel
 * 
 * This header file declares the standard memory manipulation functions
 * that are implemented in the kernel's string library. These functions
 * provide essential memory operations without depending on the standard
 * C library, which is necessary for freestanding kernel development.
 * 
 * @note These functions follow the C standard library conventions but
 *       are implemented independently for kernel use
 */

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/**
 * @brief Fill a block of memory with a specified value
 * 
 * Sets the first 'size' bytes of the memory block pointed to by 'bufptr'
 * to the specified value (interpreted as unsigned char).
 * 
 * @param bufptr Pointer to the memory block to fill
 * @param value Value to set (converted to unsigned char)
 * @param size Number of bytes to set
 * @return Pointer to the memory block (bufptr)
 * 
 * @see Common uses: Zeroing arrays, initializing buffers
 */
void* memset(void* bufptr, int value, size_t size);

/**
 * @brief Copy non-overlapping memory blocks
 * 
 * Copies 'size' bytes from the memory area pointed to by 'srcptr' to
 * the memory area pointed to by 'dstptr'. The memory areas must not overlap.
 * 
 * @param dstptr Destination buffer (restrict keyword ensures no aliasing)
 * @param srcptr Source buffer (restrict keyword ensures no aliasing)
 * @param size Number of bytes to copy
 * @return Pointer to destination buffer (dstptr)
 * 
 * @warning Undefined behavior if memory areas overlap - use memmove() instead
 * @note The 'restrict' keyword is a promise that the pointers don't alias
 */
void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size);

/**
 * @brief Copy memory blocks (handles overlapping regions)
 * 
 * Copies 'size' bytes from 'srcptr' to 'dstptr'. Unlike memcpy(), this
 * function correctly handles the case where the source and destination
 * memory regions overlap.
 * 
 * @param dstptr Destination buffer
 * @param srcptr Source buffer
 * @param size Number of bytes to copy
 * @return Pointer to destination buffer (dstptr)
 * 
 * @note Automatically determines the safe copy direction to handle overlaps
 */
void* memmove(void* dstptr, const void* srcptr, size_t size);

/**
 * @brief Compare two blocks of memory
 * 
 * Compares the first 'size' bytes of the memory areas 'ptr1' and 'ptr2'.
 * The comparison is performed byte by byte as unsigned char values.
 * 
 * @param ptr1 Pointer to first memory block
 * @param ptr2 Pointer to second memory block
 * @param size Number of bytes to compare
 * @return 0 if equal, <0 if ptr1 < ptr2, >0 if ptr1 > ptr2
 * 
 * @note Comparison stops at the first differing byte
 */
int memcmp(const void* ptr1, const void* ptr2, size_t size);

#endif  /* _STRING_H */