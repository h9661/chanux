/**
 * =============================================================================
 * Chanux OS - System Call Subsystem
 * =============================================================================
 * Implements system call initialization and dispatching using SYSCALL/SYSRET.
 *
 * The SYSCALL instruction provides a fast transition from user mode to kernel
 * mode without going through the interrupt descriptor table.
 *
 * MSR Configuration:
 *   - STAR: Segment selectors (kernel CS/SS in bits 32-47, user in bits 48-63)
 *   - LSTAR: 64-bit syscall entry point address
 *   - SFMASK: RFLAGS bits to clear on syscall (we clear IF)
 * =============================================================================
 */

#include "syscall/syscall.h"
#include "gdt.h"
#include "kernel.h"
#include "drivers/vga/vga.h"

/* =============================================================================
 * MSR Read/Write Helpers
 * =============================================================================
 */

/**
 * Read a Model-Specific Register.
 */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

/**
 * Write to a Model-Specific Register.
 */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

/* =============================================================================
 * Syscall Table
 * =============================================================================
 */

/* Forward declarations of syscall handlers */
static int64_t sys_exit_wrapper(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static int64_t sys_write_wrapper(uint64_t fd, uint64_t buf, uint64_t len, uint64_t, uint64_t, uint64_t);
static int64_t sys_read_wrapper(uint64_t fd, uint64_t buf, uint64_t len, uint64_t, uint64_t, uint64_t);
static int64_t sys_yield_wrapper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static int64_t sys_getpid_wrapper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static int64_t sys_sleep_wrapper(uint64_t ms, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* Syscall handler table */
static syscall_fn_t syscall_table[SYS_MAX] = {
    [SYS_EXIT]   = sys_exit_wrapper,
    [SYS_WRITE]  = sys_write_wrapper,
    [SYS_READ]   = sys_read_wrapper,
    [SYS_YIELD]  = sys_yield_wrapper,
    [SYS_GETPID] = sys_getpid_wrapper,
    [SYS_SLEEP]  = sys_sleep_wrapper,
};

/* Syscall names for debugging */
static const char* syscall_names[SYS_MAX] = {
    [SYS_EXIT]   = "exit",
    [SYS_WRITE]  = "write",
    [SYS_READ]   = "read",
    [SYS_YIELD]  = "yield",
    [SYS_GETPID] = "getpid",
    [SYS_SLEEP]  = "sleep",
};

/* =============================================================================
 * Syscall Initialization
 * =============================================================================
 */

/**
 * Initialize the system call subsystem.
 *
 * Configures the MSRs required for SYSCALL/SYSRET:
 *   - STAR: Segment selector bases
 *   - LSTAR: Syscall entry point
 *   - SFMASK: RFLAGS mask
 */
void syscall_init(void) {
    kprintf("syscall: Initializing system call interface...\n");

    /*
     * Enable SYSCALL/SYSRET by setting EFER.SCE (bit 0).
     * The bootloader already set EFER.LME and EFER.NXE.
     */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);
    kprintf("syscall: EFER.SCE enabled (EFER = 0x%x)\n", (uint32_t)efer);

    /*
     * STAR MSR (0xC0000081):
     *   Bits 31:0  - Reserved (EIP for 32-bit SYSCALL, not used in 64-bit)
     *   Bits 47:32 - Kernel CS (SYSCALL loads CS from here, SS = CS + 8)
     *   Bits 63:48 - User CS base (SYSRET loads CS from here + 16, SS from here + 8)
     *
     * For our GDT:
     *   Kernel CS = 0x08, Kernel SS = 0x10
     *   User Data = 0x28 (SYSRET SS = base + 8)
     *   User Code = 0x30 (SYSRET CS = base + 16)
     *
     * So: STAR[47:32] = 0x08 (kernel CS)
     *     STAR[63:48] = 0x20 (user base, so SS=0x28, CS=0x30)
     */
    uint64_t star = ((uint64_t)0x0020 << 48) |  /* User segment base */
                    ((uint64_t)0x0008 << 32);   /* Kernel code segment */
    wrmsr(MSR_STAR, star);
    kprintf("syscall: STAR MSR = 0x%016llX\n", star);

    /*
     * LSTAR MSR (0xC0000082):
     *   64-bit syscall entry point address
     */
    extern void syscall_entry(void);
    uint64_t lstar = (uint64_t)syscall_entry;
    wrmsr(MSR_LSTAR, lstar);
    kprintf("syscall: LSTAR MSR = 0x%016llX (syscall_entry)\n", lstar);

    /*
     * SFMASK MSR (0xC0000084):
     *   RFLAGS bits to clear on SYSCALL
     *   We clear IF (bit 9) to disable interrupts on entry
     */
    uint64_t sfmask = SYSCALL_RFLAGS_MASK;
    wrmsr(MSR_SFMASK, sfmask);
    kprintf("syscall: SFMASK MSR = 0x%016llX (clear IF)\n", sfmask);

    kprintf("syscall: System call interface initialized\n");
    kprintf("syscall: %d system calls registered\n", SYS_MAX);
}

/* =============================================================================
 * Syscall Dispatcher
 * =============================================================================
 */

/**
 * Dispatch a system call to the appropriate handler.
 *
 * Called from syscall_entry assembly after saving registers.
 *
 * @param num    Syscall number (from RAX)
 * @param arg1-5 Syscall arguments
 * @return       Return value (placed in RAX for user)
 */
int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    /* Validate syscall number */
    if (num >= SYS_MAX) {
        kprintf("syscall: Invalid syscall number %llu\n", num);
        return -ENOSYS;
    }

    /* Get handler from table */
    syscall_fn_t handler = syscall_table[num];
    if (!handler) {
        kprintf("syscall: No handler for syscall %llu (%s)\n",
                num, syscall_names[num]);
        return -ENOSYS;
    }

    /* Call the handler */
    int64_t result = handler(arg1, arg2, arg3, arg4, arg5, 0);

    return result;
}

/* =============================================================================
 * Syscall Handler Wrappers
 * =============================================================================
 * These wrap the actual syscall implementations to match the generic signature.
 */

/**
 * sys_exit wrapper - terminates current process
 */
static int64_t sys_exit_wrapper(uint64_t code, uint64_t arg2 UNUSED,
                                uint64_t arg3 UNUSED, uint64_t arg4 UNUSED,
                                uint64_t arg5 UNUSED, uint64_t arg6 UNUSED) {
    sys_exit((int)code);
    /* Never returns */
    return 0;
}

/**
 * sys_write wrapper - write to file descriptor
 */
static int64_t sys_write_wrapper(uint64_t fd, uint64_t buf, uint64_t len,
                                 uint64_t arg4 UNUSED, uint64_t arg5 UNUSED,
                                 uint64_t arg6 UNUSED) {
    return sys_write((int)fd, (const void*)buf, (size_t)len);
}

/**
 * sys_read wrapper - read from file descriptor
 */
static int64_t sys_read_wrapper(uint64_t fd, uint64_t buf, uint64_t len,
                                uint64_t arg4 UNUSED, uint64_t arg5 UNUSED,
                                uint64_t arg6 UNUSED) {
    return sys_read((int)fd, (void*)buf, (size_t)len);
}

/**
 * sys_yield wrapper - yield CPU to other processes
 */
static int64_t sys_yield_wrapper(uint64_t arg1 UNUSED, uint64_t arg2 UNUSED,
                                 uint64_t arg3 UNUSED, uint64_t arg4 UNUSED,
                                 uint64_t arg5 UNUSED, uint64_t arg6 UNUSED) {
    return sys_yield();
}

/**
 * sys_getpid wrapper - get process ID
 */
static int64_t sys_getpid_wrapper(uint64_t arg1 UNUSED, uint64_t arg2 UNUSED,
                                  uint64_t arg3 UNUSED, uint64_t arg4 UNUSED,
                                  uint64_t arg5 UNUSED, uint64_t arg6 UNUSED) {
    return sys_getpid();
}

/**
 * sys_sleep wrapper - sleep for milliseconds
 */
static int64_t sys_sleep_wrapper(uint64_t ms, uint64_t arg2 UNUSED,
                                 uint64_t arg3 UNUSED, uint64_t arg4 UNUSED,
                                 uint64_t arg5 UNUSED, uint64_t arg6 UNUSED) {
    return sys_sleep(ms);
}
