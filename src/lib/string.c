/**
 * @file string.c
 * @brief Memory manipulation functions for ChanUX kernel
 * 
 * This file implements basic memory operation functions that are essential
 * for kernel operations. These functions provide low-level memory manipulation
 * capabilities without relying on any external libraries.
 */

#include <stddef.h>

/**
 * @brief Fill a block of memory with a specified value
 * 
 * This function sets the first 'size' bytes of the memory area pointed to by
 * 'bufptr' to the specified 'value'. This is commonly used to initialize
 * memory regions or clear buffers.
 * 
 * @param bufptr Pointer to the memory block to fill
 * @param value The value to set (passed as int but converted to unsigned char)
 * @param size Number of bytes to set
 * @return Returns the original pointer 'bufptr'
 * 
 * @note The value is converted to unsigned char, so only the least significant
 *       byte of 'value' is used
 */
void* memset(void* bufptr, int value, size_t size) {
    /* Cast to unsigned char* for byte-by-byte access */
    unsigned char* buf = (unsigned char*) bufptr;
    
    /* Fill each byte with the specified value */
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    
    return bufptr;
}

/**
 * @brief Copy memory from source to destination (non-overlapping)
 * 
 * Copies 'size' bytes from memory area 'srcptr' to memory area 'dstptr'.
 * The memory areas must not overlap. For overlapping memory areas, use
 * memmove() instead.
 * 
 * @param dstptr Destination memory area (restrict indicates no aliasing)
 * @param srcptr Source memory area (restrict indicates no aliasing)
 * @param size Number of bytes to copy
 * @return Returns the original destination pointer 'dstptr'
 * 
 * @warning Behavior is undefined if memory areas overlap. Use memmove() for
 *          overlapping regions
 */
void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
    /* Cast pointers for byte-by-byte copying */
    unsigned char* dst = (unsigned char*) dstptr;
    const unsigned char* src = (const unsigned char*) srcptr;
    
    /* Copy each byte from source to destination */
    for (size_t i = 0; i < size; i++)
        dst[i] = src[i];
    
    return dstptr;
}

/**
 * @brief Copy memory from source to destination (handles overlapping)
 * 
 * Copies 'size' bytes from memory area 'srcptr' to memory area 'dstptr'.
 * Unlike memcpy(), this function correctly handles overlapping memory areas
 * by determining the copy direction based on the relative positions of the
 * source and destination.
 * 
 * @param dstptr Destination memory area
 * @param srcptr Source memory area
 * @param size Number of bytes to copy
 * @return Returns the original destination pointer 'dstptr'
 * 
 * @note When dst < src: Copy forward (low to high addresses)
 *       When dst > src: Copy backward (high to low addresses) to avoid
 *       overwriting source data before it's copied
 */
void* memmove(void* dstptr, const void* srcptr, size_t size) {
    unsigned char* dst = (unsigned char*) dstptr;
    const unsigned char* src = (const unsigned char*) srcptr;
    
    /* Determine copy direction based on memory layout */
    if (dst < src) {
        /* Forward copy: destination is before source, no overlap issue */
        for (size_t i = 0; i < size; i++)
            dst[i] = src[i];
    } else {
        /* Backward copy: destination is after source, copy from end to avoid
           overwriting source data that hasn't been copied yet */
        for (size_t i = size; i != 0; i--)
            dst[i-1] = src[i-1];
    }
    
    return dstptr;
}

/**
 * @brief Compare two memory areas byte by byte
 * 
 * Compares the first 'size' bytes of the memory areas pointed to by 'ptr1'
 * and 'ptr2'. The comparison is done using unsigned char values.
 * 
 * @param ptr1 Pointer to first memory area
 * @param ptr2 Pointer to second memory area
 * @param size Number of bytes to compare
 * @return Returns:
 *         - 0 if the memory areas are equal
 *         - Negative value if first differing byte in ptr1 is less than ptr2
 *         - Positive value if first differing byte in ptr1 is greater than ptr2
 * 
 * @note The sign of the return value is determined by the sign of the
 *       difference between the first pair of bytes that differ
 */
int memcmp(const void* ptr1, const void* ptr2, size_t size) {
    /* Cast to unsigned char* for byte comparison */
    const unsigned char* a = (const unsigned char*) ptr1;
    const unsigned char* b = (const unsigned char*) ptr2;
    
    /* Compare each byte until a difference is found or size is reached */
    for (size_t i = 0; i < size; i++) {
        if (a[i] < b[i])
            return -1;  /* First area is less than second */
        else if (b[i] < a[i])
            return 1;   /* First area is greater than second */
    }
    
    return 0;  /* All bytes are equal */
}