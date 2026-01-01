/**
 * =============================================================================
 * Chanux OS - Debug Configuration Header
 * =============================================================================
 * Compile-time debug system with subsystem-specific flags.
 *
 * Usage:
 *   make DEBUG=1          - Enable all debug output
 *   make DEBUG_SCHED=1    - Enable only scheduler debug output
 *   make DEBUG=1 DEBUG_VMM=0 - Enable all except VMM debug
 *
 * In code:
 *   DBG_SCHED("[SCHED] scheduling...\n");
 *   DBG_VMM("[VMM-U] mapping page...\n");
 * =============================================================================
 */

#ifndef CHANUX_DEBUG_H
#define CHANUX_DEBUG_H

#include "kernel.h"

/* =============================================================================
 * Master Debug Flag
 * =============================================================================
 * When DEBUG is defined, all subsystem debug output is enabled by default.
 * Individual subsystems can be disabled with DEBUG_XXX=0.
 */

#ifdef DEBUG
    /* Master debug enabled - enable subsystems unless explicitly disabled */
    #ifndef DEBUG_SCHED
        #define DEBUG_SCHED 1
    #endif
    #ifndef DEBUG_VMM
        #define DEBUG_VMM 1
    #endif
    #ifndef DEBUG_USER
        #define DEBUG_USER 1
    #endif
    #ifndef DEBUG_SYSCALL
        #define DEBUG_SYSCALL 1
    #endif
#else
    /* Master debug disabled - disable all unless explicitly enabled */
    #ifndef DEBUG_SCHED
        #define DEBUG_SCHED 0
    #endif
    #ifndef DEBUG_VMM
        #define DEBUG_VMM 0
    #endif
    #ifndef DEBUG_USER
        #define DEBUG_USER 0
    #endif
    #ifndef DEBUG_SYSCALL
        #define DEBUG_SYSCALL 0
    #endif
#endif

/* =============================================================================
 * Subsystem Debug Macros
 * =============================================================================
 * Each macro expands to kprintf when enabled, or ((void)0) when disabled.
 * The ((void)0) form ensures the macro can be used as a statement.
 */

#if DEBUG_SCHED
    #define DBG_SCHED(fmt, ...) kprintf(fmt, ##__VA_ARGS__)
#else
    #define DBG_SCHED(fmt, ...) ((void)0)
#endif

#if DEBUG_VMM
    #define DBG_VMM(fmt, ...) kprintf(fmt, ##__VA_ARGS__)
#else
    #define DBG_VMM(fmt, ...) ((void)0)
#endif

#if DEBUG_USER
    #define DBG_USER(fmt, ...) kprintf(fmt, ##__VA_ARGS__)
#else
    #define DBG_USER(fmt, ...) ((void)0)
#endif

#if DEBUG_SYSCALL
    #define DBG_SYSCALL(fmt, ...) kprintf(fmt, ##__VA_ARGS__)
#else
    #define DBG_SYSCALL(fmt, ...) ((void)0)
#endif

#endif /* CHANUX_DEBUG_H */
