/**
 * =============================================================================
 * Chanux OS - Type Definitions
 * =============================================================================
 * Standard type definitions for the kernel.
 * These provide consistent types across the codebase.
 * =============================================================================
 */

#ifndef CHANUX_TYPES_H
#define CHANUX_TYPES_H

/* =============================================================================
 * Fixed-width integer types
 * =============================================================================
 */

/* Unsigned integers */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

/* Signed integers */
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

/* =============================================================================
 * Size and pointer types
 * =============================================================================
 */

/* Size type (for memory sizes, array indices) */
typedef uint64_t    size_t;
typedef int64_t     ssize_t;

/* Pointer-sized integer types */
typedef uint64_t    uintptr_t;
typedef int64_t     intptr_t;

/* Pointer difference type */
typedef int64_t     ptrdiff_t;

/* =============================================================================
 * Boolean type
 * =============================================================================
 */

typedef _Bool       bool;
#define true        1
#define false       0

/* =============================================================================
 * Null pointer
 * =============================================================================
 */

#define NULL        ((void*)0)

/* =============================================================================
 * Address types (for clarity)
 * =============================================================================
 */

typedef uint64_t    phys_addr_t;    /* Physical address */
typedef uint64_t    virt_addr_t;    /* Virtual address */

/* =============================================================================
 * Kernel-specific types
 * =============================================================================
 */

typedef uint64_t    pid_t;          /* Process ID */
typedef uint64_t    tid_t;          /* Thread ID */

/* =============================================================================
 * Limits
 * =============================================================================
 */

#define UINT8_MAX   0xFF
#define UINT16_MAX  0xFFFF
#define UINT32_MAX  0xFFFFFFFF
#define UINT64_MAX  0xFFFFFFFFFFFFFFFFULL

#define INT8_MIN    (-128)
#define INT8_MAX    127
#define INT16_MIN   (-32768)
#define INT16_MAX   32767
#define INT32_MIN   (-2147483648)
#define INT32_MAX   2147483647
#define INT64_MIN   (-9223372036854775807LL - 1)
#define INT64_MAX   9223372036854775807LL

/* =============================================================================
 * Compiler attributes
 * =============================================================================
 */

#define PACKED          __attribute__((packed))
#define ALIGNED(x)      __attribute__((aligned(x)))
#define NORETURN        __attribute__((noreturn))
#define UNUSED          __attribute__((unused))
#define ALWAYS_INLINE   __attribute__((always_inline)) inline

#endif /* CHANUX_TYPES_H */
