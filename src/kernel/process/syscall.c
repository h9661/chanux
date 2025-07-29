/*
 * System Call Implementation
 * 
 * Handles system calls from user space applications
 */

#include "kernel/syscall.h"
#include "kernel/timer.h"
#include <stdint.h>

/* Terminal functions (from terminal.c) */
void terminal_writestring(const char* data);
void terminal_putchar(char c);

/* Process ID counter (simple implementation) */
static uint32_t next_pid = 1;

/* System call function table */
typedef int (*syscall_func_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* Forward declarations of system call implementations */
static int sys_exit_handler(uint32_t status, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
static int sys_write_handler(uint32_t fd, uint32_t buf, uint32_t count, uint32_t d, uint32_t e);
static int sys_read_handler(uint32_t fd, uint32_t buf, uint32_t count, uint32_t d, uint32_t e);
static int sys_open_handler(uint32_t filename, uint32_t flags, uint32_t mode, uint32_t d, uint32_t e);
static int sys_close_handler(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
static int sys_getpid_handler(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
static int sys_sleep_handler(uint32_t ms, uint32_t b, uint32_t c, uint32_t d, uint32_t e);

/* System call table */
static syscall_func_t syscall_table[MAX_SYSCALLS] = {
    0,                      /* 0: reserved */
    sys_exit_handler,       /* 1: exit */
    sys_write_handler,      /* 2: write */
    sys_read_handler,       /* 3: read */
    sys_open_handler,       /* 4: open */
    sys_close_handler,      /* 5: close */
    sys_getpid_handler,     /* 6: getpid */
    sys_sleep_handler       /* 7: sleep */
};

/* External assembly interrupt handler */
extern void isr128(void);

/* IDT functions from idt.c */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/*
 * syscall_init - Initialize the system call interface
 */
void syscall_init(void) {
    /* Install system call interrupt handler (int 0x80)
     * 0xEE = Present, DPL=3 (user callable), 32-bit interrupt gate
     */
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);
    
    terminal_writestring("System call interface initialized\n");
}

/*
 * syscall_handler - Main system call dispatcher
 * @regs: Register state from interrupt
 * 
 * Called from assembly when int 0x80 is executed
 * System call number is in EAX, parameters in EBX, ECX, EDX, ESI, EDI
 */
void syscall_handler(struct syscall_regs *regs) {
    /* Extract system call number and parameters */
    uint32_t syscall_num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    uint32_t arg4 = regs->esi;
    uint32_t arg5 = regs->edi;
    
    /* Validate system call number */
    if (syscall_num >= MAX_SYSCALLS || syscall_table[syscall_num] == 0) {
        regs->eax = -1;  /* Invalid system call */
        return;
    }
    
    /* Call the appropriate system call handler */
    int ret = syscall_table[syscall_num](arg1, arg2, arg3, arg4, arg5);
    
    /* Return value goes in EAX */
    regs->eax = ret;
}

/* System call implementations */

static int sys_exit_handler(uint32_t status, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)b; (void)c; (void)d; (void)e;  /* Unused parameters */
    
    terminal_writestring("\nProcess exited with status: ");
    
    /* Simple integer to string conversion */
    char buffer[12];
    int i = 11;
    buffer[i] = '\0';
    
    if (status == 0) {
        buffer[--i] = '0';
    } else {
        while (status > 0 && i > 0) {
            buffer[--i] = '0' + (status % 10);
            status /= 10;
        }
    }
    
    terminal_writestring(&buffer[i]);
    terminal_writestring("\n");
    
    /* In a real OS, we would terminate the process here */
    /* For now, just halt the system */
    __asm__ __volatile__ ("cli; hlt");
    
    return 0;  /* Never reached */
}

static int sys_write_handler(uint32_t fd, uint32_t buf, uint32_t count, uint32_t d, uint32_t e) {
    (void)d; (void)e;  /* Unused parameters */
    
    /* Only support stdout (fd=1) and stderr (fd=2) for now */
    if (fd != 1 && fd != 2) {
        return -1;  /* Invalid file descriptor */
    }
    
    const char *str = (const char *)buf;
    uint32_t written = 0;
    
    /* Write characters to terminal */
    while (written < count && str[written] != '\0') {
        terminal_putchar(str[written]);
        written++;
    }
    
    return written;
}

static int sys_read_handler(uint32_t fd, uint32_t buf, uint32_t count, uint32_t d, uint32_t e) {
    (void)d; (void)e;  /* Unused parameters */
    
    /* Only support stdin (fd=0) for now */
    if (fd != 0) {
        return -1;  /* Invalid file descriptor */
    }
    
    /* Keyboard input would be implemented here */
    /* For now, return 0 (no data available) */
    return 0;
}

static int sys_open_handler(uint32_t filename, uint32_t flags, uint32_t mode, uint32_t d, uint32_t e) {
    (void)filename; (void)flags; (void)mode; (void)d; (void)e;  /* Unused parameters */
    
    /* File system not implemented yet */
    return -1;
}

static int sys_close_handler(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)fd; (void)b; (void)c; (void)d; (void)e;  /* Unused parameters */
    
    /* File system not implemented yet */
    return -1;
}

static int sys_getpid_handler(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;  /* Unused parameters */
    
    /* Return a simple PID (would be from process structure in real OS) */
    return next_pid;
}

static int sys_sleep_handler(uint32_t ms, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)b; (void)c; (void)d; (void)e;  /* Unused parameters */
    
    /* Use the timer driver to sleep */
    timer_sleep(ms);
    
    return 0;
}

/* Public interface functions (for kernel use) */

int sys_exit(int status) {
    return sys_exit_handler(status, 0, 0, 0, 0);
}

int sys_write(int fd, const void *buf, size_t count) {
    return sys_write_handler(fd, (uint32_t)buf, count, 0, 0);
}

int sys_read(int fd, void *buf, size_t count) {
    return sys_read_handler(fd, (uint32_t)buf, count, 0, 0);
}

int sys_open(const char *filename, int flags, int mode) {
    return sys_open_handler((uint32_t)filename, flags, mode, 0, 0);
}

int sys_close(int fd) {
    return sys_close_handler(fd, 0, 0, 0, 0);
}

int sys_getpid(void) {
    return sys_getpid_handler(0, 0, 0, 0, 0);
}

int sys_sleep(uint32_t milliseconds) {
    return sys_sleep_handler(milliseconds, 0, 0, 0, 0);
}