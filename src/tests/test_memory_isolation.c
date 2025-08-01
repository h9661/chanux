/*
 * Memory Isolation Test Suite
 * 
 * This file contains tests to verify proper kernel/user memory isolation.
 */

#include <stdint.h>
#include <stdbool.h>
#include "kernel/scheduler.h"
#include "kernel/vmm.h"
#include "kernel/memory_protection.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern void terminal_write_dec(uint32_t value);

/* Test results */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper function to report test result */
static void report_test(const char* test_name, bool passed) {
    terminal_writestring("[TEST] ");
    terminal_writestring(test_name);
    terminal_writestring(": ");
    if (passed) {
        terminal_writestring("PASSED\n");
        tests_passed++;
    } else {
        terminal_writestring("FAILED\n");
        tests_failed++;
    }
}

/* Test 1: Verify kernel pages are not user accessible */
void test_kernel_page_protection(void) {
    page_directory_t* pd = vmm_get_current_directory();
    uint32_t kernel_addr = 0x100000;  /* Kernel space address */
    
    /* Check if kernel page is marked as non-user */
    bool is_user_accessible = vmm_is_user_accessible(pd, kernel_addr);
    
    report_test("Kernel page protection", !is_user_accessible);
}

/* Test 2: Verify user space address validation */
void test_user_address_validation(void) {
    /* Test kernel addresses */
    bool kernel_addr_test = !is_user_address((void*)0x100000);
    report_test("Kernel address validation", kernel_addr_test);
    
    /* Test user addresses */
    bool user_addr_test = is_user_address((void*)0x10000000);
    report_test("User address validation", user_addr_test);
    
    /* Test boundary */
    bool boundary_test = !is_user_address((void*)0xC0000000);
    report_test("User space boundary", boundary_test);
}

/* Test 3: Verify user pointer validation */
void test_user_pointer_validation(void) {
    /* Test NULL pointer */
    bool null_test = !validate_user_pointer(NULL, 10, 0x1);
    report_test("NULL pointer validation", null_test);
    
    /* Test kernel pointer */
    void* kernel_ptr = (void*)0x100000;
    bool kernel_ptr_test = !validate_user_pointer(kernel_ptr, 10, 0x1);
    report_test("Kernel pointer validation", kernel_ptr_test);
    
    /* Test out of bounds */
    void* oob_ptr = (void*)0xFFFFFFF0;
    bool oob_test = !validate_user_pointer(oob_ptr, 32, 0x1);
    report_test("Out of bounds validation", oob_test);
}

/* Test 4: Test secure page mapping */
void test_secure_page_mapping(void) {
    page_directory_t* pd = vmm_get_current_directory();
    
    /* Try to map kernel page with user flag - should be rejected */
    terminal_writestring("[TEST] Attempting to map kernel page with USER flag...\n");
    vmm_map_page_secure(pd, 0x200000, 0x200000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    
    /* Verify it was not marked as user accessible */
    bool is_user = vmm_is_user_accessible(pd, 0x200000);
    report_test("Secure kernel mapping", !is_user);
    
    /* Map user page - should have USER flag */
    vmm_map_page_secure(pd, 0x10000000, 0x300000, PAGE_PRESENT | PAGE_WRITABLE);
    bool user_has_flag = vmm_is_user_accessible(pd, 0x10000000);
    report_test("Secure user mapping", user_has_flag);
}

/* Test 5: Test copy from/to user functions */
void test_user_copy_functions(void) {
    char kernel_buffer[16] = "Hello, World!";
    char test_buffer[16];
    
    /* Test copy from kernel address (should fail) */
    int result = copy_from_user(test_buffer, kernel_buffer, 16);
    report_test("Copy from kernel address", result == -1);
    
    /* Test copy to kernel address (should fail) */
    result = copy_to_user(kernel_buffer, test_buffer, 16);
    report_test("Copy to kernel address", result == -1);
    
    /* Test string copy from kernel (should fail) */
    result = strncpy_from_user(test_buffer, kernel_buffer, 16);
    report_test("String copy from kernel", result == -1);
}

/* User mode test process - will attempt illegal access */
void test_user_violation_process(void) {
    terminal_writestring("\n[User Test Process] Starting...\n");
    terminal_writestring("[User Test Process] Attempting to access kernel memory...\n");
    
    /* This should trigger a page fault and security violation */
    uint32_t* kernel_addr = (uint32_t*)0x100000;
    uint32_t value = *kernel_addr;  /* This should fail */
    
    /* Should not reach here */
    terminal_writestring("[User Test Process] ERROR: Read succeeded with value: ");
    terminal_write_hex(value);
    terminal_writestring("\n");
    terminal_writestring("[User Test Process] SECURITY BREACH!\n");
}

/* Main test runner */
void run_memory_isolation_tests(void) {
    terminal_writestring("\n=== Memory Isolation Test Suite ===\n\n");
    
    /* Run basic tests */
    test_kernel_page_protection();
    test_user_address_validation();
    test_user_pointer_validation();
    test_secure_page_mapping();
    test_user_copy_functions();
    
    /* Report results */
    terminal_writestring("\n=== Test Results ===\n");
    terminal_writestring("Tests passed: ");
    terminal_write_dec(tests_passed);
    terminal_writestring("\nTests failed: ");
    terminal_write_dec(tests_failed);
    terminal_writestring("\n\n");
    
    /* Run additional VMM USER flag test */
    extern void test_vmm_user_flag(void);
    test_vmm_user_flag();
}